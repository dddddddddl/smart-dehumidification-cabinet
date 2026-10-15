# Validation record

Checks were performed on **October 15, 2026**, on an Apple Silicon macOS host. They verify source/build preparation and selected software contracts, not new physical performance.

## Firmware and filesystem

| Build | Result | Resource use |
|---|---|---|
| ATmega2560 / `ATmega2560` environment | Passed | RAM 5,257 / 8,192 bytes; flash 91,654 / 262,144 bytes |
| ESP8266 / `nodemcuv2` environment | Passed | RAM 32,984 / 81,920 bytes; flash 343,283 / 1,044,464 bytes |
| ESP8266 LittleFS | Passed | Includes HTML, CSS, JavaScript and all 15 SVG assets from `web/` |

Recorded PlatformIO Core: **6.2.0**. Mega build: **atmelavr 5.3.0**, **MegaCore 3.1.0**, OneWire 2.3.8, DallasTemperature 3.11.0, BSEC 1.8.1492, Adafruit GFX 1.12.6, ST7735/ST7789 1.11.0, ArduinoJson 6.21.6 and Servo 1.3.0. ESP build: **espressif8266 4.2.1**, Arduino core **3.1.2**.

The original version ranges are retained rather than pretending to reconstruct exact original development pins. Upstream library/build scripts emitted warnings, including ESP8266 Python escape-sequence warnings; they did not prevent successful images. Neither firmware was flashed.

## Host checks

Four Python checks passed:

1. Explicit standby and parameter-triggered wake in the supplied serial mock.
2. Ignoring manual power while auto is enabled, then accepting it in manual mode.
3. Percentage bounds and retained legacy aliases in the mock.
4. English voice-reference row count and literal frame values.

Python source parsing and JavaScript syntax checks passed. The localhost preview passed HTTP checks for the page, JavaScript, CSS, SVG, state response and five command routes. These tests exercise the mock/reference data, not the full embedded main loop or physical actuators.

```bash
python3 -m pip install -r tools/requirements.txt
python3 -m unittest discover -s tests -v
node --check web/app.js
pio run -d firmware/mega2560
pio run -d firmware/esp8266
pio run -d firmware/esp8266 -t buildfs
```

## Presentation and file checks

The six-slide English presentation was rendered and checked during its names/IDs cleanup. Structural package and import checks passed. Decorative images extending outside slide boundaries follow the original design and were not treated as measured engineering content.

Native hardware files and the original voice workbook are preserved by byte-level hashes. The original demonstration is retained locally; its public copy replaces the first 11 seconds with an English title card, preserves duration/audio and is re-encoded. The voice binary is preserved locally with its hash documented, pending redistribution permission. The public video’s 178.5-second duration and audio-track presence were checked. Title/footage frames were viewed; two-second OCR sampling found no known participant names or student identifiers. No award image is included. All 90 source-file dispositions were accounted for. English source/authored text, local documentation/frontend links, original AP-value exclusion and editable presentation identity removal passed the publication audit.

## Hardware boundaries

No physical cabinet, sensors, voice module, USB-to-TTL adapter, relays or servos were connected. No water collection, humidity transient, power/thermal, electrical isolation, voice accuracy or UV/purification efficacy experiment was performed.

Specific source limitations remain: invalid temperature readings can leave a default/stale value; sleep disables cooling; the loaded humidity target is reset to 65 at boot; some web controls latch optimistic values; the voice UART/I²C relationship and command-index mismatches need device-level verification. Successful compilation does not resolve these integration points.
