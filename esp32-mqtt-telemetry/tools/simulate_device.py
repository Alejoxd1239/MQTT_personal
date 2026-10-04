#!/usr/bin/env python3
"""
Simulates the ESP32 node so the dashboard can be tested without hardware.

It speaks the same MQTT protocol as the firmware: publishes status/telemetry,
answers cmd/led and cmd/interval, and fakes button presses now and then.

Usage:
    pip install "paho-mqtt>=2.0"
    python tools/simulate_device.py --topic nackademin-iot/your-name/node-1
"""

import argparse
import json
import random
import time

import paho.mqtt.client as mqtt


def main() -> None:
    parser = argparse.ArgumentParser(description="Simulated ESP32 telemetry node")
    parser.add_argument("--host", default="broker.hivemq.com")
    parser.add_argument("--port", type=int, default=1883)
    parser.add_argument("--topic", required=True, help="TOPIC_PREFIX/DEVICE_NAME")
    parser.add_argument("--interval", type=float, default=5.0, help="seconds between telemetry")
    args = parser.parse_args()

    base = args.topic.rstrip("/")
    state = {
        "led": False,
        "interval": args.interval,
        "buttons": 0,
        "reconnects": 0,
        "rssi": -60.0,
        "start": time.monotonic(),
    }

    def telemetry() -> str:
        # Random walk so the signal chart looks realistic.
        state["rssi"] = max(-92.0, min(-38.0, state["rssi"] + random.uniform(-3, 3)))
        return json.dumps({
            "device": base.split("/")[-1],
            "uptime_s": int(time.monotonic() - state["start"]),
            "rssi_dbm": int(state["rssi"]),
            "free_heap": random.randint(205_000, 215_000),
            "led": state["led"],
            "interval_s": int(state["interval"]),
            "reconnects": state["reconnects"],
            "button_count": state["buttons"],
            "touch": random.randint(55, 70),
        })

    def publish_telemetry(client: mqtt.Client) -> None:
        payload = telemetry()
        client.publish(f"{base}/telemetry", payload)
        print(f"-> telemetry {payload}")

    def on_connect(client, userdata, flags, reason_code, properties):
        if reason_code.is_failure:
            print(f"Connection failed: {reason_code}")
            return
        state["reconnects"] += 1
        print(f"Connected to {args.host}:{args.port}, base topic {base}")
        client.publish(f"{base}/status", "online", qos=1, retain=True)
        client.subscribe(f"{base}/cmd/#", qos=1)
        publish_telemetry(client)

    def on_message(client, userdata, msg):
        text = msg.payload.decode(errors="replace").strip()
        print(f"<- {msg.topic}: {text}")
        if msg.topic == f"{base}/cmd/led":
            if text.lower() == "on":
                state["led"] = True
            elif text.lower() == "off":
                state["led"] = False
            elif text.lower() == "toggle":
                state["led"] = not state["led"]
            else:
                return
            publish_telemetry(client)
        elif msg.topic == f"{base}/cmd/interval":
            try:
                seconds = int(text)
            except ValueError:
                return
            if 1 <= seconds <= 3600:
                state["interval"] = seconds
                publish_telemetry(client)

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,
                         client_id=f"sim-{random.randint(0, 0xFFFFFF):06x}")
    client.will_set(f"{base}/status", "offline", qos=1, retain=True)
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(args.host, args.port, keepalive=30)
    client.loop_start()

    try:
        last = 0.0
        while True:
            now = time.monotonic()
            if now - last >= state["interval"]:
                last = now
                if client.is_connected():
                    publish_telemetry(client)
            # Occasionally simulate someone pressing the button.
            if client.is_connected() and random.random() < 0.01:
                state["buttons"] += 1
                state["led"] = not state["led"]
                event = json.dumps({"type": "button", "count": state["buttons"], "led": state["led"]})
                client.publish(f"{base}/event", event)
                print(f"-> event {event}")
                publish_telemetry(client)
            time.sleep(0.2)
    except KeyboardInterrupt:
        print("\nStopping, publishing offline status")
        client.publish(f"{base}/status", "offline", qos=1, retain=True).wait_for_publish(timeout=3)
        client.loop_stop()
        client.disconnect()


if __name__ == "__main__":
    main()
