/*
 * ESP32 MQTT telemetry node
 *
 * Publishes device telemetry (Wi-Fi signal, uptime, free heap, touch sensor,
 * LED state) over MQTT and accepts remote commands (LED, publish interval).
 * The whole loop is non-blocking: Wi-Fi and MQTT reconnect in the background
 * while the button and telemetry keep working.
 *
 * Topics (base = TOPIC_PREFIX/DEVICE_NAME):
 *   base/status        "online" / "offline"   (retained, offline via Last Will)
 *   base/telemetry     JSON every publish interval
 *   base/event         JSON on button press
 *   base/cmd/led       "on" | "off" | "toggle"        (subscribed)
 *   base/cmd/interval  seconds, 1..3600              (subscribed)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h> 
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "soc/soc_caps.h"

#if __has_include("config.h")
#include "config.h"
#else
#error "Missing include/config.h - copy include/config.example.h to include/config.h and fill in your values."
#endif
#include "root_ca.h"

#if ENABLE_TOUCH && defined(SOC_TOUCH_SENSOR_NUM) && (SOC_TOUCH_SENSOR_NUM > 0)
#define TOUCH_AVAILABLE 1
#else
#define TOUCH_AVAILABLE 0
#endif

// ---------- Constants ----------
static const uint32_t WIFI_RETRY_MS        = 10000;
static const uint32_t MQTT_RETRY_MIN_MS    = 1000;
static const uint32_t MQTT_RETRY_MAX_MS    = 30000;
static const uint32_t BUTTON_DEBOUNCE_MS   = 50;
static const uint32_t MIN_INTERVAL_MS      = 1000;
static const uint32_t MAX_INTERVAL_MS      = 3600000;

WiFiClientSecure netClient;   // TLS: encrypted + broker certificate verified
PubSubClient mqtt(netClient);

String topicBase, topicStatus, topicTelemetry, topicEvent, topicCmdLed, topicCmdInterval;
String clientId;

uint32_t publishIntervalMs = DEFAULT_PUBLISH_INTERVAL_MS;
uint32_t lastPublishMs     = 0;
uint32_t lastWifiAttemptMs = 0;
uint32_t lastMqttAttemptMs = 0;
uint32_t mqttRetryDelayMs  = MQTT_RETRY_MIN_MS;
uint32_t mqttReconnects    = 0;
bool     ntpStarted        = false;

bool     ledOn             = false;
uint32_t buttonPresses     = 0;
int      lastButtonReading = HIGH;
int      buttonState       = HIGH;
uint32_t lastButtonChangeMs = 0;

// ---------- Helpers ----------
void setLed(bool on) {
  ledOn = on;
  digitalWrite(LED_PIN, (on != LED_ACTIVE_LOW) ? HIGH : LOW);
}

void publishTelemetry() {
  if (!mqtt.connected()) return;

  JsonDocument doc;
  doc["device"]       = DEVICE_NAME;
  doc["uptime_s"]     = millis() / 1000;
  doc["rssi_dbm"]     = WiFi.RSSI();
  doc["free_heap"]    = ESP.getFreeHeap();
  doc["led"]          = ledOn;
  doc["interval_s"]   = publishIntervalMs / 1000;
  doc["reconnects"]   = mqttReconnects;
  doc["button_count"] = buttonPresses;
#if TOUCH_AVAILABLE
  doc["touch"]        = touchRead(TOUCH_PIN);
#endif

  char payload[256];
  size_t len = serializeJson(doc, payload, sizeof(payload));
  mqtt.publish(topicTelemetry.c_str(), reinterpret_cast<const uint8_t*>(payload), len, false);
  Serial.printf("[mqtt] telemetry %s\n", payload);
}

void publishButtonEvent() {
  if (!mqtt.connected()) return;

  JsonDocument doc;
  doc["type"]  = "button";
  doc["count"] = buttonPresses;
  doc["led"]   = ledOn;

  char payload[96];
  size_t len = serializeJson(doc, payload, sizeof(payload));
  mqtt.publish(topicEvent.c_str(), reinterpret_cast<const uint8_t*>(payload), len, false);
}

// ---------- MQTT commands ----------
void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  String t(topic);
  String msg;
  msg.reserve(length);
  for (unsigned int i = 0; i < length; i++) msg += static_cast<char>(payload[i]);
  msg.trim();
  Serial.printf("[mqtt] <- %s: %s\n", topic, msg.c_str());

  if (t == topicCmdLed) {
    if (msg.equalsIgnoreCase("on"))          setLed(true);
    else if (msg.equalsIgnoreCase("off"))    setLed(false);
    else if (msg.equalsIgnoreCase("toggle")) setLed(!ledOn);
    else { Serial.println("[cmd] unknown LED command, ignored"); return; }
    publishTelemetry();  // Report the new state right away
  } else if (t == topicCmdInterval) {
    long seconds = msg.toInt();
    uint32_t ms = static_cast<uint32_t>(seconds) * 1000;
    if (seconds <= 0 || ms < MIN_INTERVAL_MS || ms > MAX_INTERVAL_MS) {
      Serial.println("[cmd] interval out of range (1..3600 s), ignored");
      return;
    }
    publishIntervalMs = ms;
    Serial.printf("[cmd] publish interval set to %ld s\n", seconds);
    publishTelemetry();
  }
}

// ---------- Connectivity (non-blocking) ----------
void maintainWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  uint32_t now = millis();
  if (now - lastWifiAttemptMs < WIFI_RETRY_MS && lastWifiAttemptMs != 0) return;
  lastWifiAttemptMs = now;

  Serial.printf("[wifi] connecting to %s...\n", WIFI_SSID);
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

bool clockReady() {
  if (!ntpStarted) {
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    ntpStarted = true;
    Serial.println("[time] syncing clock over NTP...");
  }
  return time(nullptr) > 1704067200;  // later than 2024-01-01 = clock is set
}

void maintainMqtt() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (mqtt.connected()) { mqtt.loop(); return; }
  if (!clockReady()) return; 

  uint32_t now = millis();
  if (now - lastMqttAttemptMs < mqttRetryDelayMs && lastMqttAttemptMs != 0) return;
  lastMqttAttemptMs = now;

  Serial.printf("[mqtt] connecting to %s:%d as %s...\n", MQTT_HOST, MQTT_PORT, clientId.c_str());
  char tlsError[128];
  if (netClient.lastError(tlsError, sizeof(tlsError)) != 0) {
    Serial.printf("[tls] %s\n", tlsError);
  }
  
  bool ok = mqtt.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD,
  topicStatus.c_str(), 1, true, "offline");

  if (ok) {
    Serial.println("[mqtt] connected");
    mqttRetryDelayMs = MQTT_RETRY_MIN_MS;
    mqttReconnects++;
    mqtt.publish(topicStatus.c_str(), "online", true);
    mqtt.subscribe((topicBase + "/cmd/#").c_str(), 1);
    publishTelemetry();
  } else {
    Serial.printf("[mqtt] failed, state=%d, retry in %lu ms\n", mqtt.state(),
                  static_cast<unsigned long>(mqttRetryDelayMs));
    // Exponential backoff so we don't hammer the broker.
    mqttRetryDelayMs = min(mqttRetryDelayMs * 2, MQTT_RETRY_MAX_MS);
  }
}

// ---------- Button ----------
void handleButton() {
  int reading = digitalRead(BUTTON_PIN);
  uint32_t now = millis();

  if (reading != lastButtonReading) lastButtonChangeMs = now;
  lastButtonReading = reading;

  if (now - lastButtonChangeMs > BUTTON_DEBOUNCE_MS && reading != buttonState) {
    buttonState = reading;
    if (buttonState == LOW) {  // Pressed (active low with pull-up)
      buttonPresses++;
      setLed(!ledOn);
      Serial.printf("[button] press #%lu, LED %s\n",
                    static_cast<unsigned long>(buttonPresses), ledOn ? "on" : "off");
      publishButtonEvent();
      publishTelemetry();
    }
  }
}

// ---------- Arduino entry points ----------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== ESP32 MQTT telemetry node ===");

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  setLed(false);

  topicBase        = String(TOPIC_PREFIX) + "/" + DEVICE_NAME;
  topicStatus      = topicBase + "/status";
  topicTelemetry   = topicBase + "/telemetry";
  topicEvent       = topicBase + "/event";
  topicCmdLed      = topicBase + "/cmd/led";
  topicCmdInterval = topicBase + "/cmd/interval";

  // Unique client id based on the chip's MAC address.
  uint64_t mac = ESP.getEfuseMac();
  char id[32];
  snprintf(id, sizeof(id), "esp32-%04X%08X",
           static_cast<uint16_t>(mac >> 32), static_cast<uint32_t>(mac));
  clientId = id;

  Serial.printf("[boot] base topic: %s\n", topicBase.c_str());
#if !TOUCH_AVAILABLE
  Serial.println("[boot] touch sensor not available on this chip, skipping");
#endif

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  netClient.setCACert(ROOT_CA_PEM);  
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);
  mqtt.setBufferSize(512);
  mqtt.setKeepAlive(30);
}

void loop() {
  maintainWifi();
  maintainMqtt();
  handleButton();

  uint32_t now = millis();
  if (now - lastPublishMs >= publishIntervalMs) {
    lastPublishMs = now;
    publishTelemetry();
  }
}
