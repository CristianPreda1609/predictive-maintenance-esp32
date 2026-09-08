#include "realtime.h"

#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <cmath>
#include <algorithm>

#include "model_cnn1d_int8.h"   // artefact generat: model INT8
#include "preprocessing.h"      // artefact generat: kInputMean / kInputScale

#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

#define DIAGNOSTIC_MODE 1   // 1 = print pe serial fereastra vs training ; 0 = curat (demo)

// =====================================================================
//  STARE INTERNA A MODULULUI
//  Totul in namespace anonim -> invizibil in afara acestui fisier.
//  Asta inlocuieste globalele 'extern' de dinainte si elimina riscul ca
//  alt .cpp sa atinga direct window_buffer / g_pred / scaler.
// =====================================================================
namespace {

Adafruit_MPU6050 mpu;

const tflite::Model*      model       = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor*             input       = nullptr;
TfLiteTensor*             output      = nullptr;

constexpr int kTensorArenaSize = 35 * 1024;
uint8_t tensor_arena[kTensorArenaSize] __attribute__((aligned(16)));

// Parametri de cuantizare cititi din model la boot.
bool  input_is_int8  = false; float input_scale  = 1.0f; int input_zero_point  = 0;
bool  output_is_int8 = false; float output_scale = 1.0f; int output_zero_point = 0;

struct SensorSample { float features[NUM_FEATURES]; };
QueueHandle_t     sensor_queue = nullptr;
SemaphoreHandle_t i2c_mutex    = nullptr;   // serializeaza accesul la bus-ul I2C
SemaphoreHandle_t state_mutex  = nullptr;   // protejeaza g_pred + scope_buf

// Fereastra de inferenta: atinsa DOAR de AITask -> nu necesita mutex.
float window_buffer[WINDOW_SIZE][NUM_FEATURES];
int   current_sample = 0;

// Stare publicata catre web (sub state_mutex).
struct PredState {
  char     cls[16]            = "...";
  float    conf               = 0.0f;
  float    probs[NUM_CLASSES] = {0};
  uint32_t infer_us           = 0;
  uint32_t count              = 0;
};
PredState g_pred;

// Ring buffer osciloscop (scris de SensorTask, citit de web; sub state_mutex).
float    scope_buf[SCOPE_N][NUM_FEATURES];
uint16_t scope_head = 0;

// PWM curent. int aliniat pe 32 biti -> scriere/citire atomica pe ESP32;
// de aceea 'volatile' fara mutex e suficient (un singur cuvant masina).
volatile int g_pwm = INITIAL_PWM;

// ---------------------------------------------------------------------
//  HELPERS
// ---------------------------------------------------------------------
void setMotorSpeed(int speed) {
  speed = std::max(0, std::min(255, speed));
  ledcWrite(PWM_CHANNEL, speed);
}

bool initMPU() {
  if (!mpu.begin()) return false;
  // 16G / 500deg / 21Hz: EXACT setarile cu care s-a colectat datasetul.
  // Orice abatere aici = shift fata de scaler -> predictii gresite mut.
  mpu.setAccelerometerRange(MPU6050_RANGE_16_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  return true;
}

void recoverI2C() {
  // Recuperare din freeze de bus: reinitializare completa I2C + MPU.
  Wire.end();
  delay(5);
  Wire.begin(MPU_SDA_PIN, MPU_SCL_PIN);
  Wire.setClock(100000);
  initMPU();
}

#if DIAGNOSTIC_MODE
void printDiagnostics() {
  Serial.println("--- DIAGNOSTIC: fereastra curenta vs training ---");
  for (int f = 0; f < NUM_FEATURES; f++) {
    float sum = 0.0f, sumsq = 0.0f;
    for (int i = 0; i < WINDOW_SIZE; i++) {
      float v = window_buffer[i][f]; sum += v; sumsq += v * v;
    }
    float m  = sum / WINDOW_SIZE;
    float sd = sqrtf(std::max(0.0f, sumsq / WINDOW_SIZE - m * m));
    float dm = m - kInputMean[f];
    bool shift = (f < 3) && (fabsf(dm) > 1.0f);   // doar pe accel are sens pragul
    Serial.printf("  %s obs=%8.3f train=%8.3f diff=%7.3f std=%6.3f%s\n",
                  feature_names[f], m, kInputMean[f], dm, sd, shift ? "  <-- SHIFT" : "");
  }
  Serial.println("-------------------------------------------------");
}
#endif

// =====================================================================
//  TASK 1 (Core 1): achizitie senzor EXACT la 100Hz
//  vTaskDelayUntil pastreaza rata fixa (nu drift, indiferent de jitter).
// =====================================================================
void SensorTask(void* pv) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(10);  // 100 Hz
  float last_ax = 1e9f; int stale = 0;

  for (;;) {
    vTaskDelayUntil(&xLastWakeTime, xFrequency);

    sensors_event_t a, g, temp;
    bool ok = false;
    if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE) {
      ok = mpu.getEvent(&a, &g, &temp);
      xSemaphoreGive(i2c_mutex);
    }

    // Detectie senzor inghetat: aceeasi valoare ax de >20 cicluri = bus mort.
    if (a.acceleration.x == last_ax) stale++; else stale = 0;
    last_ax = a.acceleration.x;

    if (!ok || stale > 20) {
      Serial.println("[WARN] Senzor inghetat -> reinit I2C/MPU");
      if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE) { recoverI2C(); xSemaphoreGive(i2c_mutex); }
      stale = 0; continue;
    }

    SensorSample s;
    s.features[0] = a.acceleration.x; s.features[1] = a.acceleration.y; s.features[2] = a.acceleration.z;
    s.features[3] = g.gyro.x;         s.features[4] = g.gyro.y;         s.features[5] = g.gyro.z;
    xQueueSend(sensor_queue, &s, 0);   // non-blocant: daca s-ar umple, pierdem 1 sample, nu blocam 100Hz

    // Alimenteaza osciloscopul (toate 6 canale). Mutex cu timeout 0:
    // daca web-ul tine state_mutex chiar acum, sarim afisajul, nu achizitia.
    if (xSemaphoreTake(state_mutex, 0) == pdTRUE) {
      for (int f = 0; f < NUM_FEATURES; ++f) scope_buf[scope_head][f] = s.features[f];
      scope_head = (scope_head + 1) % SCOPE_N;
      xSemaphoreGive(state_mutex);
    }
  }
}

// =====================================================================
//  TASK 2 (Core 1): inferenta CNN 1D
//  ----  MATH IDENTICA CU main.cpp care merge. NU modifica aici.  ----
// =====================================================================
void AITask(void* pv) {
  for (;;) {
    SensorSample s;
    if (xQueueReceive(sensor_queue, &s, portMAX_DELAY) != pdTRUE) continue;

    for (int f = 0; f < NUM_FEATURES; ++f) window_buffer[current_sample][f] = s.features[f];
    current_sample++;
    if (current_sample < WINDOW_SIZE) continue;

#if DIAGNOSTIC_MODE
    printDiagnostics();
#endif

    // --- Filtru repaus pe giroscop ---
    // Daca toate axele gyro stau sub prag in toata fereastra, e Repaus sigur
    // si scurtcircuitam Invoke (mai stabil decat sa lasam CNN sa decida).
    bool is_repaus = true;
    const float repaus_threshold = 0.04f;
    for (int f = 3; f < 6; f++) {
      float mn = window_buffer[0][f], mx = window_buffer[0][f];
      for (int i = 1; i < WINDOW_SIZE; i++) { mn = std::min(mn, window_buffer[i][f]); mx = std::max(mx, window_buffer[i][f]); }
      if ((mx - mn) > repaus_threshold) { is_repaus = false; break; }
    }

    int      best     = 0;
    float    conf     = 100.0f;
    float    probs[NUM_CLASSES] = {0};
    uint32_t infer_us = 0;

    if (is_repaus) {
      probs[0] = 1.0f;   // Repaus pe filtru, fara Invoke
    } else {
      // Preprocesare: standardizare (x-mean)/scale, apoi cuantizare INT8.
      for (int i = 0; i < WINDOW_SIZE; ++i)
        for (int f = 0; f < NUM_FEATURES; ++f) {
          float norm = (window_buffer[i][f] - kInputMean[f]) / kInputScale[f];
          if (input_is_int8) {
            long q = lroundf(norm / input_scale) + input_zero_point;
            input->data.int8[i * NUM_FEATURES + f] = (int8_t)std::max(-128L, std::min(127L, q));
          } else {
            input->data.f[i * NUM_FEATURES + f] = norm;
          }
        }

      uint32_t t0 = micros();
      TfLiteStatus st = interpreter->Invoke();
      infer_us = micros() - t0;

      if (st == kTfLiteOk) {
        float mx = -1.0f;
        for (int i = 0; i < NUM_CLASSES; i++) {
          float v = output_is_int8 ? (output->data.int8[i] - output_zero_point) * output_scale
                                   : output->data.f[i];
          probs[i] = std::max(0.0f, v);          // pastram toate 5 pt barele din web
          if (v > mx) { mx = v; best = i; }
        }
        conf = mx * 100.0f;
      } else {
        Serial.println("[ERR] Invoke esuat");
      }
    }

    Serial.printf("-> %-13s %5.1f%%  (PWM %d, %lu us)\n\n",
                  class_names[best], conf, g_pwm, (unsigned long)infer_us);

    // Publica starea pentru web (atomic, sub state_mutex).
    if (xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE) {
      strncpy(g_pred.cls, class_names[best], sizeof(g_pred.cls) - 1);
      g_pred.cls[sizeof(g_pred.cls) - 1] = '\0';
      g_pred.conf = conf;
      for (int i = 0; i < NUM_CLASSES; ++i) g_pred.probs[i] = probs[i];
      g_pred.infer_us = infer_us;
      g_pred.count++;
      xSemaphoreGive(state_mutex);
    }

    // Sliding window: pastram ultimele (WINDOW-STEP) esantioane pt overlap.
    for (int i = 0; i < STEP_SIZE; ++i)
      for (int f = 0; f < NUM_FEATURES; ++f)
        window_buffer[i][f] = window_buffer[WINDOW_SIZE - STEP_SIZE + i][f];
    current_sample = STEP_SIZE;
  }
}

}  // namespace anonim

// =====================================================================
//  API PUBLIC
// =====================================================================
bool realtime_init() {
  // PWM
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RES);
  ledcAttachPin(MOTOR_PIN, PWM_CHANNEL);
  setMotorSpeed(g_pwm);

  // I2C + MPU
  Wire.begin(MPU_SDA_PIN, MPU_SCL_PIN);
  Wire.setClock(100000);
  if (!initMPU()) { Serial.println("EROARE: MPU6050 negasit!"); return false; }

  // TFLite micro
  static tflite::MicroErrorReporter rep;
  model = tflite::GetModel(cnn1d_int8_model);
  if (model->version() != TFLITE_SCHEMA_VERSION) { Serial.println("EROARE: schema TFLite!"); return false; }
  static tflite::AllOpsResolver resolver;
  static tflite::MicroInterpreter si(model, resolver, tensor_arena, kTensorArenaSize, &rep);
  interpreter = &si;
  if (interpreter->AllocateTensors() != kTfLiteOk) { Serial.println("EROARE alocare tensori!"); return false; }

  input  = interpreter->input(0);  output = interpreter->output(0);
  input_is_int8 = (input->type == kTfLiteInt8);
  input_scale = input->params.scale; input_zero_point = input->params.zero_point;
  output_is_int8 = (output->type == kTfLiteInt8);
  output_scale = output->params.scale; output_zero_point = output->params.zero_point;

  // SANITY CHECK la boot: verifici cu ochiul ca scaler-ul si ordinea claselor
  // sunt cele asteptate.
  Serial.print("kInputMean[0..5] =");
  for (int f = 0; f < NUM_FEATURES; ++f) Serial.printf(" %.4f", kInputMean[f]);
  Serial.print("\nClase: ");
  for (int c = 0; c < NUM_CLASSES; ++c) Serial.printf("%d=%s ", c, class_names[c]);
  Serial.println();

  // Obiecte FreeRTOS
  i2c_mutex    = xSemaphoreCreateMutex();
  state_mutex  = xSemaphoreCreateMutex();
  sensor_queue = xQueueCreate(256, sizeof(SensorSample));  // ~2.5s tampon @100Hz
  if (!i2c_mutex || !state_mutex || !sensor_queue) { Serial.println("EROARE obiecte FreeRTOS!"); return false; }

  for (int i = 0; i < SCOPE_N; ++i)
    for (int f = 0; f < NUM_FEATURES; ++f) scope_buf[i][f] = 0.0f;

  return true;
}

void realtime_start_tasks() {
  // Senzor + CNN pe Core 1 (real-time). Web ramane pe Core 0 
  xTaskCreatePinnedToCore(SensorTask, "Sensor", 4096,  nullptr, 3, nullptr, 1);
  xTaskCreatePinnedToCore(AITask,     "AI",     16384, nullptr, 2, nullptr, 1);
}

bool realtime_snapshot(StateSnapshot* out, TickType_t wait) {
  if (xSemaphoreTake(state_mutex, wait) != pdTRUE) return false;

  strncpy(out->cls, g_pred.cls, sizeof(out->cls)); out->cls[sizeof(out->cls) - 1] = '\0';
  out->conf     = g_pred.conf;
  out->infer_us = g_pred.infer_us;
  out->count    = g_pred.count;
  for (int i = 0; i < NUM_CLASSES; ++i) out->probs[i] = g_pred.probs[i];

  // Reordoneaza ring-ul in ordine cronologica AICI, ca web-ul sa doar
  // serializeze (separa logica de prezentare de cea de stocare).
  uint16_t head = scope_head;
  for (int k = 0; k < SCOPE_N; ++k)
    for (int f = 0; f < NUM_FEATURES; ++f)
      out->scope[k][f] = scope_buf[(head + k) % SCOPE_N][f];

  xSemaphoreGive(state_mutex);

  out->pwm = g_pwm;   // citit dupa mutex; int atomic, nu are nevoie de protectie
  return true;
}

bool realtime_set_pwm(int v) {
  // Whitelist: doar viteze din distributia de training. Restul ar produce
  // un regim pe care modelul nu l-a vazut niciodata.
  for (int i = 0; i < kNumAllowedPwm; ++i)
    if (kAllowedPwm[i] == v) { g_pwm = v; setMotorSpeed(v); return true; }
  return false;
}

int realtime_get_pwm() { return g_pwm; }