#include "web.h"
#include <Arduino.h>
#include <LittleFS.h>
#include "protocol.h"

namespace {
ESP8266WebServer* gServer = nullptr;
DeviceState* gState = nullptr;
bool gFsReady = false;

// Convert water level label.
String waterLevelText(const String& value) {
  if (value == "empty") return "empty";
  if (value == "normal") return "normal";
  if (value == "fulll" || value == "full") return "full";
  if (value == "error") return "error";
  return value.length() > 0 ? value : "unknown";
}

// Convert online status text.
String onlineText(const bool online) {
  return online ? "Online" : "Offline";
}

// Escape a string for safe JSON output.
String jsonEscape(const String& input) {
  String out;
  out.reserve(input.length() + 16);
  for (size_t i = 0; i < input.length(); ++i) {
    const char c = input[i];
    if (c == '\\') {
      out += "\\\\";
    } else if (c == '\"') {
      out += "\\\"";
    } else if (c == '\n') {
      out += "\\n";
    } else if (c == '\r') {
      out += "\\r";
    } else if (c == '\t') {
      out += "\\t";
    } else {
      out += c;
    }
  }
  return out;
}

// Built-in stylesheet fallback when filesystem is unavailable.
String defaultCss() {
  String css;
  css += "body{font-family:Arial,sans-serif;background:#f4f6f8;margin:0;padding:20px;}";
  css += ".wrap{max-width:720px;margin:0 auto;background:#fff;padding:20px;border-radius:12px;box-shadow:0 2px 12px rgba(0,0,0,.08);}";
  css += "h1{margin-top:0;font-size:24px;}";
  css += ".status{padding:10px 14px;border-radius:8px;margin-bottom:16px;background:#eef3f8;}";
  css += ".grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:12px;}";
  css += ".card{background:#f8fafc;border:1px solid #e5e7eb;border-radius:10px;padding:14px;}";
  css += ".label{font-size:13px;color:#666;display:block;margin-bottom:6px;}";
  css += ".value{font-size:20px;font-weight:bold;color:#111;}";
  css += ".section{margin-top:20px;}";
  css += ".buttons a{display:inline-block;margin:6px 8px 0 0;padding:10px 14px;border-radius:8px;text-decoration:none;color:#fff;background:#2563eb;}";
  css += ".buttons a.secondary{background:#6b7280;}";
  css += ".buttons a.warn{background:#ea580c;}";
  css += ".raw{margin-top:12px;padding:12px;background:#111827;color:#e5e7eb;border-radius:10px;white-space:pre-wrap;word-break:break-all;}";
  css += "form{margin-top:12px;}";
  css += "input[type='number']{padding:8px;width:100px;}";
  css += "button{padding:9px 14px;border:none;border-radius:8px;background:#2563eb;color:#fff;cursor:pointer;}";
  return css;
}

// Build HTML head content.
String htmlHeader() {
  String html;
  html += "<!DOCTYPE html><html><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<meta http-equiv='refresh' content='5'>";
  html += "<title>ESP-01S Mock Debug</title>";
  html += "<link rel='stylesheet' href='/app.css'>";
  html += "</head><body><div class='wrap'>";
  return html;
}

// Render full status page.
String renderHtml(const DeviceState& state) {
  String html = htmlHeader();

  html += "<h1>ESP-01S Mock Serial Debug</h1>";
  html += "<div class='status'><strong>Device:</strong> " + onlineText(state.online) + "</div>";

  html += "<div class='grid'>";
  html += "<div class='card'><span class='label'>Temperature</span><span class='value'>" + String(state.temperature, 1) + " C</span></div>";
  html += "<div class='card'><span class='label'>Humidity</span><span class='value'>" + String(state.humidity, 1) + " %</span></div>";
  html += "<div class='card'><span class='label'>IAQ</span><span class='value'>" + String(state.iaq) + "</span></div>";
  html += "<div class='card'><span class='label'>Water Level</span><span class='value'>" + waterLevelText(state.waterLevel) + "</span></div>";
  html += "<div class='card'><span class='label'>Status</span><span class='value'>" + state.status + "</span></div>";
  html += "<div class='card'><span class='label'>UV</span><span class='value'>" + state.uv + "</span></div>";
  html += "<div class='card'><span class='label'>Auto Mode</span><span class='value'>" + state.autoMode + "</span></div>";
  html += "<div class='card'><span class='label'>Power Level</span><span class='value'>" + String(state.powerLevel) + "</span></div>";
  html += "<div class='card'><span class='label'>Last Update</span><span class='value'>" + String(state.lastUpdateMs / 1000) + " s</span></div>";
  html += "</div>";

  html += "<div class='section'><h2>Quick Actions</h2><div class='buttons'>";
  html += "<a href='/uv_on'>UV On</a>";
  html += "<a href='/uv_off' class='secondary'>UV Off</a>";
  html += "<a href='/auto_on'>Auto On</a>";
  html += "<a href='/auto_off' class='secondary'>Auto Off</a>";
  html += "<a href='/manual_change' class='warn'>Manual Change</a>";
  html += "</div></div>";

  html += "<div class='section'><h2>Set Power</h2><div class='buttons'>";
  html += "<a href='/set_strength?level=0' class='secondary'>0</a>";
  html += "<a href='/set_strength?level=40'>40</a>";
  html += "<a href='/set_strength?level=70'>70</a>";
  html += "<a href='/set_strength?level=100' class='warn'>100</a>";
  html += "</div></div>";

  html += "<div class='section'><h2>Set Target Humidity</h2>";
  html += "<form action='/set_target' method='get'>";
  html += "<input type='number' name='value' min='0' max='100' value='" + String(state.targetHumidity) + "'>";
  html += "<button type='submit'>Send</button>";
  html += "</form></div>";

  html += "<div class='section'><h2>Last Raw JSON</h2>";
  html += "<div class='raw'>" + state.lastRawJson + "</div></div>";

  html += "</div></body></html>";
  return html;
}

// Redirect browser to home.
void redirectHome() {
  if (gServer->hasHeader("X-Requested-With")) {
    const String requestedWith = gServer->header("X-Requested-With");
    if (requestedWith == "fetch" || requestedWith == "XMLHttpRequest") {
      gServer->send(204, "text/plain", "");
      return;
    }
  }
  gServer->sendHeader("Location", "/");
  gServer->send(302, "text/plain", "");
}

// Handle root page request.
void handleRoot() {
  if (gFsReady && LittleFS.exists("/index.html")) {
    File indexFile = LittleFS.open("/index.html", "r");
    if (indexFile) {
      gServer->streamFile(indexFile, "text/html; charset=utf-8");
      indexFile.close();
      return;
    }
  }
  gServer->send(200, "text/html; charset=utf-8", renderHtml(*gState));
}

// Serve stylesheet from LittleFS with in-memory fallback.
void handleAppCss() {
  if (gFsReady && LittleFS.exists("/app.css")) {
    File cssFile = LittleFS.open("/app.css", "r");
    if (cssFile) {
      gServer->streamFile(cssFile, "text/css; charset=utf-8");
      cssFile.close();
      return;
    }
  }
  gServer->send(200, "text/css; charset=utf-8", defaultCss());
}

// Serve JavaScript file from LittleFS.
void handleAppJs() {
  if (gFsReady && LittleFS.exists("/app.js")) {
    File jsFile = LittleFS.open("/app.js", "r");
    if (jsFile) {
      gServer->streamFile(jsFile, "application/javascript; charset=utf-8");
      jsFile.close();
      return;
    }
  }
  gServer->send(200, "application/javascript; charset=utf-8", "");
}

// Return device state as JSON for static UI polling.
void handleState() {
  String json;
  json.reserve(320);
  const unsigned long nowMs = millis();
  const unsigned long secondsSinceUpdate =
      (gState->lastChangedMs > 0 && nowMs >= gState->lastChangedMs)
          ? ((nowMs - gState->lastChangedMs) / 1000UL)
          : 0UL;
  json += "{";
  json += "\"temperature\":" + String(gState->temperature, 1) + ",";
  json += "\"humidity\":" + String(gState->humidity, 1) + ",";
  json += "\"iaq\":\"" + jsonEscape(gState->iaq) + "\",";
  json += "\"waterLevel\":\"" + jsonEscape(gState->waterLevel) + "\",";
  json += "\"status\":\"" + jsonEscape(gState->status) + "\",";
  json += "\"uv\":\"" + jsonEscape(gState->uv) + "\",";
  json += "\"autoMode\":\"" + jsonEscape(gState->autoMode) + "\",";
  json += "\"powerLevel\":" + String(gState->powerLevel) + ",";
  json += "\"targetHumidity\":" + String(gState->targetHumidity) + ",";
  json += "\"online\":" + String(gState->online ? "true" : "false") + ",";
  json += "\"secondsSinceUpdate\":" + String(secondsSinceUpdate) + ",";
  json += "\"lastUpdateMs\":" + String(gState->lastUpdateMs) + ",";
  json += "\"lastRawJson\":\"" + jsonEscape(gState->lastRawJson) + "\"";
  json += "}";
  gServer->send(200, "application/json; charset=utf-8", json);
}

// Handle UV on action.
void handleUvOn() {
  if (gState->uv != "on") {
    gState->uv = "on";
    gState->lastChangedMs = millis();
  }
  gState->uvManualOverride = true;
  sendCommand("{\"uv\":\"on\"}");
  redirectHome();
}

// Handle UV off action.
void handleUvOff() {
  if (gState->uv != "off") {
    gState->uv = "off";
    gState->lastChangedMs = millis();
  }
  gState->uvManualOverride = true;
  sendCommand("{\"uv\":\"off\"}");
  redirectHome();
}

// Handle auto mode on.
void handleAutoOn() {
  if (gState->autoMode != "on") {
    gState->autoMode = "on";
    gState->lastChangedMs = millis();
  }
  gState->autoManualOverride = true;
  sendCommand("{\"auto\":\"on\"}");
  redirectHome();
}

// Handle auto mode off.
void handleAutoOff() {
  if (gState->autoMode != "off") {
    gState->autoMode = "off";
    gState->lastChangedMs = millis();
  }
  gState->autoManualOverride = true;
  sendCommand("{\"auto\":\"off\"}");
  redirectHome();
}

// Handle manual change action.
void handleManualChange() {
  sendCommand("{\"cmd_silica\":\"change_now\"}");
  redirectHome();
}

// Handle power level update.
void handleSetStrength() {
  const String level = gServer->arg("level");
  if (level.length() > 0) {
    const int next = level.toInt();
    if (gState->powerLevel != next) {
      gState->powerLevel = next;
      gState->lastChangedMs = millis();
    }
    gState->powerManualOverride = true;
    sendCommand("{\"p_lvl\":" + String(next) + "}");
  }
  redirectHome();
}

// Handle target humidity update.
void handleSetTarget() {
  const String value = gServer->arg("value");
  if (value.length() > 0) {
    int next = value.toInt();
    if (next < 0) next = 0;
    if (next > 100) next = 100;
    if (gState->targetHumidity != next) {
      gState->targetHumidity = next;
      gState->lastChangedMs = millis();
    }
    sendCommand("{\"target_h\":" + String(next) + "}");
  }
  redirectHome();
}

// Handle explicit running mode request.
void handlePowerRunning() {
  if (gState->status != "running") {
    gState->status = "running";
    gState->lastChangedMs = millis();
  }
  sendCommand("{\"power\":\"running\"}");
  redirectHome();
}

// Handle explicit standby mode request.
void handlePowerStandby() {
  if (gState->status != "standby") {
    gState->status = "standby";
    gState->lastChangedMs = millis();
  }
  sendCommand("{\"power\":\"standby\"}");
  redirectHome();
}
}  // namespace

//Register HTTP routes
void setupWebServer(ESP8266WebServer& server, DeviceState& state) {
  gServer = &server;
  gState = &state;
  gFsReady = LittleFS.begin();
  server.collectHeaders("X-Requested-With");
  server.on("/", handleRoot);
  server.on("/app.css", handleAppCss);
  server.on("/app.js", handleAppJs);
  server.on("/state", handleState);
  server.serveStatic("/assets/", LittleFS, "/assets/");
  server.on("/uv_on", handleUvOn);
  server.on("/uv_off", handleUvOff);
  server.on("/auto_on", handleAutoOn);
  server.on("/auto_off", handleAutoOff);
  server.on("/manual_change", handleManualChange);
  server.on("/set_strength", handleSetStrength);
  server.on("/set_target", handleSetTarget);
  server.on("/power_running", handlePowerRunning);
  server.on("/power_standby", handlePowerStandby);
  server.begin();
}
