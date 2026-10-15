#include "protocol.h"

// Trim whitespace from string.
String trimCopy(const String& input) {
  String value = input;
  value.trim();
  return value;
}

// Extract value by key.
static String extractJsonValue(const String& json, const String& key) {
  const String token = "\"" + key + "\"";
  const int keyPos = json.indexOf(token);
  if (keyPos < 0) {
    return "";
  }

  const int colonPos = json.indexOf(':', keyPos + token.length());
  if (colonPos < 0) {
    return "";
  }

  int valueStart = colonPos + 1;
  while (valueStart < static_cast<int>(json.length())) {
    const char c = json[valueStart];
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
      break;
    }
    valueStart++;
  }

  if (valueStart >= static_cast<int>(json.length())) {
    return "";
  }

  if (json[valueStart] == '"') {
    const int valueEnd = json.indexOf('"', valueStart + 1);
    if (valueEnd < 0) {
      return "";
    }
    return json.substring(valueStart + 1, valueEnd);
  }

  int valueEnd = valueStart;
  while (valueEnd < static_cast<int>(json.length()) &&
         json[valueEnd] != ',' && json[valueEnd] != '}') {
    valueEnd++;
  }

  return trimCopy(json.substring(valueStart, valueEnd));
}

// Send serial command text.
void sendCommand(const String& cmd) {
  Serial.println(cmd);
}

// Parse incoming mock JSON.
bool parseMockJson(const String& line, DeviceState& state) {
  if (!line.startsWith("{")) {
    return false;
  }

  const String t = extractJsonValue(line, "t");
  const String h = extractJsonValue(line, "h");
  const String iaq = extractJsonValue(line, "iaq");
  const String water = extractJsonValue(line, "w_lvl");
  const String status = extractJsonValue(line, "status");
  const String uv = extractJsonValue(line, "uv");
  const String autoMode = extractJsonValue(line, "auto");
  const String power = extractJsonValue(line, "p_lvl");

  if (t.length() == 0 && h.length() == 0 && iaq.length() == 0 && status.length() == 0) {
    return false;
  }

  bool changed = false;
  if (t.length() > 0) {
    const float next = t.toFloat();
    if (state.temperature != next) {
      state.temperature = next;
      changed = true;
    }
  }
  if (h.length() > 0) {
    const float next = h.toFloat();
    if (state.humidity != next) {
      state.humidity = next;
      changed = true;
    }
  }
  if (iaq.length() > 0) {
    if (state.iaq != iaq) {
      state.iaq = iaq;
      changed = true;
    }
  }
  if (water.length() > 0 && state.waterLevel != water) {
    state.waterLevel = water;
    changed = true;
  }
  if (status.length() > 0 && state.status != status) {
    state.status = status;
    changed = true;
  }
  if (uv.length() > 0 && !state.uvManualOverride && state.uv != uv) {
    state.uv = uv;
    changed = true;
  }
  if (autoMode.length() > 0 && !state.autoManualOverride && state.autoMode != autoMode) {
    state.autoMode = autoMode;
    changed = true;
  }
  if (power.length() > 0 && !state.powerManualOverride) {
    const int next = power.toInt();
    if (state.powerLevel != next) {
      state.powerLevel = next;
      changed = true;
    }
  }

  state.lastRawJson = line;
  state.online = true;
  const unsigned long nowMs = millis();
  state.lastRxMs = nowMs;
  state.lastUpdateMs = nowMs;
  if (changed || state.lastChangedMs == 0) {
    state.lastChangedMs = nowMs;
  }
  return true;
}

// Process one serial line.
void processSerialLine(const String& line, DeviceState& state) {
  const String trimmed = trimCopy(line);
  if (trimmed.length() == 0) {
    return;
  }
  parseMockJson(trimmed, state);
}

// Read and buffer serial data.
void readSerialData(String& serialBuffer, DeviceState& state) {
  while (Serial.available()) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      processSerialLine(serialBuffer, state);
      serialBuffer = "";
      continue;
    }

    serialBuffer += c;
    if (serialBuffer.length() > 512) {
      serialBuffer = "";
    }
  }
}

// Mark device offline by timeout.
void refreshOnlineState(DeviceState& state, const unsigned long timeoutMs) {
  if (millis() - state.lastRxMs > timeoutMs) {
    state.online = false;
  }
}
