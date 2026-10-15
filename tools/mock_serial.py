import argparse
import json
import random
import threading
import time

import serial

# ============== Configuration ==============
# Set the USB-to-TTL serial port for the attached ESP8266.
# Windows examples: COM3 or COM4.
# macOS/Linux examples: /dev/cu.usbserial-xxx or /dev/ttyUSB0.
SERIAL_PORT = "COM3"
# The main controller uses ESP_SERIAL.begin(19200).
BAUD_RATE = 19200
# ==========================================

# Simulated IAQ warm-up duration in seconds
PREHEAT_SECONDS = 60


def clamp_percent(value, default=0):
    try:
        n = int(float(value))
    except Exception:
        return default
    return max(0, min(100, n))


def apply_esp_command(command, mock_state):
    """
    Apply received ESP commands using the supplied Mega command semantics.
    """
    explicit_power_cmd = False

    if "power" in command:
        explicit_power_cmd = True
        p = str(command.get("power", "")).lower()
        if p in ("standby", "running"):
            mock_state["status"] = p

    # Parameter requests wake standby unless the request explicitly sets power state.
    if not explicit_power_cmd and mock_state.get("status") == "standby":
        mock_state["status"] = "running"

    if "uv" in command:
        mock_state["uv"] = str(command.get("uv", "off")).lower()

    if "auto" in command:
        mock_state["auto"] = str(command.get("auto", "off")).lower()

    # Canonical target-humidity field
    if "target_h" in command:
        mock_state["target_h"] = clamp_percent(command.get("target_h"), mock_state.get("target_h", 65))

    # Legacy key retained for diagnostic compatibility
    if "target_humidity" in command:
        mock_state["target_h"] = clamp_percent(command.get("target_humidity"), mock_state.get("target_h", 65))

    # Accept p_lvl only in manual mode.
    if "p_lvl" in command and mock_state.get("auto") == "off":
        mock_state["p_lvl"] = clamp_percent(command.get("p_lvl"), mock_state.get("p_lvl", 0))

    # Legacy field compatibility
    if "strength" in command and mock_state.get("auto") == "off":
        mock_state["p_lvl"] = clamp_percent(command.get("strength"), mock_state.get("p_lvl", 0))

    if "cmd_silica" in command and str(command.get("cmd_silica")) == "change_now":
        print("action:  silica change triggered")



def read_from_port(ser, mock_state):
    """
    Background thread: listen for incoming serial commands.
    """
    while True:
        try:
            if ser.in_waiting > 0:
                incoming_data = ser.readline().decode("utf-8", errors="replace").strip()
                if incoming_data:
                    print(f"receive: {incoming_data}")
                    if incoming_data.startswith("{") and incoming_data.endswith("}"):
                        try:
                            cmd = json.loads(incoming_data)
                            apply_esp_command(cmd, mock_state)
                            print(f"apply:   {cmd}")
                        except json.JSONDecodeError:
                            # Ignore debug output and incomplete/non-JSON frames.
                            pass
        except Exception as e:
            print(f"\n[Serial read error]: {e}")
            break
        # Avoid busy waiting.
        time.sleep(0.01)


def main():
    parser = argparse.ArgumentParser(description="Simulate Mega telemetry for an ESP8266 over USB-to-TTL.")
    parser.add_argument("--port", default=SERIAL_PORT)
    parser.add_argument("--baud", type=int, default=BAUD_RATE)
    args = parser.parse_args()
    print(f"Connecting to {args.port} at {args.baud} baud...")

    try:
        # Use a timeout so readline cannot block indefinitely.
        ser = serial.Serial(args.port, args.baud, timeout=0.1)
        print("Connected. Simulating the Arduino Mega 2560...\n")
    except Exception as e:
        print(f"Connection failed. Check the port and whether another process is using it.\nError: {e}")
        return

    # Synthetic telemetry using the original sendStatusToESP field names
    mock_state = {
        "t": 24.5,
        "h": 45.2,
        "iaq": "preheating",
        "w_lvl": "normal",
        "uv": "off",
        "auto": "on",
        "p_lvl": 0,
        "status": "standby",
        "target_h": 65,
    }

    # Receive commands in a daemon thread while sending telemetry periodically.
    read_thread = threading.Thread(target=read_from_port, args=(ser, mock_state))
    read_thread.daemon = True
    read_thread.start()

    last_send_time = 0
    preheat_end_time = time.time() + PREHEAT_SECONDS
    iaq_numeric = random.randint(30, 180)
    iaq_calculated = False
    print(f"IAQ preheating for {PREHEAT_SECONDS}s")

    try:
        # Main thread: send synthetic telemetry every two seconds.
        while True:
            current_time = time.time()
            if not iaq_calculated and current_time >= preheat_end_time:
                mock_state["iaq"] = iaq_numeric
                iaq_calculated = True
            if current_time - last_send_time >= 2.0:
                # Optional variation for a synthetic data display.
                # mock_state["t"] = round(mock_state["t"] + 0.1, 1)

                # Serialize one JSON object per newline-terminated frame.
                payload = json.dumps(mock_state) + '\n'

                # Send the frame.
                ser.write(payload.encode('utf-8'))
                # Log the frame without its trailing newline.
                print(f"send:    {payload.strip()}")

                last_send_time = current_time

            time.sleep(0.05) # Limit the main-loop frequency.

    except KeyboardInterrupt:
        print("\n[Exit] Ctrl+C received.")
    finally:
        if ser.is_open:
            ser.close()
            print("Serial port closed.")


if __name__ == "__main__":
    main()
