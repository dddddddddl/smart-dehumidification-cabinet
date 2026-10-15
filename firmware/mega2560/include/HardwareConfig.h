#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

#include <Arduino.h>

// Pin definitions for Arduino Mega 2560

// I2C pins use the Mega 2560 hardware defaults: SDA = 20, SCL = 21.
// No manual pin definition is required. Wire.begin() is sufficient.

// TFT SPI pins using hardware SPI: MOSI = 51, SCK = 52.
const uint8_t TFT_CS_Pin  = 10;
const uint8_t TFT_DC_Pin  = 9;
const uint8_t TFT_RST_Pin = 8;

// Encoder and button pins using external interrupts.
const uint8_t Encoder_A_Pin    = 2;
const uint8_t Encoder_B_Pin    = 3;
const uint8_t K0_Pin           = 18;
const uint8_t Encoder_Push_Pin = 19;


// Sensor pins.
const uint8_t DS18B20_PIN      = 6;  // DS18B20 1-Wire pin.
const uint8_t WATER_LEVEL_PIN = A2;         // Analog pin A2 on the Mega 2560.
const int WATER_LEVEL_THRESHOLD = 307;      // Threshold set to about 30% of the full ADC range (1023).

// Actuator pins.
const uint8_t DEHUMIDIFIER_PWM_PIN = 7;  // Dehumidifier PWM control pin. Supports analogWrite().
const uint8_t UV_LAMP_PIN          = 22; // UV lamp relay.
const uint8_t TFT_BL_Pin           = 23;
const uint8_t FAN_PIN              = 24; // Fan relay.

// Desiccant replacement servo pins.
const uint8_t SERVO_OUT_PIN = 11; // Output servo.
const uint8_t SERVO_IN_PIN  = 12; // Input servo.

// 360-degree servo control parameters.
const int SERVO_STOP = 90;        // Command value used to stop the servo.
const int SERVO_ROTATE_FWD = 120; // Command value for forward rotation. Adjust if the direction is reversed.

// Configured duration intended to approximate a 60-degree servo movement.
// Fine-tune this value according to the actual hardware test result.
const unsigned long SERVO_ROTATION_TIME_60 = 200;
const unsigned long DROP_WAIT_TIME = 1000; // Wait time for material to fall by gravity, in milliseconds.

// ASR voice recognition module.
#define ASR_I2C_ADDR 0x34
#define ASR_RESULT_ADDR 100

// ESP module connection.
#define ESP_SERIAL Serial2        // ESP01S is connected to Serial2 (TX2 = 16, RX2 = 17).

// BSEC EEPROM storage settings.
#define EEPROM_MAGIC_BYTE_ADDR 0  // Magic byte address used to check whether valid data has been stored.
#define EEPROM_MAGIC_BYTE 0xAA    // Custom marker byte.
#define EEPROM_STATE_ADDR 1       // Start address for storing the BSEC calibration state array.
#define EEPROM_SETTINGS_ADDR 200  // Start address for storing user settings.
#define SETTINGS_MAGIC_BYTE 0x5A  // Marker used to check whether EEPROM settings are valid.

// Interrupt flags.
volatile bool k0InterruptTriggered = false;
volatile unsigned long lastInterruptTime = 0;
volatile int encoderDirection = 0; // Encoder rotation direction, 1 or -1.
volatile bool encoderBtnTriggered = false; // Encoder button trigger flag.


int menuCursor = 0;    // Menu cursor, range 0 to 5.
unsigned long lastInteractionTime = 0;     // Last user interaction time, used for timeout handling.

// Non-blocking servo state machine variables.
uint8_t servoPhase = 0; // 0: idle, 1: output open, 2: output close, 3: input open, 4: input close.
unsigned long servoTimer = 0;

// Global system state structure.
struct SystemState {
    // Sensor data.
    float temperature = 0.0;
    float humidity = 0.0;
    float iaq = 0.0;
    uint8_t iaqAccuracy = 0;
    String waterLevel = "normal";

    // Peltier temperature and actual hardware control.
    float peltierTemp = 0.0;     // Peltier hot-side temperature.
    int actualPwmLevel = 0;      // Actual PWM level after temperature-based power limiting.

    // Control states.
    String uvLampState = "off";
    String autoDehumidState = "off";
    float targetHumidity = 65.0;
    int dehumidifierLevel = 0; // Target dehumidifier level, 0 to 100%.

    // Desiccant replacement state.
    String silicaAutoEnabled = "off";
    int silicaChangeInterval = 24;

    // Runtime flags.
    bool settingsChanged = false;
    bool isSleeping = false;
};

struct SavedSettings {
    uint8_t magicByte;
    float targetHumidity;
    bool autoDehumidState;
    bool uvLampState;
    uint8_t dehumidifierLevel;
};

#endif // HARDWARE_CONFIG_H
