#pragma once

#include <Arduino.h>

struct DeviceState {
  float temperature = 0.0f;
  float humidity = 0.0f;
  String iaq = "0";
  String waterLevel = "unknown";
  String status = "standby";
  String uv = "off";
  String autoMode = "off";
  int powerLevel = 0;
  int targetHumidity = 65;
  bool uvManualOverride = false;
  bool autoManualOverride = false;
  bool powerManualOverride = false;

  bool online = false;
  // Last received status frame timestamp (used by online/offline timeout).
  unsigned long lastRxMs = 0;
  // Last effective state change timestamp (used by "x seconds ago" display).
  unsigned long lastChangedMs = 0;
  // Legacy field kept for compatibility with older fallback rendering.
  unsigned long lastUpdateMs = 0;
  String lastRawJson = "{}";
};
