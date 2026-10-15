# Serial and HTTP protocol

## Controller link

Both supplied controllers use **19200 baud**. The Mega uses `Serial2`; the ESP8266 uses `Serial`. Frames are newline-terminated JSON objects. The Mega parses with ArduinoJson; the ESP8266 uses a small flat-field extractor.

### Mega to ESP8266

Illustrative packet only: the numbers below show the message format and are not a recorded hardware measurement.

```json
{"t":25.4,"h":62.0,"iaq":42,"w_lvl":"normal","uv":"off","auto":"on","p_lvl":35,"status":"running"}
```

| Field | Meaning |
|---|---|
| `t`, `h` | Environmental temperature and relative humidity |
| `iaq` | BSEC IAQ value, or the string `preheating` when accuracy is zero |
| `w_lvl` | `normal` or `full` |
| `uv`, `auto` | Requested mode strings `on` / `off` |
| `p_lvl` | Requested dehumidifier level; not actual temperature-limited PWM |
| `status` | `running` or `standby` |

The Mega sends status approximately every 2 seconds. It does **not** include target humidity, Peltier temperature, actual PWM or a command acknowledgement identifier in this snapshot.

### ESP8266 to Mega

```json
{"uv":"on"}
{"auto":"off"}
{"target_h":65}
{"p_lvl":70}
{"cmd_silica":"change_now"}
{"power":"standby"}
```

`p_lvl` is acted on by the Mega only when auto mode is off. Parameter requests can wake standby when no explicit `power` key is present. A valid but otherwise unrecognized object can also follow that wake path in the original code. No independent transaction/acknowledgement protocol is defined.

## Browser API

| Route | ESP8266 action |
|---|---|
| `/` | LittleFS dashboard, or built-in fallback HTML |
| `/app.css`, `/app.js`, `/assets/...` | LittleFS static assets |
| `/state` | JSON snapshot of local/last received state |
| `/uv_on`, `/uv_off` | Update optimistic UV state and send request |
| `/auto_on`, `/auto_off` | Update optimistic auto state and send request |
| `/set_strength?level=70` | Send `p_lvl`; original handler does not enforce the UI's discrete level choices |
| `/set_target?value=65` | Clamp local target to 0–100 and send `target_h` |
| `/manual_change` | Request desiccant change |
| `/power_running`, `/power_standby` | Send explicit operating/standby request |

The original control handlers accept GET requests and have no application authentication. Frontend fetch requests identify themselves with `X-Requested-With: fetch`; ordinary navigation is redirected back to `/`. This is a local AP prototype API, not an Internet control service.

## Reading state correctly

`/state` exposes `online`, receive time, state-change age and `lastRawJson` in addition to display/control fields. Online is set on accepted telemetry and cleared after a **5-second receive timeout**.

The state-change clock and receive clock are separate. Unchanged recurring telemetry keeps a device online while the change-age display continues to increase.

UV, automatic-mode and power-level commands set manual-override latches in the supplied ESP8266 source. The latches are not cleared by a subsequent confirmation frame, so their display values can diverge from later Mega telemetry. Target humidity is local because the Mega status frame omits it. A page toggle therefore indicates a requested/local state, not independently confirmed physical execution.
