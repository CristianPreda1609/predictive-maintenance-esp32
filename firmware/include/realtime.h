#pragma once
#include <Arduino.h>
#include "config.h"

// =====================================================================
//  realtime.h  ::  interfata stratului real-time (senzor + CNN1D + state)
//
//  Tot ce e intern (model, scaler, window_buffer, mutex-uri, g_pred) e
//  ascuns in realtime.cpp. Restul codului ajunge la stare DOAR prin
//  functiile de aici -> suprafata minima => greu de desincronizat.
// =====================================================================

// Snapshot consistent al starii pentru stratul web. Toate campurile sunt
// copiate sub state_mutex intr-un singur apel -> web-ul nu vede stari rupte
// (ex. clasa noua cu probabilitatile vechi).
struct StateSnapshot {
  char     cls[16];
  float    conf;
  float    probs[NUM_CLASSES];
  uint32_t infer_us;                      // latenta Invoke (us); 0 = filtru repaus
  uint32_t count;                         // heartbeat (creste la fiecare predictie)
  int      pwm;
  float    scope[SCOPE_N][NUM_FEATURES];  // deja in ordine cronologica
};

// Init hardware (MPU) + TFLite + obiecte FreeRTOS. false = fatal.
bool realtime_init();

// Porneste SensorTask (100Hz) + AITask (CNN1D), ambele pe Core 1.
void realtime_start_tasks();

// Copie atomica a starii pentru /data. false daca nu prinde mutex-ul in 'wait'.
bool realtime_snapshot(StateSnapshot* out, TickType_t wait);

// Seteaza PWM doar daca e pe whitelist-ul antrenat. false = respins.
bool realtime_set_pwm(int v);
int  realtime_get_pwm();