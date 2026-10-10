
#pragma once

// ---- Wi-Fi (2.4 GHz only) ----
#define WIFI_SSID     "CHANGE-ME"
#define WIFI_PASSWORD "CHANGE-ME"

// ---- MQTT broker ----
// Public test broker: fine for demos, NOT for real data (no auth, no TLS).
// ---- MQTT broker (private, with TLS and login) ----
#define MQTT_HOST     "CHANGE-ME-central-1.emqxsl.com"
#define MQTT_PORT     8883
#define MQTT_USE_TLS  true
#define MQTT_USER     "device"
#define MQTT_PASSWORD "CHANGE-ME"

// All topics live under TOPIC_PREFIX/DEVICE_NAME.
// Make the prefix unique so you don't collide with other people on the public broker.
#define TOPIC_PREFIX "nackademin-iot/CHANGE-ME" 
#define DEVICE_NAME  "CHANGE-ME"

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
