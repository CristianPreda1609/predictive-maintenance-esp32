#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <cmath>

#include "model_cnn1d_int8.h"
#include "preprocessing.h" 

#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

// ==========================================
// CONFIGURARI PINI & HARDWARE
// ==========================================
#define MPU_SDA_PIN 21
#define MPU_SCL_PIN 22
#define MOTOR_PIN   14

// Setari PWM pentru ESP32
const int pwmFreq = 5000;
const int pwmChannel = 0;
const int pwmResolution = 8; 

// ==========================================
// CONFIGURARI MODEL
// ==========================================
const int WINDOW_SIZE = 128;   
const int STEP_SIZE   = 64;    
const int NUM_FEATURES = 6;  // Modificat strict la 6 (fara temperatura)
const int NUM_CLASSES = 5;

int current_pwm_value = 0; 

// Obiecte Globale
Adafruit_MPU6050 mpu;
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;
TfLiteTensor* output = nullptr;
tflite::MicroErrorReporter* error_reporter = nullptr;

// 30 KB este arhisuficient pentru modelul nostru de 11.5 KB
const int kTensorArenaSize = 30 * 1024;
uint8_t tensor_arena[kTensorArenaSize] __attribute__((aligned(16)));

float window_buffer[WINDOW_SIZE][NUM_FEATURES];
int current_sample = 0;

bool input_is_int8 = false;
float input_scale = 1.0f;
int input_zero_point = 0;

bool output_is_int8 = false;
float output_scale = 1.0f;
int output_zero_point = 0;

void setMotorSpeed(int speed) {
  if (speed > 255) speed = 255;
  if (speed < 0) speed = 0;

  ledcWrite(pwmChannel, speed);
  current_pwm_value = speed;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  Serial.println("--- Start TensorFlow Lite ESP32 (Mentenanta Predictiva) ---");

  // 1. CONFIGURARE PWM
  ledcSetup(pwmChannel, pwmFreq, pwmResolution);
  ledcAttachPin(MOTOR_PIN, pwmChannel);
  
  Serial.println("Pornire motor la PWM 255...");
  setMotorSpeed(255); 

  // 2. INITIALIZARE I2C
  Wire.begin(MPU_SDA_PIN, MPU_SCL_PIN);
  if (!mpu.begin()) {
    Serial.println("EROARE: MPU6050 nu raspunde pe pinii 21/22!");
    while (1) delay(10);
  }
  
  mpu.setAccelerometerRange(MPU6050_RANGE_16_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // 3. INITIALIZARE TENSORFLOW
  static tflite::MicroErrorReporter micro_error_reporter;
  error_reporter = &micro_error_reporter;

  model = tflite::GetModel(cnn1d_int8_model);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    TF_LITE_REPORT_ERROR(error_reporter, "Eroare versiune schema TFLite!");
    return;
  }

  static tflite::AllOpsResolver resolver;
  static tflite::MicroInterpreter static_interpreter(
      model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
  interpreter = &static_interpreter;

  if (interpreter->AllocateTensors() != kTfLiteOk) {
    TF_LITE_REPORT_ERROR(error_reporter, "Eroare Alocare Memorie!");
    return;
  }

  input = interpreter->input(0);
  output = interpreter->output(0);
  input_is_int8 = (input->type == kTfLiteInt8);
  input_scale = input->params.scale;
  input_zero_point = input->params.zero_point;
  output_is_int8 = (output->type == kTfLiteInt8);
  output_scale = output->params.scale;
  output_zero_point = output->params.zero_point;

  // Diagnosticare memorie
  if (interpreter->Invoke() != kTfLiteOk) {
    Serial.println("Atentie: Invoke de test a esuat.");
  }

  size_t used_bytes = interpreter->arena_used_bytes();
  Serial.println("\n----------------------------------------");
  Serial.print("Tensor Arena Alocata: "); Serial.print(kTensorArenaSize); Serial.println(" bytes");
  Serial.print("NECESAR REAL (Minim): "); Serial.print(used_bytes); Serial.println(" bytes");
  Serial.println("----------------------------------------\n");
  Serial.println("Sistem initializat complet!");
}

void loop() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Citim strict cele 6 canale cinematice
  float raw_data[NUM_FEATURES];
  raw_data[0] = a.acceleration.x;
  raw_data[1] = a.acceleration.y;
  raw_data[2] = a.acceleration.z;
  raw_data[3] = g.gyro.x;
  raw_data[4] = g.gyro.y;
  raw_data[5] = g.gyro.z;

  // Adaugam direct in fereastra curenta
  for (int f = 0; f < NUM_FEATURES; ++f) {
    window_buffer[current_sample][f] = raw_data[f];
  }
  current_sample++;

  if (current_sample >= WINDOW_SIZE) {
    
    // ==========================================
    // 1. FILTRUL HIBRID (DETECTIE REPAUS ABSOLUT PE GIROSCOP)
    // ==========================================
    bool is_repaus = true;
    float repaus_threshold = 0.15; // Prag de zgomot pentru giroscop (rad/s)

    // Verificam STRICT axele giroscopului (f = 3, 4, 5) pentru ca ele ignora gravitatia
    for (int f = 3; f < 6; f++) {
      float min_val = window_buffer[0][f];
      float max_val = window_buffer[0][f];
      for (int i = 1; i < WINDOW_SIZE; i++) {
        if (window_buffer[i][f] < min_val) min_val = window_buffer[i][f];
        if (window_buffer[i][f] > max_val) max_val = window_buffer[i][f];
      }
      
      // Daca diferenta pe giroscop depaseste pragul, avem miscare dinamica
      if ((max_val - min_val) > repaus_threshold) {
        is_repaus = false;
        break;
      }
    }

    Serial.println("\n--- Rezultate Predictie ---");

    // Daca sta pe loc, afisam direct REPAUS
    if (is_repaus) {
      Serial.println("Clasa 0: 100.0% (Calcul Hardware Giroscop)");
      Serial.println("Clasa 1: 0.0%");
      Serial.println("Clasa 2: 0.0%");
      Serial.println("Clasa 3: 0.0%");
      Serial.println("Clasa 4: 0.0%");
      Serial.println("-> DETECTAT: Repaus");
      Serial.print("(Motor PWM curent: "); Serial.print(current_pwm_value); Serial.println(")");
      Serial.println("---------------------------");
    } 
    else {
      // ==========================================
      // 2. INFERENTA AI (CNN 1D) DACA EXISTA MISCARE
      // ==========================================
      
      // Standardizare + Cuantizare
      for (int i = 0; i < WINDOW_SIZE; ++i) {
        for (int f = 0; f < NUM_FEATURES; ++f) {
          float normalized = (window_buffer[i][f] - kInputMean[f]) / kInputScale[f];

          if (input_is_int8) {
            float scaled = normalized / input_scale;
            long quantized = lroundf(scaled) + input_zero_point;
            if (quantized > 127) quantized = 127;
            if (quantized < -128) quantized = -128;
            input->data.int8[i * NUM_FEATURES + f] = static_cast<int8_t>(quantized);
          } else {
            input->data.f[i * NUM_FEATURES + f] = normalized;
          }
        }
      }

      if (interpreter->Invoke() != kTfLiteOk) {
        Serial.println("Eroare Invoke!");
        return;
      }

      int best_class = -1;
      float max_val = -1.0f;

      for (int i = 0; i < NUM_CLASSES; i++) {
        float val = 0.0f;
        if (output_is_int8) {
          val = (output->data.int8[i] - output_zero_point) * output_scale;
        } else {
          val = output->data.f[i];
        }
        Serial.print("Clasa "); Serial.print(i); Serial.print(": ");
        Serial.print(val * 100.0, 1); Serial.println("%");

        if (val > max_val) {
          max_val = val;
          best_class = i;
        }
      }
      
      const char* class_names[] = {"Repaus", "Normal", "Dezechilibru", "Frecare", "Slabit"};
      Serial.print("-> DETECTAT: "); Serial.println(class_names[best_class]);
      Serial.print("(Motor PWM curent: "); Serial.print(current_pwm_value); Serial.println(")");
      Serial.println("---------------------------");
    }

    // ==========================================
    // 3. GLISAREA FERESTREI (SLICING) CU PASUL 64
    // ==========================================
    for (int i = 0; i < STEP_SIZE; ++i) {
      for (int f = 0; f < NUM_FEATURES; ++f) {
        window_buffer[i][f] = window_buffer[WINDOW_SIZE - STEP_SIZE + i][f];
      }
    }
    current_sample = STEP_SIZE;
  }

  delay(10);
}