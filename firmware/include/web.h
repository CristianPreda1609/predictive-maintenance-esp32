#pragma once

// =====================================================================
//  web.h  ::  stratul web (SoftAP + WebServer). Ruleaza pe Core 0.
//  Nu atinge inferenta direct - cere stare prin realtime_snapshot().
// =====================================================================

bool web_init();        // SoftAP + rute + server.begin(). false = fatal.
void web_start_task();  // WebTask pe Core 0