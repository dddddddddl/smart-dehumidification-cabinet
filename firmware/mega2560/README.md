# ATmega2560 main controller

This PlatformIO project contains the supplied cabinet control program. Start with `include/HardwareConfig.h` for pins/state, then read `setup()`, `loop()`, `updateHardwareState()`, `processJSONCommands()` and the servo/menu handlers in `src/main.cpp`.

## Build and upload

From the repository root:

```bash
pio run -d firmware/mega2560
pio run -d firmware/mega2560 -t upload
pio device monitor -d firmware/mega2560 -b 115200
```

The original environment/board identifier is **`ATmega2560`**. PlatformIO resolved it to the **MegaCore 3.1.0** Arduino framework in the recorded build. It is not silently replaced by the different `megaatmega2560` board definition. Check the actual board, clock/bootloader and pin layout before uploading; no physical upload was performed here.

Libraries remain declared by the supplied version ranges: OneWire, DallasTemperature, Bosch BSEC, Adafruit GFX/ST7789, ArduinoJson and Servo. Resolved versions are in [validation](../../docs/validation.md).

## Control flow

`loop()` services buttons, sensor updates, the control calculation, serial commands, voice results, servo phases and the menu. Most work is timer-driven; the serial `readStringUntil()` and some library/startup calls can still block, so the loop is not described as a verified hard-real-time system.

- The humidity PID uses gains `(-4.0, -0.2, -0.1)` and a nominal target of 65% RH.
- The thermal PID uses `(15.0, 1.0, 0.5)` and a 60 °C setpoint to limit requested power.
- Actual PWM is the smaller of humidity/manual demand and thermal allowance. Full-tank detection forces actual output to zero and disables automatic mode.
- The fan runs when output is nonzero, UV is on, or Peltier temperature exceeds 45 °C. The supplied sleep branch overrides this and turns the fan, UV, PWM and backlight off.
- The material sequence drives continuous-rotation servos by timed pulses: output servo, wait for gravity drop, input servo, stop. No measured position feedback is included.

The PID implementation is a discrete update with clamped integral and no explicit `dt` term. Its effective response depends on update timing, which can vary with other work in the main loop. No PID retuning or control-law change was made in preparation.

## Main I/O

| Function | Configuration |
|---|---|
| TFT | CS 10, DC 9, RESET 8; hardware SPI MOSI 51/SCK 52; backlight 23 |
| Encoder / buttons | A 2, B 3, PUSH 19, K0 18 |
| DS18B20 | digital 6 |
| Water-level sensor | A2; threshold 307 on the nominal 10-bit scale |
| Dehumidifier PWM | digital 7 |
| UV / fan | digital 22 / 24 |
| Output / input servo | digital 11 / 12 |
| Environmental sensor | I²C `0x77`; Mega SDA 20/SCL 21 |
| Voice result interface | I²C `0x34`, register 100 |
| ESP8266 link | `Serial2`, TX2 16/RX2 17, 19200 baud |

The source notes refer to ESP-01S hardware, while the accompanying ESP firmware targets NodeMCU. Reconcile the actual module, logic levels, power and serial pins rather than assuming the board definitions are interchangeable.

## Persistence and known boundaries

BSEC state uses EEPROM magic byte `0xAA` at address 0 and a blob from address 1. User settings begin at address 200 with magic `0x5A`. Although settings are loaded at boot, the supplied `setup()` subsequently sets target humidity back to 65; this behavior is retained and documented.

Invalid or absent DS18B20 readings do not establish a fail-safe shutdown: the last/default temperature may remain in state. Full-tank and thermal behavior need real sensor/fault tests. The web status reports requested `p_lvl`, not actual temperature-limited PWM. The voice command mapping also has unresolved indices; see [voice/README.md](../../voice/README.md).
