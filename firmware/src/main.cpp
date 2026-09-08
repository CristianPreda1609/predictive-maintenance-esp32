// =====================================================================
//  Mentenanta Predictiva - CNN1D pe ESP32  ::  DASHBOARD WEB (v2 modular)
//
//  main.cpp e DOAR cablaj. Logica reala e in module:
//    config.{h,cpp}   - constante + ordinea canonica a claselor
//    realtime.{h,cpp} - MPU + scaler + CNN1D + state  (Core 1)
//    web.{h,cpp}      - SoftAP + WebServer            (Core 0)
//    web_page.h       - pagina HTML servita
//
//  Artefacte (O SINGURA sursa, incluse doar in realtime.cpp):
//    model_cnn1d_int8.h + preprocessing.h
// =====================================================================
#include <Arduino.h>

#include "config.h"
#include "realtime.h"
#include "web.h"

// Oprire controlata: afiseaza cauza pe serial si stai. Mai bine asa decat
// sa pornesti un demo rupt fara sa stii de ce.
static void fatal(const char* msg) {
  Serial.println(msg);
  while (true) delay(10);
}

void setup() {
  Serial.begin(115200); delay(200);
  Serial.println("\n--- CNN1D Mentenanta Predictiva :: WEB v2 (modular) ---");

  // Ordine: intai inferenta (hardware + model + sanity check artefacte),
  // apoi web. Daca pica ceva critic, ne oprim cu mesaj clar.
  if (!realtime_init()) fatal("STOP: init real-time esuat.");
  if (!web_init())      fatal("STOP: init web esuat.");

  realtime_start_tasks();   // SensorTask + AITask -> Core 1
  web_start_task();         // WebTask           -> Core 0

  Serial.println("Pornit: senzor+CNN pe Core 1, web pe Core 0.\n");
}

// Tot ruleaza in task-uri FreeRTOS; loop()-ul Arduino nu ne trebuie.
void loop() { vTaskDelete(NULL); }