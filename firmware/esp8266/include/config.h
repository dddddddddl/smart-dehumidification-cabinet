#pragma once

#if __has_include("local_config.h")
#include "local_config.h"
#endif
#ifndef DEHUMIDIFIER_AP_SSID
#define DEHUMIDIFIER_AP_SSID "SmartDehumidifier-Demo"
#endif
#ifndef DEHUMIDIFIER_AP_PASSWORD
#define DEHUMIDIFIER_AP_PASSWORD "demo-only-8266"
#endif

namespace AppConfig {
// In ESP8266 AP mode, the gateway IP is typically 192.168.4.1.
constexpr const char* kApSsid = DEHUMIDIFIER_AP_SSID;
constexpr const char* kApPassword = DEHUMIDIFIER_AP_PASSWORD;
constexpr unsigned long kDeviceTimeoutMs = 5000;
}
