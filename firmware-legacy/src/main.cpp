// =====================================================================
//  Mentenanta Predictiva - CNN1D pe ESP32
//  Scris de la zero conform pipeline-ului de training:
//    - ordinea features:  ax, ay, az, gx, gy, gz   (FEATURE_COLUMNS)
//    - unitati:           m/s^2 si rad/s            (Adafruit getEvent, ca la colectare)
//    - senzor:            16G, 500deg, BAND_21_HZ   (identic cu main-ul de colectare)
//    - sampling:          100 Hz uniform            (delay(10) la colectare -> aici vTaskDelayUntil)
//    - fereastra:         128, pas 64
//    - standardizare:     (x - kInputMean) / kInputScale   (StandardScaler din Python)
//    - cuantizare:        norm / input_scale + zero_point  (INT8)
//    - clase (LABEL_MAP): 0=Repaus 1=Normal 2=Dezechilibru 3=Frecare 4=Slabit
// =====================================================================

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <cmath>
#include <algorithm>

#include "model_cnn1d_int8.h"
#include "preprocessing.h"

#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

// ---- Pune 1 ca sa vezi distributia live vs training; 0 pentru rulare normala ----
#define DIAGNOSTIC_MODE 1

// =====================================================================
//  CONFIGURARI
// =====================================================================
#define MPU_SDA_PIN 21
#define MPU_SCL_PIN 22
#define MOTOR_PIN   14

const int pwmFreq       = 5000;
const int pwmChannel    = 0;
const int pwmResolution = 8;
const int TARGET_PWM    = 200;  // in range-ul de training (160-255); mai mic = mai putin EMI pe I2C

const int WINDOW_SIZE  = 128;
const int STEP_SIZE    = 64;
const int NUM_FEATURES = 6;
const int NUM_CLASSES  = 5;

const char* class_names[]  = {"Repaus", "Normal", "Dezechilibru", "Frecare", "Slabit"};
const char* feature_names[] = {"ax", "ay", "az", "gx", "gy", "gz"};

// =====================================================================
//  OBIECTE GLOBALE
// =====================================================================
Adafruit_MPU6050 mpu;
const tflite::Model*       model       = nullptr;
tflite::MicroInterpreter*  interpreter = nullptr;
TfLiteTensor*              input       = nullptr;
TfLiteTensor*              output      = nullptr;

const int kTensorArenaSize = 35 * 1024;
uint8_t tensor_arena[kTensorArenaSize] __attribute__((aligned(16)));

bool  input_is_int8  = false; float input_scale  = 1.0f; int input_zero_point  = 0;
bool  output_is_int8 = false; float output_scale = 1.0f; int output_zero_point = 0;

typedef struct { float features[NUM_FEATURES]; } SensorSample;
QueueHandle_t     sensor_queue;
SemaphoreHandle_t i2c_mutex;

float window_buffer[WINDOW_SIZE][NUM_FEATURES];  // atins doar de AITask
int   current_sample = 0;

// =====================================================================
//  HELPERS
// =====================================================================
void setMotorSpeed(int speed) {
  speed = std::max(0, std::min(255, speed));
  ledcWrite(pwmChannel, speed);
}

bool initMPU() {
  if (!mpu.begin()) return false;
  mpu.setAccelerometerRange(MPU6050_RANGE_16_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  return true;
}

// Recuperare automata dupa un I2C inghetat de EMI-ul motorului (inlocuieste reset-ul manual)
void recoverI2C() {
  Wire.end();
  delay(5);
  Wire.begin(MPU_SDA_PIN, MPU_SCL_PIN);
  Wire.setClock(100000);
  initMPU();
}

#if DIAGNOSTIC_MODE
// Compara media/deviatia ferestrei curente cu statisticile de training.
void printDiagnostics() {
  Serial.println("--- DIAGNOSTIC: fereastra curenta vs training ---");
  bool orientation_shift = false;

  for (int f = 0; f < NUM_FEATURES; f++) {
    float sum = 0.0f, sumsq = 0.0f;
    for (int i = 0; i < WINDOW_SIZE; i++) {
      float v = window_buffer[i][f];
      sum += v; sumsq += v * v;
    }
    float m  = sum / WINDOW_SIZE;
    float sd = sqrtf(std::max(0.0f, sumsq / WINDOW_SIZE - m * m));
    float dm = m - kInputMean[f];

    // Doar pe accelerometru (0,1,2) media depinde de orientarea senzorului.
    bool axis_shift = (f < 3) && (fabsf(dm) > 1.0f);
    if (axis_shift) orientation_shift = true;

    // NOTA: kInputScale e std-ul GLOBAL (toate clasele). std-ul pe o
    // singura fereastra e normal sa fie mai mic - nu il interpreta ca shift.
    Serial.printf("  %s  obs_mean=%8.3f  train_mean=%8.3f  diff=%7.3f | obs_std=%6.3f  (global_std=%6.3f)%s\n",
                  feature_names[f], m, kInputMean[f], dm, sd, kInputScale[f],
                  axis_shift ? "   <-- SHIFT" : "");
  }

  if (orientation_shift)
    Serial.println("  VERDICT: senzorul e montat/orientat ALTFEL decat la colectare.\n"
                   "           Fix: remonteaza identic, SAU re-calculeaza StandardScaler pe date noi.");
  else
    Serial.println("  VERDICT: orientarea e OK. Daca clasele tot se confunda, e shift de\n"
                   "           PATTERN de la motorul nou (nu apare in medie) -> re-antreneaza.");
  Serial.println("-------------------------------------------------");
}
#endif

// =====================================================================
//  TASK 1 (Core 1): achizitie senzor EXACT la 100Hz, decuplat de inferenta
// =====================================================================
void SensorTask(void* pv) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(10);  // 100 Hz, ca la colectare

  float last_ax = 1e9f;
  int   stale   = 0;

  for (;;) {
    vTaskDelayUntil(&xLastWakeTime, xFrequency);

    sensors_event_t a, g, temp;
    bool ok = false;
    if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE) {
      ok = mpu.getEvent(&a, &g, &temp);
      xSemaphoreGive(i2c_mutex);
    }

    // Senzor viu = zgomot pe ultimul bit. Citiri EXACT identice = bus inghetat.
    if (a.acceleration.x == last_ax) stale++; else stale = 0;
    last_ax = a.acceleration.x;

    if (!ok || stale > 20) {  // ~200ms de citiri moarte
      Serial.println("[WARN] Senzor inghetat -> reinit I2C/MPU");
      if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE) {
        recoverI2C();
        xSemaphoreGive(i2c_mutex);
      }
      stale = 0;
      continue;
    }

    SensorSample s;
    s.features[0] = a.acceleration.x;
    s.features[1] = a.acceleration.y;
    s.features[2] = a.acceleration.z;
    s.features[3] = g.gyro.x;
    s.features[4] = g.gyro.y;
    s.features[5] = g.gyro.z;
    xQueueSend(sensor_queue, &s, 0);
  }
}

// =====================================================================
//  TASK 2 (Core 1): inferenta CNN 1D
// =====================================================================
void AITask(void* pv) {
  for (;;) {
    SensorSample s;
    if (xQueueReceive(sensor_queue, &s, portMAX_DELAY) != pdTRUE) continue;

    for (int f = 0; f < NUM_FEATURES; ++f)
      window_buffer[current_sample][f] = s.features[f];
    current_sample++;

    if (current_sample < WINDOW_SIZE) continue;

#if DIAGNOSTIC_MODE
    printDiagnostics();
#endif

    // --- Filtru repaus pe giroscop ---
    bool is_repaus = true;
    const float repaus_threshold = 0.04f;
    for (int f = 3; f < 6; f++) {
      float mn = window_buffer[0][f], mx = window_buffer[0][f];
      for (int i = 1; i < WINDOW_SIZE; i++) {
        mn = std::min(mn, window_buffer[i][f]);
        mx = std::max(mx, window_buffer[i][f]);
      }
      if ((mx - mn) > repaus_threshold) { is_repaus = false; break; }
    }

    int   best = 0;
    float conf = 100.0f;

    if (!is_repaus) {
      for (int i = 0; i < WINDOW_SIZE; ++i)
        for (int f = 0; f < NUM_FEATURES; ++f) {
          float norm = (window_buffer[i][f] - kInputMean[f]) / kInputScale[f];
          if (input_is_int8) {
            long q = lroundf(norm / input_scale) + input_zero_point;
            input->data.int8[i * NUM_FEATURES + f] =
                (int8_t)std::max(-128L, std::min(127L, q));
          } else {
            input->data.f[i * NUM_FEATURES + f] = norm;
          }
        }

      if (interpreter->Invoke() == kTfLiteOk) {
        float mx = -1.0f;
        for (int i = 0; i < NUM_CLASSES; i++) {
          float v = output_is_int8
                      ? (output->data.int8[i] - output_zero_point) * output_scale
                      : output->data.f[i];
          if (v > mx) { mx = v; best = i; }
        }
        conf = mx * 100.0f;
      } else {
        Serial.println("[ERR] Invoke esuat");
      }
    }

    Serial.printf("-> %-13s %5.1f%%  (PWM %d)\n\n", class_names[best], conf, TARGET_PWM);

    for (int i = 0; i < STEP_SIZE; ++i)
      for (int f = 0; f < NUM_FEATURES; ++f)
        window_buffer[i][f] = window_buffer[WINDOW_SIZE - STEP_SIZE + i][f];
    current_sample = STEP_SIZE;
  }
}

// =====================================================================
//  SETUP
// =====================================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n--- CNN1D Mentenanta Predictiva (rescris de la zero) ---");
#if DIAGNOSTIC_MODE
  Serial.println("MOD DIAGNOSTIC ACTIV (pune DIAGNOSTIC_MODE 0 pentru rulare curata)\n");
#endif

  ledcSetup(pwmChannel, pwmFreq, pwmResolution);
  ledcAttachPin(MOTOR_PIN, pwmChannel);
  setMotorSpeed(TARGET_PWM);

  Wire.begin(MPU_SDA_PIN, MPU_SCL_PIN);
  Wire.setClock(100000);
  if (!initMPU()) {
    Serial.println("EROARE: MPU6050 negasit pe pinii 21/22!");
    while (1) delay(10);
  }

  static tflite::MicroErrorReporter rep;
  model = tflite::GetModel(cnn1d_int8_model);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println("EROARE: versiune schema TFLite incompatibila!");
    while (1) delay(10);
  }
  static tflite::AllOpsResolver resolver;
  static tflite::MicroInterpreter si(model, resolver, tensor_arena, kTensorArenaSize, &rep);
  interpreter = &si;
  if (interpreter->AllocateTensors() != kTfLiteOk) {
    Serial.println("EROARE alocare tensori!");
    while (1) delay(10);
  }

  input  = interpreter->input(0);
  output = interpreter->output(0);
  input_is_int8     = (input->type == kTfLiteInt8);
  input_scale       = input->params.scale;
  input_zero_point  = input->params.zero_point;
  output_is_int8    = (output->type == kTfLiteInt8);
  output_scale      = output->params.scale;
  output_zero_point = output->params.zero_point;

  Serial.printf("Input: %s  scale=%.6f zp=%d | Output: %s scale=%.6f zp=%d\n",
                input_is_int8 ? "int8" : "float", input_scale, input_zero_point,
                output_is_int8 ? "int8" : "float", output_scale, output_zero_point);

  i2c_mutex    = xSemaphoreCreateMutex();
  sensor_queue = xQueueCreate(256, sizeof(SensorSample));
  if (!i2c_mutex || !sensor_queue) {
    Serial.println("EROARE creare obiecte FreeRTOS!");
    while (1) delay(10);
  }

  xTaskCreatePinnedToCore(SensorTask, "Sensor",  4096, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(AITask,     "AI",     16384, NULL, 2, NULL, 1);

  Serial.println("Pornit: 100Hz decuplat de inferenta + recuperare I2C automata.\n");
}

void loop() {
  vTaskDelete(NULL);
}