#pragma once

#include <Arduino.h>
#include "state.h"

// Trim whitespace from string.
String trimCopy(const String& input);
// Send serial command text.
void sendCommand(const String& cmd);
// Parse incoming mock JSON.
bool parseMockJson(const String& line, DeviceState& state);
// Process one serial line.
void processSerialLine(const String& line, DeviceState& state);
// Read and buffer serial data.
void readSerialData(String& serialBuffer, DeviceState& state);
// Mark device offline by timeout.
void refreshOnlineState(DeviceState& state, unsigned long timeoutMs);
