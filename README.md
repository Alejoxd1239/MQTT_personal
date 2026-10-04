# ESP32 MQTT telemetry node

An ESP32 that streams live telemetry over MQTT and can be controlled remotely from a web dashboard. Built with PlatformIO (Arduino framework), PubSubClient and ArduinoJson. No sensors needed: it uses the board's built-in Wi-Fi radio, capacitive touch and LED, plus a jumper wire as a button. Runs on the Arduino Nano ESP32 (ESP32-S3) and the classic ESP32 DevKit.

![Dashboard showing live readings, the LED turned on and the Wi-Fi signal trace](docs/dashboard.png)

## What it does

- Publishes telemetry as JSON every few seconds: Wi-Fi signal (RSSI), uptime, free heap, touch sensor value, LED state, button count and number of MQTT connections.
- Accepts commands over MQTT: turn the LED on/off/toggle, and change how often data is sent (1 to 3600 s).
- Reports online/offline status using an MQTT **Last Will** message, so the dashboard knows when the device drops off.
- Pressing the button (a wire from D2 to GND on the Nano, the BOOT button on a DevKit) toggles the LED locally and publishes an event, and the dashboard stays in sync.
- Recovers on its own: Wi-Fi and MQTT reconnect in the background with exponential backoff, without blocking the main loop.

## Architecture

```mermaid
flowchart LR
    subgraph ESP32
        S[Touch, button, Wi-Fi RSSI, heap] --> F[Firmware loop]
        F --> L[Onboard LED]
    end
    F -- "MQTT / TCP 1883<br/>telemetry, status, event" --> B[(MQTT broker)]
    B -- "cmd/led, cmd/interval" --> F
    B <-- "MQTT over WebSockets<br/>(wss 8884)" --> D[Web dashboard]
```

### Topics

All topics live under `TOPIC_PREFIX/DEVICE_NAME` (for example `nackademin-iot/alex/node-1`).

| Topic | Direction | Payload |
|---|---|---|
| `.../status` | device → broker | `online` / `offline` (retained, offline via Last Will) |
| `.../telemetry` | device → broker | JSON, see below |
| `.../event` | device → broker | `{"type":"button","count":3,"led":true}` |
| `.../cmd/led` | broker → device | `on`, `off` or `toggle` |
| `.../cmd/interval` | broker → device | seconds, `1` to `3600` |

Example telemetry message:

```json
{"device":"node-1","uptime_s":842,"rssi_dbm":-61,"free_heap":214532,"led":true,
 "interval_s":5,"reconnects":1,"button_count":3,"touch":64}
```

## Getting started

**You need:** an Arduino Nano ESP32 or a classic ESP32 DevKit, a USB cable, a jumper wire, and [PlatformIO](https://platformio.org/) (the VS Code extension is easiest).

| | Arduino Nano ESP32 | ESP32 DevKit |
|---|---|---|
| PlatformIO env | `nano_esp32` (default) | `esp32dev` |
| LED | built-in `L` LED (D13) | GPIO 2 |
| Button | wire from D2 to GND | BOOT button (GPIO 0) |
| Touch | pin A0 | GPIO 4 |

1. Clone the repo and create your config file:
   ```bash
   git clone https://github.com/<your-username>/esp32-mqtt-telemetry.git
   cd esp32-mqtt-telemetry
   cp include/config.example.h include/config.h
   ```
2. Edit `include/config.h`: set your Wi-Fi name and password, and change `TOPIC_PREFIX` to something unique (the public broker is shared with everyone).
3. Build, flash and open the serial monitor:
   ```bash
   pio run -t upload            # add -e esp32dev for a DevKit
   pio device monitor
   ```
4. Open `dashboard/index.html` in a browser, open **Connection settings** and enter your `TOPIC_PREFIX/DEVICE_NAME`. You can also pass it in the URL: `index.html?topic=nackademin-iot/alex/node-1`.

Using a different board? Add an environment to `platformio.ini` and set the pins in `config.h`. On chips without capacitive touch, the touch reading is skipped automatically at compile time.

### Live demo with GitHub Pages

Enable GitHub Pages for the repo (Settings → Pages → deploy from the `main` branch, root folder). The dashboard is then available at `https://<your-username>.github.io/esp32-mqtt-telemetry/dashboard/?topic=<your-topic>`.

### Testing without hardware

`tools/simulate_device.py` speaks exactly the same protocol as the firmware, so you can develop the dashboard without a board plugged in:

```bash
pip install "paho-mqtt>=2.0"
python tools/simulate_device.py --topic nackademin-iot/alex/node-1
```

## Design decisions

- **Non-blocking loop.** Nothing uses `delay()` or waits for the network. Wi-Fi, MQTT, the button and telemetry are all driven by `millis()` timers, so a lost connection never freezes the button or the LED.
- **Exponential backoff** on MQTT reconnects (1 s up to 30 s) to avoid hammering the broker when it is unreachable.
- **Last Will and Testament** plus a retained `online` message, so any client that subscribes later still gets the current status.
- **Secrets stay out of git.** `config.h` is in `.gitignore`; only `config.example.h` is committed. The build stops with a clear error message if `config.h` is missing.
- **State is echoed back.** After each command the device publishes fresh telemetry, so the dashboard shows the device's real state instead of assuming the command worked.

## Limitations and next steps

- The public broker has no authentication or encryption, so anyone who knows the topic can read and send commands. Fine for a demo; not for real data. The next step is TLS with certificates and a private broker.
- Add a real sensor (for example a BME280 for temperature, humidity and pressure).
- Over-the-air (OTA) firmware updates.
- Deep sleep for battery-powered operation.
- Store history in a time-series database (InfluxDB) and visualise it in Grafana.

## What I learned
Hello!
This was only a personal project that I had private. Just to have something to show when I share my GitHub.
If you know me and you see this project and have some doubts about or something that I can do better just tell me, I want to learn more.
When I did this project took me 2 weeks for complete it. It was really difficult when I was beginning with MQTT's but the with some logic was more understandable.
Thanks for see it. :)


