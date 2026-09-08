#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <cmath>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "model_cnn1d_int8.h"
#include "preprocessing.h" 
#include "web_interface.h"

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

const int pwmFreq = 5000;
const int pwmChannel = 0;
const int pwmResolution = 8; 
volatile int current_pwm_value = 0; 

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
SemaphoreHandle_t oled_mutex;
// ==========================================
// CONFIGURARI MODEL
// ==========================================
const int WINDOW_SIZE = 128;   
const int STEP_SIZE   = 64;    
const int NUM_FEATURES = 6;  
const int NUM_CLASSES = 5;
const char* class_names[] = {"Repaus", "Normal", "Frecare", "Dezechilibru", "Slăbit"};

// Obiecte Globale Hardware & ML
Adafruit_MPU6050 mpu;
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;
TfLiteTensor* output = nullptr;
tflite::MicroErrorReporter* error_reporter = nullptr;

const int kTensorArenaSize = 35 * 1024; // Marit putin pentru siguranta cu FreeRTOS
uint8_t tensor_arena[kTensorArenaSize] __attribute__((aligned(16)));

float window_buffer[WINDOW_SIZE][NUM_FEATURES];
int current_sample = 0;

bool input_is_int8 = false; float input_scale = 1.0f; int input_zero_point = 0;
bool output_is_int8 = false; float output_scale = 1.0f; int output_zero_point = 0;

// ==========================================
// FREERTOS: DECUPLARE AI DE WIFI
// ==========================================
typedef struct {
  float features[NUM_FEATURES];
} SensorSample;

QueueHandle_t sensor_queue;
SemaphoreHandle_t telemetry_mutex;
SemaphoreHandle_t i2c_mutex; // Mutex pentru a proteja magistrala I2C

// Variabile si mutex pentru streaming-ul datelor de la senzor
SensorSample latest_sample;
SemaphoreHandle_t sensor_data_mutex;

// NOU: Variabile pentru AJAX Polling (Fallback robust pentru mobil)
// Initializam cu un JSON perfect valid pentru a preveni erorile la prima incarcare
char current_json_data[256] = "{\"label\":\"Asteptare...\",\"conf\":\"0.0\",\"pwm\":\"0\",\"uptime\":\"0\",\"g_mag\":\"0.0\",\"a_lin_vib\":\"0.0\"}";
SemaphoreHandle_t json_mutex;

// Variabile globale pentru telemetrie, protejate de mutex
char global_detected_class[32] = "Asteptare...";
float global_confidence = 0.0f;
bool new_prediction_ready = false;

// ==========================================
// WEB SERVER
// ==========================================
AsyncWebServer server(80);
DNSServer dnsServer;

void setMotorSpeed(int speed) {
  if (speed > 255) speed = 255;
  if (speed < 0) speed = 0;
  ledcWrite(pwmChannel, speed);
  current_pwm_value = speed;
}

// =================================================================
// TASK 1 (Core 1, Prioritate MAXIMĂ): Achiziție Date Senzor
// =================================================================
void SensorTask(void * pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(10); // 10ms = 100Hz

  for(;;) {
    vTaskDelayUntil(&xLastWakeTime, xFrequency);

    sensors_event_t a, g, temp;
    // Preluam lacatul I2C inainte de a folosi senzorul
    if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE) {
        mpu.getEvent(&a, &g, &temp);
        // Eliberam lacatul imediat dupa ce am terminat
        xSemaphoreGive(i2c_mutex);
    }
    
    SensorSample sample;
    sample.features[0] = a.acceleration.x;
    sample.features[1] = a.acceleration.y;
    sample.features[2] = a.acceleration.z;
    sample.features[3] = g.gyro.x;
    sample.features[4] = g.gyro.y;
    sample.features[5] = g.gyro.z;

    xQueueSend(sensor_queue, &sample, 0);

    if (xSemaphoreTake(sensor_data_mutex, (TickType_t) 5) == pdTRUE) {
        latest_sample = sample;
        xSemaphoreGive(sensor_data_mutex);
    }
  }
}

// =================================================================
// TASK 2 (Core 1, Prioritate Medie): Inferență AI
// =================================================================
void AITask(void * pvParameters) {
  for(;;) {
    SensorSample sample;
    if (xQueueReceive(sensor_queue, &sample, portMAX_DELAY) == pdTRUE) {
      
      for (int f = 0; f < NUM_FEATURES; ++f) {
        window_buffer[current_sample][f] = sample.features[f];
      }
      current_sample++;

      if (current_sample >= WINDOW_SIZE) {
        
        bool is_repaus = true;
        float repaus_threshold = 0.04;
        for (int f = 3; f < 6; f++) {
          float min_val = window_buffer[0][f], max_val = window_buffer[0][f];
          for (int i = 1; i < WINDOW_SIZE; i++) {
            if (window_buffer[i][f] < min_val) min_val = window_buffer[i][f];
            if (window_buffer[i][f] > max_val) max_val = window_buffer[i][f];
          }
          if ((max_val - min_val) > repaus_threshold) {
            is_repaus = false;
            break;
          }
        }

        String detected_class_str = "";
        float confidence = 0.0f;

        if (is_repaus) {
          detected_class_str = "Repaus";
          confidence = 100.0f;
        } else {
          for (int i = 0; i < WINDOW_SIZE; ++i) {
            for (int f = 0; f < NUM_FEATURES; ++f) {
              float normalized = (window_buffer[i][f] - kInputMean[f]) / kInputScale[f];
              if (input_is_int8) {
                long quantized = lroundf(normalized / input_scale) + input_zero_point;
                input->data.int8[i * NUM_FEATURES + f] = static_cast<int8_t>(std::max(-128L, std::min(127L, quantized)));
              } else {
                input->data.f[i * NUM_FEATURES + f] = normalized;
              }
            }
          }

          if (interpreter->Invoke() == kTfLiteOk) {
            int best_class_idx = -1;
            float max_val = -1.0f;
            for (int i = 0; i < NUM_CLASSES; i++) {
              float val = output_is_int8 ? (output->data.int8[i] - output_zero_point) * output_scale : output->data.f[i];
              if (val > max_val) {
                max_val = val;
                best_class_idx = i;
              }
            }
            detected_class_str = class_names[best_class_idx];
            confidence = max_val * 100.0f;
          }
        }
        
        Serial.println("-> DETECTAT: " + detected_class_str + " (" + String(confidence, 1) + "%)");

        xSemaphoreTake(telemetry_mutex, portMAX_DELAY);
        strncpy(global_detected_class, detected_class_str.c_str(), sizeof(global_detected_class)-1);
        global_confidence = confidence;
        new_prediction_ready = true;
        xSemaphoreGive(telemetry_mutex);

        for (int i = 0; i < STEP_SIZE; ++i) {
          for (int f = 0; f < NUM_FEATURES; ++f) {
            window_buffer[i][f] = window_buffer[WINDOW_SIZE - STEP_SIZE + i][f];
          }
        }
        current_sample = STEP_SIZE;
      }
    }
  }
}

// =================================================================
// TASK 3 (Core 0, Prioritate Scăzută): Telemetrie Web
// =================================================================
void TelemetryTask(void * pvParameters) {
  static char last_known_class[32] = "Asteptare...";
  static float last_known_conf = 0.0f;

  for(;;) {
    if (xSemaphoreTake(telemetry_mutex, (TickType_t) 5) == pdTRUE) {
      if (new_prediction_ready) {
        strncpy(last_known_class, global_detected_class, sizeof(last_known_class) - 1);
        last_known_conf = global_confidence;
        new_prediction_ready = false;
      }
      xSemaphoreGive(telemetry_mutex);
    }

    // STATIC si initializat cu 0 pentru a preveni valori "NaN" sau "Inf" daca mutex-ul rateaza la startup!
    static SensorSample local_sensor_sample = {0};
    if (xSemaphoreTake(sensor_data_mutex, (TickType_t) 5) == pdTRUE) {
        local_sensor_sample = latest_sample;
        xSemaphoreGive(sensor_data_mutex);
    }

    float ax = local_sensor_sample.features[0];
    float ay = local_sensor_sample.features[1];
    float az = local_sensor_sample.features[2];
    float gx = local_sensor_sample.features[3];
    float gy = local_sensor_sample.features[4];
    float gz = local_sensor_sample.features[5];

    float g_mag = sqrt(gx*gx + gy*gy + gz*gz);

    const float ACCEL_MAG_AT_REST = 10.69f;
    float a_mag = sqrt(ax*ax + ay*ay + az*az);
    float a_lin_vib = std::fabs(a_mag - ACCEL_MAG_AT_REST); // Folosim std::fabs corect pentru numere cu virgula

    char json_buffer[256];
    snprintf(json_buffer, sizeof(json_buffer),
             "{\"label\":\"%s\",\"conf\":\"%.1f\",\"pwm\":\"%d\",\"uptime\":\"%lu\",\"g_mag\":\"%.4f\",\"a_lin_vib\":\"%.4f\"}",
             last_known_class,
             last_known_conf,
             current_pwm_value,
             millis() / 1000,
             g_mag,
             a_lin_vib);
    
    // Salvam JSON-ul in siguranta pentru a fi cerut de browser (AJAX Polling)
    if (xSemaphoreTake(json_mutex, portMAX_DELAY) == pdTRUE) {
        strncpy(current_json_data, json_buffer, sizeof(current_json_data) - 1);
        xSemaphoreGive(json_mutex);
    }

    vTaskDelay(pdMS_TO_TICKS(50)); // Rulam la 20Hz pentru un grafic fluid
  }
}
void OLEDTask(void * pvParameters) {
    static char oled_class[32] = "Asteptare...";
    static float oled_conf = 0.0f;
    static int pred_count = 0;

    for(;;) {
        // Citim ultima predictie
        if (xSemaphoreTake(telemetry_mutex, (TickType_t)5) == pdTRUE) {
            if (new_prediction_ready) {
                strncpy(oled_class, global_detected_class, sizeof(oled_class)-1);
                oled_conf = global_confidence;
                pred_count++;
            }
            xSemaphoreGive(telemetry_mutex);
        }

        // Scriem pe display cu mutex pe I2C
        if (xSemaphoreTake(i2c_mutex, (TickType_t)10) == pdTRUE) {
            display.clearDisplay();
            
            // Linia 1 - titlu
            display.setTextSize(1);
            display.setCursor(0, 0);
            display.println("Motor AI - CNN1D");
            
            // Separator
            display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
            
            // Clasa detectata - mare
            display.setTextSize(2);
            display.setCursor(0, 16);
            display.println(oled_class);
            
            // Confidenta
            display.setTextSize(1);
            display.setCursor(0, 40);
            display.printf("Conf: %.1f%%", oled_conf);
            
            // Contor predictii - util pentru experiment
            display.setCursor(0, 52);
            display.printf("Pred #%d", pred_count);
            
            display.display();
            xSemaphoreGive(i2c_mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(200)); // 5Hz e suficient pentru display
    }
}
// =================================================================
// TASK 4 (Core 0, Prioritate Medie): DNS Server
// =================================================================
void DnsTask(void * pvParameters) {
  for(;;) {
    dnsServer.processNextRequest();
    vTaskDelay(pdMS_TO_TICKS(10)); // Ruleaza la 100Hz pentru responsivitate maxima
  }
}

// ==========================================
// SETUP: NUCLEUL 0 (WIFI & INIT)
// ==========================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n--- Start Sistem Multi-Task: AI + Web Server ---");
  
  sensor_queue = xQueueCreate(256, sizeof(SensorSample));
  telemetry_mutex = xSemaphoreCreateMutex();
  sensor_data_mutex = xSemaphoreCreateMutex();
  i2c_mutex = xSemaphoreCreateMutex();
  json_mutex = xSemaphoreCreateMutex();
  if (sensor_queue == NULL || telemetry_mutex == NULL || sensor_data_mutex == NULL || i2c_mutex == NULL || json_mutex == NULL) {
    Serial.println("Eroare creare obiecte FreeRTOS!");
    while(1);
  }

  ledcSetup(pwmChannel, pwmFreq, pwmResolution);
  ledcAttachPin(MOTOR_PIN, pwmChannel);
  setMotorSpeed(160); 

  Wire.begin(MPU_SDA_PIN, MPU_SCL_PIN);
  Wire.setClock(400000);
  
  xSemaphoreTake(i2c_mutex, portMAX_DELAY);
  if (!mpu.begin()) {
    Serial.println("Eroare MPU6050!");
    while (1) delay(10);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_16_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  xSemaphoreGive(i2c_mutex);

  static tflite::MicroErrorReporter micro_error_reporter;
  error_reporter = &micro_error_reporter;
  model = tflite::GetModel(cnn1d_int8_model);
  static tflite::AllOpsResolver resolver;
  static tflite::MicroInterpreter static_interpreter(model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
  interpreter = &static_interpreter;
  if (interpreter->AllocateTensors() != kTfLiteOk) {
    Serial.println("Eroare alocare tensori!");
    while(1);
  }

  input = interpreter->input(0);
  output = interpreter->output(0);
  input_is_int8 = (input->type == kTfLiteInt8); input_scale = input->params.scale; input_zero_point = input->params.zero_point;
  output_is_int8 = (output->type == kTfLiteInt8); output_scale = output->params.scale; output_zero_point = output->params.zero_point;

  IPAddress local_IP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP("Licenta_Motor_AI", "12345678");
  Serial.print("Access Point pornit. Conectati-va si accesati IP: ");
  Serial.println(WiFi.softAPIP());

  dnsServer.start(53, "*", local_IP);
  Serial.println("DNS Server pornit.");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html; charset=utf-8", index_html);
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "-1");
    request->send(response);
  });

  // Endpoint robust pentru polling de pe mobil (scapa de restrictiile Captive Portal)
  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
      String responseData;
      
      // Asteptam maxim 10 ticks (10ms) pentru Mutex. Daca e blocat, nu inghetam serverul web!
      if (xSemaphoreTake(json_mutex, (TickType_t)10) == pdTRUE) {
          responseData = String(current_json_data);
          xSemaphoreGive(json_mutex);
      } else {
          // Returnam un JSON de eroare ca sa stim clar de ce nu merge
          responseData = "{\"label\":\"Eroare Mutex C++\",\"conf\":\"0\",\"pwm\":\"0\",\"uptime\":\"0\",\"g_mag\":\"0\",\"a_lin_vib\":\"0\"}";
      }
      
      AsyncWebServerResponse *response = request->beginResponse(200, "application/json", responseData);
      response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
      response->addHeader("Pragma", "no-cache");
      response->addHeader("Expires", "-1");
      request->send(response);
  });

  // Endpoint pentru a controla motorul din pagina web
  server.on("/set_pwm", HTTP_GET, [](AsyncWebServerRequest *request){
      if(request->hasParam("v")){
          int new_pwm = request->getParam("v")->value().toInt();
          setMotorSpeed(new_pwm);
      }
      request->send(200, "text/plain", "OK");
  });

  server.onNotFound([](AsyncWebServerRequest *request){
    request->redirect("/");
  });
  oled_mutex = xSemaphoreCreateMutex();

  if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE) {
      if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
          Serial.println("OLED negasit!");
      } else {
          display.clearDisplay();
          display.setTextColor(SSD1306_WHITE);
          display.setTextSize(1);
          display.setCursor(0, 0);
          display.println("Sistem pornit...");
          display.display();
      }
      xSemaphoreGive(i2c_mutex);
  }
  server.begin();
  Serial.println("Server Web Pornit!");

  xTaskCreatePinnedToCore(SensorTask, "SensorTask", 4096, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(AITask, "AITask", 16384, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(TelemetryTask, "TelemetryTask", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(DnsTask, "DnsTask", 2048, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(OLEDTask, "OLEDTask", 4096, NULL, 1, NULL, 0);
  Serial.println("Toate task-urile au fost pornite. Sistemul este operational.");
}

// ==========================================
// LOOP ORIGINAL - RAMANE GOL
// ==========================================
void loop() {
  vTaskDelete(NULL);
}
