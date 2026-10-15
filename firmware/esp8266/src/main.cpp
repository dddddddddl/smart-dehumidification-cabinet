#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include "config.h"
#include "protocol.h"
#include "state.h"
#include "web.h"

ESP8266WebServer gServer(80);
DeviceState gState;
String gSerialBuffer;

// Initialize AP network mode.
void setupWiFi() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AppConfig::kApSsid, AppConfig::kApPassword);
}

// Initialize firmware components.
void setup() {
  Serial.begin(19200);
  setupWiFi();
  setupWebServer(gServer, gState);
}

// Run periodic service loop.
void loop() {
  gServer.handleClient();
  readSerialData(gSerialBuffer, gState);
  refreshOnlineState(gState, AppConfig::kDeviceTimeoutMs);
}
