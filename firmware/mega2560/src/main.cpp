#include <Arduino.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Servo.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <EEPROM.h>
#include "bsec.h"
#include "HardwareConfig.h"

// Object instances
DeviceAddress peltierAddress;
bool isPeltierFound = false;
SystemState sysState;
Bsec iaqSensor;
OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18b20(&oneWire);
Servo servoOut;
Servo servoIn;
SavedSettings currentSettings;
// TFT display instance using hardware SPI by default. Mega2560 uses pin 51 for MOSI and pin 52 for SCK.
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS_Pin, TFT_DC_Pin, TFT_RST_Pin);
// UI state machine and menu variables
enum UIState { UI_MAIN, UI_MENU, UI_EDIT };
UIState currentUIState = UI_MAIN;
SystemState tempState; // Temporary state used for menu preview. It only overwrites sysState after saving.



bool bsecStateSaved = false;      // Tracks whether the full-accuracy BSEC state has already been saved during this boot.
// PID controller class
class PIDController {
public:
    float Kp, Ki, Kd;
    float setpoint;
    float _integral = 0;
    float _last_error = 0;
    float output_min = 0, output_max = 100;

    PIDController(float p, float i, float d, float sp) : Kp(p), Ki(i), Kd(d), setpoint(sp) {}

    float update(float current_value) {
        float error = setpoint - current_value;
        _integral += error;
        if (_integral > 30) _integral = 30;
        if (_integral < -30) _integral = -30;

        float derivative = error - _last_error;
        float output = (Kp * error) + (Ki * _integral) + (Kd * derivative);
        _last_error = error;

        if (output > output_max) return output_max;
        if (output < output_min) return output_min;
        return output;
    }
};

// Humidity PID. Requests power based on the measured humidity.
// Original PID gains are kept: P = -4.0, I = -0.2, D = -0.1.
PIDController humiPid(-4.0, -0.2, -0.1, sysState.targetHumidity);

// Temperature protection PID. Limits power to prevent overheating.
// A 60 C setpoint controls the maximum allowed output.
PIDController tempPid(15.0, 1.0, 0.5, 60.0);

// Function declarations
void updateHardwareState();
void processJSONCommands();
void sendStatusToESP();
void performMaterialChange();
void initTFT();
void updateTFT();
void printDebugInfo();
uint8_t readASRResult();
void handleVoiceCommand(uint8_t result_index);
void checkIaqSensorStatus();
void saveBsecState();
void loadBsecState();
void loadUserSettings();
void saveUserSettings();
void K0_ISR();
void processMaterialChangeTask();
void handleMenuLogic();
void Encoder_Rot_ISR();
void Encoder_Btn_ISR();


void setup() {
    Serial.begin(115200);      // Serial port for PC debugging.
    ESP_SERIAL.begin(19200);  // Communication with the ESP8266.
    Wire.begin();
    // Check whether the ASR voice module is online.
    Wire.beginTransmission(ASR_I2C_ADDR);
    if (Wire.endTransmission() == 0) {
        Serial.println("[SYS] ASR Module found at 0x34.");
    } else {
        Serial.println("[ERR] ASR Module NOT found at I2C address 0x34!");
    }
    // 1. Initialize the TFT display.
    initTFT();

    // 2. Initialize sensors.
    pinMode(DS18B20_PIN, INPUT_PULLUP); // Enable the internal pull-up resistor.
    ds18b20.begin();

    // Read and store the sensor address during the quiet startup period.
    isPeltierFound = ds18b20.getAddress(peltierAddress, 0);
    if (isPeltierFound) {
        Serial.println("[SYS] DS18B20 (Peltier) Sensor Found!");
        ds18b20.setResolution(peltierAddress, 11); // Use 11-bit resolution. Conversion takes about 375 ms and reduces blocking time.
    } else {
        Serial.println("[ERR] DS18B20 NOT Found! Check wiring.");
    }

    ds18b20.setWaitForConversion(false); // Enable non-blocking conversion mode.
    ds18b20.requestTemperatures();       // Start the first temperature conversion in advance.
    iaqSensor.begin(0x77, Wire);
    bsec_virtual_sensor_t sensorList[3] = {
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE, // Use heat-compensated temperature for display.
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
    BSEC_OUTPUT_IAQ
};
    iaqSensor.updateSubscription(sensorList, 3, BSEC_SAMPLE_RATE_LP);
    loadBsecState();
    loadUserSettings();
    // 3. Initialize actuator pins.
    pinMode(DEHUMIDIFIER_PWM_PIN, OUTPUT);
    pinMode(UV_LAMP_PIN, OUTPUT);
    pinMode(FAN_PIN, OUTPUT);
    pinMode(WATER_LEVEL_PIN, INPUT);
    // 4. Initialize buttons and display backlight.
    pinMode(K0_Pin, INPUT_PULLUP); // Enable the internal pull-up. The pin reads LOW when pressed.
    attachInterrupt(digitalPinToInterrupt(K0_Pin), K0_ISR, FALLING);
    pinMode(TFT_BL_Pin, OUTPUT);
    digitalWrite(TFT_BL_Pin, HIGH); // Turn on the display backlight at startup.
    pinMode(Encoder_A_Pin, INPUT_PULLUP);
    pinMode(Encoder_B_Pin, INPUT_PULLUP);
    pinMode(Encoder_Push_Pin, INPUT_PULLUP);

    // Mega2560 pin 2 maps to external interrupt 0.
    attachInterrupt(digitalPinToInterrupt(Encoder_A_Pin), Encoder_Rot_ISR, FALLING);
    // Pin 19 maps to external interrupt 4.
    attachInterrupt(digitalPinToInterrupt(Encoder_Push_Pin), Encoder_Btn_ISR, FALLING);

    // Attach both material-change servos.
    servoOut.attach(SERVO_OUT_PIN);
    servoIn.attach(SERVO_IN_PIN);
    servoOut.write(SERVO_STOP); // Set the servo to 90 at initialization so it stays stopped.
    servoIn.write(SERVO_STOP);  // Set the servo to 90 at initialization so it stays stopped.

    sysState.targetHumidity=65;
    Serial.println("[SYS] Mega2560 Initialized. Waiting for ESP8266...");
}

void loop() {

    // 1. Respond to the hardware interrupt flag as quickly as possible.
    if (k0InterruptTriggered) {
        k0InterruptTriggered = false; // Clear the flag immediately after receiving the event.

        sysState.isSleeping = !sysState.isSleeping; // Toggle the sleep state.

        if (sysState.isSleeping) {
            Serial.println("[SYS] INT Button: Entering SLEEP mode");
        } else {
            Serial.println("[SYS] INT Button: WAKING UP");
        }
        updateHardwareState(); // Apply the hardware state change immediately.
    }

    // 2. Read the DS18B20 temperature in non-blocking mode once per second.
    static unsigned long lastTempTimer = 0;
    if (millis() - lastTempTimer > 1000) {
        // Read the result from the previous conversion request.
        float tempC = ds18b20.getTempCByIndex(0);
        if (tempC != DEVICE_DISCONNECTED_C && tempC != 85.0) { // 85.0 is the sensor's default power-on error value.
            // sysState.temperature = tempC;
        }

        // Start the next conversion request. It returns immediately because non-blocking mode was enabled in setup.
        ds18b20.requestTemperatures();
        lastTempTimer = millis();
    }
    // 2. Read BME680 data in non-blocking mode.
    if (iaqSensor.run()) {
        sysState.humidity = iaqSensor.humidity;
        sysState.temperature = iaqSensor.temperature;
        sysState.iaq = iaqSensor.iaq;
        sysState.iaqAccuracy = iaqSensor.iaqAccuracy; // Store the latest IAQ accuracy value for display.
        Serial.print("Current accurancy: ");Serial.println(sysState.iaqAccuracy);

        // Save the BSEC state to EEPROM once the highest accuracy level has been reached during this boot.
        if (sysState.iaqAccuracy >= 3 && !bsecStateSaved) {
            saveBsecState();
            bsecStateSaved = true; // Save only once to avoid unnecessary EEPROM wear.
        }
    } else {
        checkIaqSensorStatus();
    }

    // 3. Check the water level.
    int currentWaterLevel = analogRead(WATER_LEVEL_PIN); // Read the current analog value from 0 to 1023.
    // Treat the tank as full when the analog value reaches or exceeds the configured threshold.
    if (currentWaterLevel >= WATER_LEVEL_THRESHOLD) {
        sysState.waterLevel = "full";
        sysState.autoDehumidState = "off";
        sysState.dehumidifierLevel = 0;
    } else {
        sysState.waterLevel = "normal";
    }

    // 2. Schedule environmental reading and dual-PID control about once per second.
    static unsigned long lastControlTimer = 0;
    if (millis() - lastControlTimer > 1000) {

        // A. Read the Peltier temperature.
        if (isPeltierFound) {
            // 1. Read the addressed sensor directly to avoid bus conflicts.
            float tempC = ds18b20.getTempC(peltierAddress);

            if (tempC != DEVICE_DISCONNECTED_C && tempC != 85.0) {
                sysState.peltierTemp = tempC;
            } else {
                Serial.println("[DS18B20] Bus Crash Detected! Restarting 1-Wire...");
                // If the read fails, reset the lower-level 1-Wire search state immediately.
                oneWire.reset_search();
            }

            // 2. Start the next conversion by using the Skip ROM broadcast command.
            // The broadcast command is shorter than an addressed command and is less likely to be interrupted by interference.
            ds18b20.requestTemperatures();
        }

        // B. Run the humidity PID only in automatic mode and when the tank is not full.
        if (sysState.autoDehumidState == "on" && sysState.waterLevel != "full") {
            sysState.dehumidifierLevel = (int)humiPid.update(sysState.humidity);
        }

        // C. Run the temperature protection PID continuously with a 60 C safety target.
        float maxAllowedLevel = tempPid.update(sysState.peltierTemp);

        // D. Determine the final output.
        if (sysState.waterLevel == "full") {
            sysState.actualPwmLevel = 0;
        } else {
            // Core logic: actual output is the smaller value between requested strength and temperature-limited strength.
            sysState.actualPwmLevel = (int)min((float)sysState.dehumidifierLevel, maxAllowedLevel);
        }

        lastControlTimer = millis();
    }

    // 5. Update the low-level hardware state.
    updateHardwareState();

    // 6. Communicate with the ESP8266.
    processJSONCommands();
    // 6.5 Poll the ASR voice-recognition module every 200 ms.
    static unsigned long lastAsrTimer = 0;
    if (millis() - lastAsrTimer > 200) {
        uint8_t asrResult = readASRResult();
        if (asrResult > 0) {
            handleVoiceCommand(asrResult);
            sysState.settingsChanged = true; // Mark the settings as dirty so they will be saved.
        }
        lastAsrTimer = millis();
    }
    // Handle the non-blocking servo task in every loop cycle.
    processMaterialChangeTask();

    // Handle the menu UI logic.
    handleMenuLogic();
    // 7. Every 2 seconds, update the display, print debug information, and send data to the ESP8266.
    static unsigned long lastUpdateTimer = 0;
    if (millis() - lastUpdateTimer > 2000) {

        // Refresh live data only on the main screen to avoid flicker or conflicts with the menu screen.
        if (currentUIState == UI_MAIN) {
            updateTFT();
        }

        printDebugInfo();
        sendStatusToESP();

        // Handle the save logic in one place.
        if (sysState.settingsChanged) {
            saveUserSettings();
            sysState.settingsChanged = false; // Reset the flag after saving.
        }

        lastUpdateTimer = millis();
    }
}

// Function implementations

void initTFT() {
    tft.init(240, 320);
    tft.setRotation(3); // Use landscape orientation.
    tft.fillScreen(ST77XX_BLACK);

    // Print static labels that do not change.
    tft.setTextSize(3);
    tft.setTextColor(ST77XX_WHITE);

    tft.setCursor(10, 15);  tft.print("Temp:");
    tft.setCursor(10, 55);  tft.print("Humi:");
    tft.setCursor(10, 95); tft.print("IAQ:");
    tft.setCursor(10, 135); tft.print("UV:");
    tft.setCursor(10, 175); tft.print("Inten:");
}

void updateTFT() {
    tft.setTextSize(3);

    // Use tft.setTextColor with foreground and background colors to prevent number flicker.
    // Append spaces to overwrite leftover characters from previous values.
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);

    tft.setCursor(120, 15);
    tft.print(sysState.temperature, 1);
    // Print the Celsius unit in separate steps.
    tft.print(" ");           // Print the space between the number and the unit symbol.
    tft.print((char)247);     // Print the degree symbol provided by Adafruit_GFX.
    tft.print("C   ");        // Print C and extra spaces to overwrite leftover characters.

    tft.setCursor(120, 55);
    tft.print(sysState.humidity, 1); tft.print(" %   ");

    tft.setCursor(120, 95);
    if (sysState.iaqAccuracy == 0) {
        // Show a yellow Preheating message during warm-up.
        tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
        tft.print("Preheating");
    } else {
        // After warm-up, display the normal numeric value.
        tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
        tft.print(sysState.iaq, 0);
        tft.print("         "); // Overwrite old characters.
    }

    tft.setCursor(120, 135);
    if (sysState.uvLampState == "on") {
        tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
        tft.print("ON   ");
    } else {
        tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
        tft.print("OFF  ");
    }
   tft.setCursor(120, 175);

    // 1. Check water-level protection first because it has the highest priority.
    if (sysState.waterLevel == "full") {
        tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
        tft.print(sysState.actualPwmLevel); // This is usually 0 in this state.
        tft.print("% (full)                "); // Show the full-tank protection status.
    }
    // 2. Then check overheat protection. The temperature PID may limit the actual output below the requested output.
    else if (sysState.actualPwmLevel < sysState.dehumidifierLevel) {
        tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
        tft.print(sysState.actualPwmLevel);
        tft.print("% (HOT)                 "); // Show the overheat protection status.
    }
    // 3. Normal automatic mode.
    else if (sysState.autoDehumidState == "on") {
        tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
        tft.print(sysState.actualPwmLevel);
        tft.print("% (Auto)                ");
    }
    // 4. Manual mode.
    else {
        if (sysState.actualPwmLevel == 0) {
            tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
            tft.print("OFF                  ");
        } else {
            tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
            tft.print(sysState.actualPwmLevel);
            tft.print("%                    ");
        }
    }
}


void printDebugInfo() {
    Serial.print("[DEBUG] T:"); Serial.print(sysState.temperature, 1);
    Serial.print(" | H:"); Serial.print(sysState.humidity, 1);
    Serial.print(" | IAQ:"); Serial.print(sysState.iaq, 0);
    // Print the Peltier temperature and actual output.
    Serial.print(" | PelTemp:"); Serial.print(sysState.peltierTemp, 1);
    Serial.print(" | ActPWM:"); Serial.print(sysState.actualPwmLevel);
    Serial.println("%");
}

void updateHardwareState() {
    // If sleep mode is active, force all hardware outputs and the backlight off.
    if (sysState.isSleeping) {
        analogWrite(DEHUMIDIFIER_PWM_PIN, 0);
        digitalWrite(UV_LAMP_PIN, LOW);
        digitalWrite(FAN_PIN, LOW);
        digitalWrite(TFT_BL_Pin, LOW); // Turn off the display backlight.
        return; // Exit the function and skip the normal logic below.
    }

    // Normal operation logic when the system is not sleeping.
    digitalWrite(TFT_BL_Pin, HIGH); // Keep the display backlight on.

    // Map the actual low-level output strength.
    int pwmValue = map(sysState.actualPwmLevel, 0, 100, 0, 255);
    analogWrite(DEHUMIDIFIER_PWM_PIN, pwmValue);
    digitalWrite(UV_LAMP_PIN, sysState.uvLampState == "on" ? HIGH : LOW);

    // Safety fan logic.
    // Force the fan on when power is being output, the UV lamp is on, or the Peltier plate is still hotter than 45 C.
    if (sysState.actualPwmLevel > 0 || sysState.uvLampState == "on" || sysState.peltierTemp > 45.0) {
        digitalWrite(FAN_PIN, HIGH);
    } else {
        digitalWrite(FAN_PIN, LOW);
    }
}

void performMaterialChange() {
    if (servoPhase == 0) {
        Serial.println("[INFO] Changing Material Started...");
        servoPhase = 1;
        servoOut.write(SERVO_ROTATE_FWD);  // Start rotating the output servo.
        servoTimer = millis();

        // Show a message when the main screen is active.
        if (currentUIState == UI_MAIN) {
            tft.setTextSize(2);
            tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
            tft.setCursor(10, 215);
            tft.print("Changing Material...");
        }
    }
}
void processMaterialChangeTask() {
    if (servoPhase == 0) return; // Return immediately when the task is idle.

    unsigned long currentTime = millis();

    // Phase 1: the output port is rotating. Wait until it has rotated about 60 degrees.
    if (servoPhase == 1 && currentTime - servoTimer >= SERVO_ROTATION_TIME_60) {
        servoOut.write(SERVO_STOP); // Stop the output servo.
        servoPhase = 2;
        servoTimer = currentTime;
    }
    // Phase 2: wait DROP_WAIT_TIME milliseconds for old material to fall by gravity.
    else if (servoPhase == 2 && currentTime - servoTimer >= DROP_WAIT_TIME) {
        servoIn.write(SERVO_ROTATE_FWD); // Start rotating the input servo.
        servoPhase = 3;
        servoTimer = currentTime;
    }
    // Phase 3: the input port is rotating. Wait until it has rotated about 60 degrees.
    else if (servoPhase == 3 && currentTime - servoTimer >= SERVO_ROTATION_TIME_60) {
        servoIn.write(SERVO_STOP); // Stop the input servo.
        servoPhase = 4;
        servoTimer = currentTime;
    }
    // Phase 4: finish the sequence.
    else if (servoPhase == 4) {
        Serial.println("[INFO] Material Change Completed!");
        servoPhase = 0; // The sequence is complete.

        // Clear the on-screen message.
        if (currentUIState == UI_MAIN) {
            tft.fillRect(0, 210, 320, 30, ST77XX_BLACK);
        }
    }
}
void sendStatusToESP() {
    StaticJsonDocument<256> doc;
    doc["t"] = sysState.temperature;
    doc["h"] = sysState.humidity;
    doc["iaq"] = sysState.iaq;
    doc["w_lvl"] = sysState.waterLevel;
    doc["uv"] = sysState.uvLampState;
    doc["auto"] = sysState.autoDehumidState;
    doc["p_lvl"] = sysState.dehumidifierLevel;

    // Report the current power and screen state to the front end.
    doc["status"] = sysState.isSleeping ? "standby" : "running";

    // Send a string to the ESP when accuracy is 0. Otherwise, send the numeric value.
    if (sysState.iaqAccuracy == 0) {
        doc["iaq"] = "preheating";
    } else {
        doc["iaq"] = sysState.iaq;
    }

    serializeJson(doc, ESP_SERIAL);
    ESP_SERIAL.println();
}
void processJSONCommands() {
    if (ESP_SERIAL.available()) {
        String jsonStr = ESP_SERIAL.readStringUntil('\n');
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, jsonStr);

        if (!error) {
            // Parse standby and running commands from the front end.
            bool isExplicitPowerCmd = false;
            if (doc.containsKey("power")) {
                isExplicitPowerCmd = true;
                String pwrCmd = doc["power"].as<String>();

                if (pwrCmd == "standby" && !sysState.isSleeping) {
                    sysState.isSleeping = true;
                    Serial.println("[SYS] Entering sleep mode (ESP Command)");
                    updateHardwareState(); // Turn off the display and devices immediately.
                }
                else if (pwrCmd == "running" && sysState.isSleeping) {
                    sysState.isSleeping = false;
                    Serial.println("[SYS] Waking up from sleep mode (ESP Command)");
                    updateHardwareState(); // Turn on the display immediately.
                }
            }

            // If a command changes parameters instead of only changing power state, wake the display automatically.
            if (!isExplicitPowerCmd && sysState.isSleeping) {
                sysState.isSleeping = false;
                Serial.println("[SYS] Waking up from sleep mode (ESP Trigger - Param change)");
                updateHardwareState();
            }

            if (doc.containsKey("uv")) sysState.uvLampState = doc["uv"].as<String>();
            if (doc.containsKey("auto")) {
                sysState.autoDehumidState = doc["auto"].as<String>();
                if (sysState.autoDehumidState == "on") humiPid.setpoint = sysState.targetHumidity;
            }
            if (doc.containsKey("target_h")) {
                sysState.targetHumidity = doc["target_h"].as<float>();
                humiPid.setpoint = sysState.targetHumidity;
            }
            if (doc.containsKey("p_lvl") && sysState.autoDehumidState == "off") {
                sysState.dehumidifierLevel = doc["p_lvl"].as<int>();
            }
            if (doc.containsKey("cmd_silica")) {
                if (doc["cmd_silica"].as<String>() == "change_now") {
                    performMaterialChange();
                }
            }
            Serial.println("[RECV] JSON command received and processed.");
            sysState.settingsChanged = true; // Mark the settings as dirty so they will be saved.
        }
    }
}
uint8_t readASRResult() {
    // 1. Write the register address to be read, 100.
    Wire.beginTransmission(ASR_I2C_ADDR);
    Wire.write(ASR_RESULT_ADDR);
    if (Wire.endTransmission() != 0) {
        // A transmission failure indicates a bus error or disconnected module.
        return 0;
    }

    // 2. Request one byte of data from this address.
    Wire.requestFrom((uint8_t)ASR_I2C_ADDR, (uint8_t)1);
    if (Wire.available()) {
        return Wire.read();
    }

    return 0;
}

void handleVoiceCommand(uint8_t result_index) {
    if (sysState.isSleeping) {
        sysState.isSleeping = false;
        Serial.println("[SYS] Waking up from sleep mode (Voice Trigger)");
        updateHardwareState(); // Turn on the display immediately.
    }
    Serial.print("[ASR]: ");
    Serial.println(result_index);

    if (result_index == 3) {
        Serial.println("[ASR]");
    }
    else if (result_index >= 1 && result_index <= 6) {
        Serial.println("[ASR]");
        sysState.autoDehumidState = "on";
        humiPid.setpoint = sysState.targetHumidity;
    }
    else if (result_index >= 7 && result_index <= 9) {
        Serial.println("[ASR]");
        sysState.autoDehumidState = "off";
        sysState.dehumidifierLevel = 0;
    }
    else if (result_index >= 10 && result_index <= 11) {
        Serial.println("[ASR]");
        sysState.uvLampState = "on";
    }
    else if (result_index == 12) {
        Serial.println("[ASR]");
        sysState.uvLampState = "off";
    }else if (result_index == 14) {
        Serial.println("[ASR]");
        performMaterialChange();
    }
    // Only sysState needs to be changed here.
    // updateHardwareState in the main loop will read the updated sysState on the next cycle and synchronize the relay and PWM output, so direct GPIO calls are not needed here.
}
void checkIaqSensorStatus() {
    if (iaqSensor.bsecStatus != BSEC_OK) {
        if (iaqSensor.bsecStatus < BSEC_OK) {
            Serial.println("[BSEC ERR] BSEC Software ERROR code: " + String(iaqSensor.bsecStatus));
        } else {
            Serial.println("[BSEC WARN] BSEC software warning code: " + String(iaqSensor.bsecStatus));
        }
    }

    if (iaqSensor.bme68xStatus != BME68X_OK) {
        if (iaqSensor.bme68xStatus < BME68X_OK) {
            Serial.println("[BME680 ERR] Hardware communication ERROR code: " + String(iaqSensor.bme68xStatus));
        } else {
            Serial.println("[BME680 WARN] Hardware communication WARNING code: " + String(iaqSensor.bme68xStatus));
        }
    }
}
void loadBsecState() {
    if (EEPROM.read(EEPROM_MAGIC_BYTE_ADDR) == EEPROM_MAGIC_BYTE) {
        uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE];
        for (int i = 0; i < BSEC_MAX_STATE_BLOB_SIZE; i++) {
            bsecState[i] = EEPROM.read(EEPROM_STATE_ADDR + i);
        }
        iaqSensor.setState(bsecState);
        Serial.println("[BSEC] EEPROM Calibration State Loaded!");
    } else {
        Serial.println("[BSEC] No valid state in EEPROM, starting fresh.");
    }
}

// Save the BSEC state to EEPROM.
void saveBsecState() {
    uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE];
    iaqSensor.getState(bsecState);

    // EEPROM.update avoids rewriting unchanged data and helps extend EEPROM life.
    for (int i = 0; i < BSEC_MAX_STATE_BLOB_SIZE; i++) {
        EEPROM.update(EEPROM_STATE_ADDR + i, bsecState[i]);
    }
    EEPROM.update(EEPROM_MAGIC_BYTE_ADDR, EEPROM_MAGIC_BYTE);
    Serial.println("[BSEC] Full Accuracy State Saved to EEPROM!");
}
void loadUserSettings() {
    EEPROM.get(EEPROM_SETTINGS_ADDR, currentSettings);

    // Verify that stored user settings are valid by checking the magic byte.
    if (currentSettings.magicByte == SETTINGS_MAGIC_BYTE) {
        sysState.targetHumidity = currentSettings.targetHumidity;
        sysState.autoDehumidState = currentSettings.autoDehumidState ? "on" : "off";
        sysState.uvLampState = currentSettings.uvLampState ? "on" : "off";
        sysState.dehumidifierLevel = currentSettings.dehumidifierLevel;

        // If automatic mode is loaded at startup, synchronize the PID setpoint.
        if (sysState.autoDehumidState == "on") {
            humiPid.setpoint = sysState.targetHumidity;
        }
        Serial.println("[EEPROM] User settings loaded successfully.");
    } else {
        Serial.println("[EEPROM] No valid user settings found, using default values.");
    }
}

void saveUserSettings() {
    currentSettings.magicByte = SETTINGS_MAGIC_BYTE;
    currentSettings.targetHumidity = sysState.targetHumidity;
    currentSettings.autoDehumidState = (sysState.autoDehumidState == "on");
    currentSettings.uvLampState = (sysState.uvLampState == "on");
    currentSettings.dehumidifierLevel = sysState.dehumidifierLevel;

    // EEPROM.put internally avoids unnecessary writes when the data is unchanged.
    EEPROM.put(EEPROM_SETTINGS_ADDR, currentSettings);
    Serial.println("[EEPROM] User settings saved to EEPROM.");
}
void K0_ISR() {
    unsigned long currentTime = millis(); // millis does not increment inside the ISR, but reading the current value is safe.
    // 200 ms software debounce cooldown.
    if (currentTime - lastInterruptTime > 200) {
        k0InterruptTriggered = true; // Set the flag.
        lastInterruptTime = currentTime;
    }
}
// Encoder rotation interrupt on pin 2.
void Encoder_Rot_ISR() {
    unsigned long currentTime = millis();
    static unsigned long lastRotTime = 0;
    if (currentTime - lastRotTime > 100) { // 100 ms debounce.
        // Read phase B to determine the rotation direction.
        if (digitalRead(Encoder_B_Pin) == LOW) {
            encoderDirection = -1;  // Clockwise rotation.
        } else {
            encoderDirection = 1; // Counterclockwise rotation.
        }
        lastRotTime = currentTime;
    }
}

// Encoder button interrupt on pin 19.
void Encoder_Btn_ISR() {
    unsigned long currentTime = millis();
    static unsigned long lastBtnTime = 0;
    if (currentTime - lastBtnTime > 200) { // 200 ms debounce.
        encoderBtnTriggered = true;
        lastBtnTime = currentTime;
    }
}
// Partial refresh. Update only the cursor.
void updateMenuCursor(int oldIdx, int newIdx) {
    // 1. Erase the old cursor.
    // Since the cursor is no longer text, erase the old triangle by drawing a black rectangle instead of printing spaces.
    if (oldIdx >= 0 && oldIdx <= 5 && oldIdx != newIdx) {
        int y_base = 20 + oldIdx * 35;
        // Draw a 15 by 16 black filled rectangle at x = 10 for precise erasing.
        tft.fillRect(10, y_base, 15, 16, ST77XX_BLACK);
    }

    // 2. Draw the new cursor as a filled triangle.
    if (newIdx >= 0 && newIdx <= 5) {
        int y_base = 20 + newIdx * 35;

        // Choose the triangle color based on the current UI state.
        uint16_t cursorColor = (currentUIState == UI_EDIT) ? ST77XX_YELLOW : ST77XX_GREEN;

        // Draw a right-pointing filled triangle.
        // fillTriangle(x0, y0, x1, y1, x2, y2, color)
        tft.fillTriangle(
            10, y_base,         // Upper-left vertex.
            10, y_base + 14,    // Lower-left vertex.
            22, y_base + 7,     // Right tip.
            cursorColor
        );
    }
}

// Partial refresh. Update only the selected value.
void updateMenuValue(int idx) {
    tft.setTextSize(2);
    tft.setCursor(255, 20 + idx * 35); // Align values at a fixed position on the right side.

    // Handle the disabled color for Target.
    uint16_t color = ST77XX_WHITE;
    if (idx == 1 && tempState.autoDehumidState == "off") color = 0x7BEF; // Dark gray.

    tft.setTextColor(color, ST77XX_BLACK);

    String valStr = "";
    switch(idx) {
        case 0: valStr = (tempState.autoDehumidState == "on") ? "ON   " : "OFF  "; break;
        case 1: valStr = String((int)tempState.targetHumidity) + "%  "; break;
        case 2: valStr = String(tempState.dehumidifierLevel) + "%  "; break;
        case 3: valStr = (tempState.uvLampState == "on") ? "ON   " : "OFF  "; break;
        case 4: valStr = "     "; break;
        case 5: valStr = "     "; break;
    }
    tft.print(valStr); // Print the value and overwrite old pixels.
}

// Full redraw. Call this only when entering the menu.
void drawMenuFull() {
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextSize(2);

    const char* menuTitles[6] = {
        "1. Auto Mode:      ",
        "2. --Target:       ",
        "3. Manual Strength:",
        "4. UV Lamp:        ",
        "5. Change Material ",
        "6. Exit & Save     "
    };

    for (int i = 0; i < 6; i++) {
        tft.setCursor(25, 20 + i * 35); // Shift the title right to leave space for the cursor.
        if (i == 1 && tempState.autoDehumidState == "off") {
            tft.setTextColor(0x7BEF, ST77XX_BLACK); // Gray out Target.
        } else {
            tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
        }
        tft.print(menuTitles[i]);
        updateMenuValue(i); // Draw the initial value for this item.
    }
    updateMenuCursor(-1, menuCursor); // Draw the initial cursor.
}

void handleMenuLogic() {
    // 1. On the main screen, ignore and clear any encoder rotation input.
    if (currentUIState == UI_MAIN) {
        encoderDirection = 0;
    }

    // 2. Exit the menu after 15 seconds of inactivity.
    if (currentUIState != UI_MAIN && (millis() - lastInteractionTime > 15000)) {
        Serial.println("[UI] Menu Timeout, discarding changes.");
        currentUIState = UI_MAIN;
        tft.fillScreen(ST77XX_BLACK);
        initTFT();
        return;
    }

    // 3. Handle encoder rotation.
    if (encoderDirection != 0) {
        lastInteractionTime = millis();
        int dir = encoderDirection; // Store the direction temporarily.
        encoderDirection = 0;       // Clear the direction immediately to prevent repeated input.

        if (currentUIState == UI_MENU) {
            int oldCursor = menuCursor;
            menuCursor += dir;

            if (menuCursor < 0) menuCursor = 5;
            if (menuCursor > 5) menuCursor = 0;

            if (menuCursor == 1 && tempState.autoDehumidState == "off") {
                if (dir > 0) menuCursor = 2;
                else menuCursor = 0;
            }
            updateMenuCursor(oldCursor, menuCursor); // Refresh only the cursor movement.
        }
        else if (currentUIState == UI_EDIT) {
            if (menuCursor == 0) { // Auto mode toggle.
                tempState.autoDehumidState = (tempState.autoDehumidState == "on") ? "off" : "on";

                // If the Auto state changes, refresh the Target title color accordingly.
                tft.setCursor(25, 20 + 1 * 35);
                tft.setTextColor(tempState.autoDehumidState == "on" ? ST77XX_WHITE : 0x7BEF, ST77XX_BLACK);
                tft.print("2. --Target:");
                updateMenuValue(1);
            }
            else if (menuCursor == 1) { // Target humidity.
                tempState.targetHumidity += dir * 5.0;
                if (tempState.targetHumidity > 95) tempState.targetHumidity = 95;
                if (tempState.targetHumidity < 20) tempState.targetHumidity = 20;
            }
            else if (menuCursor == 2) { // Manual PWM.
                tempState.dehumidifierLevel += dir * 5;
                if (tempState.dehumidifierLevel > 100) tempState.dehumidifierLevel = 100;
                if (tempState.dehumidifierLevel < 0) tempState.dehumidifierLevel = 0;

                // When manual PWM is adjusted, turn off Auto mode and refresh the UI accordingly.
                if (tempState.dehumidifierLevel > 0 && tempState.autoDehumidState == "on") {
                    tempState.autoDehumidState = "off";
                    updateMenuValue(0); // Refresh the Auto display to show OFF.
                    // Gray out Target.
                    tft.setCursor(25, 20 + 1 * 35);
                    tft.setTextColor(0x7BEF, ST77XX_BLACK);
                    tft.print("2. --Target:");
                    updateMenuValue(1);
                }
            }
            else if (menuCursor == 3) { // UV lamp toggle.
                tempState.uvLampState = (tempState.uvLampState == "on") ? "off" : "on";
            }

            updateMenuValue(menuCursor); // Refresh only the value currently being edited.
        }
    }

    // 4. Handle the encoder button.
    if (encoderBtnTriggered) {
        encoderBtnTriggered = false;
        lastInteractionTime = millis();

        if (currentUIState == UI_MAIN) {
            currentUIState = UI_MENU;
            tempState = sysState;
            menuCursor = 0;
            drawMenuFull(); // Redraw the full screen only once when entering the menu.
        }
        else if (currentUIState == UI_MENU) {
            if (menuCursor >= 0 && menuCursor <= 3) {
                currentUIState = UI_EDIT;
                updateMenuCursor(menuCursor, menuCursor); // Change the cursor to yellow.
            }
            else if (menuCursor == 4) {
                performMaterialChange();
            }
            else if (menuCursor == 5) {
                sysState = tempState;
                sysState.settingsChanged = true;
                if (sysState.autoDehumidState == "on") {
                    humiPid.setpoint = sysState.targetHumidity;
                }

                currentUIState = UI_MAIN;
                tft.fillScreen(ST77XX_BLACK);
                initTFT();
                updateHardwareState();
            }
        }
        else if (currentUIState == UI_EDIT) {
            currentUIState = UI_MENU;
            updateMenuCursor(menuCursor, menuCursor); // Confirm the change and turn the cursor back to green.
        }
    }
}
