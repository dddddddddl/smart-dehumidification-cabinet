# Browser dashboard

This folder is the **actual frontend source** supplied inside the ESP8266 project's former `data/` directory. It contains `index.html`, `app.css`, `app.js` and all 15 SVG icons. No frontend framework, npm build or cloud service is required.

## Read the interface

- `index.html`: running/standby pages, environmental cards, controls and target-humidity picker.
- `app.css`: responsive layout, transitions and control styling.
- `app.js`: `/state` polling every second, view transitions, control requests and local browser state.
- `assets/`: SVG icons referenced by the page.

The page displays temperature, humidity, IAQ/warm-up state, water level, UV, automatic mode, target humidity and a discrete manual power selector (0/40/70/100%). A material-change request and standby/start controls use the same routes as the firmware fallback page.

## Preview without an ESP8266

From the repository root:

```bash
python3 tools/preview_server.py
```

Open **http://127.0.0.1:8080**. The helper serves these unchanged web assets with an explicit synthetic-data banner and in-memory command responses. It never opens a device connection. See [tools/README.md](../tools/README.md).

Opening the HTML file directly or using a static-only server will not supply `/state` or command routes. For actual hardware use, flash both the ESP8266 firmware and LittleFS image as described in the [firmware guide](../firmware/esp8266/README.md).

## State semantics

The original browser stores the last running/standby page in `localStorage` for transitions. The ESP8266 also updates some control values optimistically. Therefore, a successful HTTP response or visual toggle is not proof of a completed physical operation. Requested power differs from the Mega's thermally limited actual PWM.

The last-update display uses the timestamp of an **effective state change**, while the online timeout uses the last received frame. An unchanged but regularly received frame can remain online even while its displayed change age increases. No cloud synchronization or historical data backend is included.
