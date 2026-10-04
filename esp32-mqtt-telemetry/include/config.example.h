// Copy this file to include/config.h and fill in your own values.
// config.h is listed in .gitignore so your Wi-Fi password never ends up on GitHub.
#pragma once

// ---- Wi-Fi (2.4 GHz only) ----
#define WIFI_SSID     "WIFI-DUMMY"
#define WIFI_PASSWORD "CHANGE-password"

// ---- MQTT broker ----
// Public test broker: fine for demos, NOT for real data (no auth, no TLS).
#define MQTT_HOST "broker.hivemq.com"
#define MQTT_PORT 1883

// All topics live under TOPIC_PREFIX/DEVICE_NAME.
// Make the prefix unique so you don't collide with other people on the public broker.
#define TOPIC_PREFIX "nackademin-iot/change-me" 
#define DEVICE_NAME  "node-1"

// ---- Behaviour ----
#define DEFAULT_PUBLISH_INTERVAL_MS 5000

// ---- Hardware ----
// Defaults below are for the Arduino Nano ESP32. Pin names like D2 and A0
// are the labels printed on the board.
//
// Classic ESP32 DevKit instead:
//   LED_PIN 2, LED_ACTIVE_LOW false, BUTTON_PIN 0 (BOOT button), TOUCH_PIN 4
#define LED_PIN        LED_BUILTIN  // Orange "L" LED next to the USB port (D13)
#define LED_ACTIVE_LOW false        // Set to true if your LED lights up when the pin is LOW
#define BUTTON_PIN     D2           // No user button on the Nano: touch a wire from D2 to GND
#define ENABLE_TOUCH   true         // Built-in capacitive touch (classic ESP32 / S2 / S3)
#define TOUCH_PIN      A0           // Touch the A0 pin (or a wire connected to it) with a finger
