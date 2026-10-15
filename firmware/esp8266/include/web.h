#pragma once

#include <ESP8266WebServer.h>
#include "state.h"

// Register all HTTP routes.
void setupWebServer(ESP8266WebServer& server, DeviceState& state);
