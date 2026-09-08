#include "web.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#include "config.h"
#include "realtime.h"
#include "web_page.h"

// =====================================================================
//  STARE INTERNA (server-ul nu trebuie vazut de nimeni altcineva)
// =====================================================================
namespace {

WebServer server(80);

void handleRoot() { server.send_P(200, "text/html", PAGE_HTML); }

void handleData() {
  // Singura cale prin care web-ul ajunge la stare: un snapshot atomic.
  StateSnapshot s;
  if (!realtime_snapshot(&s, pdMS_TO_TICKS(50))) {
    server.send(503, "application/json", "{\"err\":\"busy\"}");
    return;
  }

  // Serializare JSON manuala (evitam ArduinoJson -> zero alocari dinamice
  // mari). reserve() ca sa nu reallocam String-ul de zeci de ori.
  String out; out.reserve(7000);
  out += "{\"cls\":\""; out += s.cls; out += "\",\"conf\":"; out += String(s.conf, 1);
  out += ",\"n\":";   out += String(s.count);
  out += ",\"lat\":"; out += String(s.infer_us / 1000.0f, 1);   // us -> ms
  out += ",\"pwm\":"; out += String(s.pwm);
  out += ",\"p\":[";
  for (int i = 0; i < NUM_CLASSES; ++i) { if (i) out += ","; out += String(s.probs[i], 3); }
  out += "],";
  // feature_names vine din config (sursa unica) - nu redeclaram cheile aici.
  for (int f = 0; f < NUM_FEATURES; ++f) {
    out += "\""; out += feature_names[f]; out += "\":[";
    for (int k = 0; k < SCOPE_N; ++k) { if (k) out += ","; out += String(s.scope[k][f], 3); }
    out += "]";
    if (f < NUM_FEATURES - 1) out += ",";
  }
  out += "}";
  server.send(200, "application/json", out);
}

void handlePwm() {
  if (!server.hasArg("v")) { server.send(400, "text/plain", "lipseste v"); return; }
  int v = server.arg("v").toInt();
  // Validarea whitelist-ului traieste in realtime (langa motor), nu aici.
  if (!realtime_set_pwm(v)) { server.send(422, "text/plain", "PWM neantrenat - respins"); return; }
  server.send(200, "application/json", String("{\"pwm\":") + v + "}");
}

// Server sincron pe Core 0: poll constant, dar cedeaza 2ms ca sa nu
// infometeze idle/WiFi housekeeping.
void WebTask(void* pv) {
  for (;;) { server.handleClient(); vTaskDelay(pdMS_TO_TICKS(2)); }
}

}  // namespace anonim

// =====================================================================
//  API PUBLIC
// =====================================================================
bool web_init() {
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(AP_SSID, AP_PASS)) return false;
  Serial.printf("AP: SSID=\"%s\" PASS=\"%s\" -> http://%s\n",
                AP_SSID, AP_PASS, WiFi.softAPIP().toString().c_str());

  server.on("/",     handleRoot);
  server.on("/data", handleData);
  server.on("/pwm",  handlePwm);
  server.begin();
  return true;
}

void web_start_task() {
  xTaskCreatePinnedToCore(WebTask, "Web", 8192, nullptr, 1, nullptr, 0);
}