#pragma once
#include <Arduino.h>

// =====================================================================
//  config.h  ::  parametri de compilare + SURSA CANONICA pt etichete
//  Aici stau DOAR constante. Nimic ce are stare la runtime.
// =====================================================================

// ---- Dimensiuni fereastra / model (TREBUIE identice cu antrenarea) ----
constexpr int WINDOW_SIZE  = 128;  // esantioane / fereastra
constexpr int STEP_SIZE    = 64;   // pas = 50% overlap
constexpr int NUM_FEATURES = 6;    // ax ay az gx gy gz
constexpr int NUM_CLASSES  = 5;

// Osciloscop web: cate esantioane pastram pentru afisare (ring buffer).
constexpr int SCOPE_N = 120;

// ---- Pini ----
constexpr int MPU_SDA_PIN = 21;
constexpr int MPU_SCL_PIN = 22;
constexpr int MOTOR_PIN   = 14;

// ---- PWM motor ----
constexpr int PWM_FREQ    = 5000;
constexpr int PWM_CHANNEL = 0;
constexpr int PWM_RES     = 8;     // 8 biti -> 0..255
constexpr int INITIAL_PWM = 200;

// ---- SoftAP ----
constexpr char AP_SSID[] = "Mentenanta-ESP32";
constexpr char AP_PASS[] = "vibratii2026";   // >= 8 caractere (WPA2)

// =====================================================================
//  SURSA UNICA pt ordinea claselor + vitezele permise.
//  Declarate 'extern' -> exista O SINGURA copie in tot binarul, definita
//  in config.cpp. Asa nu se pot desincroniza intre fisiere.
//
//  LABEL_MAP: 0=Repaus 1=Normal 2=Dezechilibru 3=Frecare 4=Slabit
// =====================================================================
extern const char* const class_names[NUM_CLASSES];
extern const char* const feature_names[NUM_FEATURES];

// Doar vitezele din distributia de training (+ Stop). Orice in afara ->
// shift de distributie -> modelul devine nesigur. De aceea whitelist.
extern const int kAllowedPwm[];
extern const int kNumAllowedPwm;