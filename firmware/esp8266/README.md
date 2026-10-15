# ESP8266 web controller

The ESP8266 runs a local Wi-Fi access point, hosts the dashboard from LittleFS, parses main-controller status and forwards control requests over UART. The original PlatformIO target is **`nodemcuv2`**, with ESP8266 Arduino Core 3.1.2 in the recorded build.

## Build and configure

From the repository root:

```bash
pio run -d firmware/esp8266
pio run -d firmware/esp8266 -t buildfs
```

`platformio.ini` uses `data_dir = ../../web`, so the dashboard is stored once and included in the filesystem build. To flash after checking your hardware:

```bash
pio run -d firmware/esp8266 -t upload
pio run -d firmware/esp8266 -t uploadfs
pio device monitor -d firmware/esp8266 -b 19200
```

Full firmware and filesystem builds passed; no flashing occurred during preparation. The malformed inline upload-speed comment was converted to a proper INI comment without changing the selected baud rate.

## Access point

The public build uses **`SmartDehumidifier-Demo`** with the explicit, non-secret demo password **`demo-only-8266`**. These are replacement demonstration values, not the original project credentials.

For physical use, copy `include/local_config.example.h` to the Git-ignored `include/local_config.h` and choose your own SSID/password. The local file overrides the demo values. Keep the password within ESP8266 SoftAP's supported length.

After flashing, join the configured access point and open **http://192.168.4.1/**. There is no cloud backend. HTTP control endpoints are unauthenticated and intended for the prototype's local AP, not public Internet hosting.

## Code map

| File | Responsibility |
|---|---|
| `src/main.cpp` | Wi-Fi, serial and web startup; main service loop |
| `src/protocol.cpp` | Newline buffering, flat status-field parsing and command transmission |
| `src/web.cpp` | HTTP routes, JSON response, LittleFS assets and fallback page |
| `include/state.h` | Last received state plus optimistic command overrides |
| `include/config.h` | AP values and 5-second receive timeout |

`protocol.cpp` uses a small field extractor, not a general JSON parser. It is designed around the provided flat status packets. UV/auto/power manual overrides are latched in the source and can prevent later telemetry from replacing those displayed values. `targetHumidity` is locally retained and is absent from the original Mega status frame. These fields should not be treated as verified actuator acknowledgements.

The [protocol guide](../../docs/protocol.md) lists all routes and messages. The [web guide](../../web/README.md) describes the standalone HTML/CSS/JavaScript implementation.
