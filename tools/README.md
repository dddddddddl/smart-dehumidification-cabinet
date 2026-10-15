# Preview and serial tools

## Software-only dashboard preview

```bash
python3 tools/preview_server.py --port 8080
```

The standard-library HTTP server binds to `127.0.0.1`. It serves the real dashboard with fixed, explicitly synthetic environmental values and in-memory command responses. It does not model humidity dynamics, thermal limiting, voice recognition or real relay feedback. It was added for repository review; it is not the deployed ESP8266 implementation.

## Original serial simulator

Install its one dependency:

```bash
python3 -m pip install -r tools/requirements.txt
python3 tools/mock_serial.py --port /dev/cu.usbserial-YOUR_ADAPTER
```

On Windows, use a port such as `COM3`. The default link speed is 19200 baud; `--baud` can override it.

This tool opens a real USB-to-TTL serial port and simulates Mega status for an attached ESP8266. It is an alternative to the main controller, not something to connect to the same active UART transmitter. Use hardware-appropriate power/logic levels.

The source already included IAQ warm-up variables but never applied them; preparation completed the advertised transition from `preheating` to one synthetic numeric value after 60 seconds. Console text was translated, indentation normalized and portable CLI port selection added. The command semantics and old field aliases remain intact. No serial device was used in validation.
