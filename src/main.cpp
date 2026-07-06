#include <Arduino.h>
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

#include "config.h"

// ═══════════════════════════════════════════════════════════════
//  CODE INJECTION  (order matters — each file uses symbols above)
// ═══════════════════════════════════════════════════════════════

#include "display_utils.h"
#include "icons.h"
#include "TicTacToeState.h"
#include "HomeState.h"
#include "InboxState.h"
#include "ComposeState.h"
#include "SentState.h"
#include "GamesState.h"
#include "wifiState.h"
#include "Callbacks.h"

// ═══════════════════════════════════════════════════════════════
//  Functions
// ═══════════════════════════════════════════════════════════════
WiFiManager wm;
unsigned long lastReconnectAttempt = 0;
bool lastWifiState = false;
bool lastMqttState = false;

void connectWifi(){
  if (sizeof(HOTSPOT_PASSWORD) > 1) {
    wm.autoConnect(AP_NAME, HOTSPOT_PASSWORD);
  } else {
    wm.autoConnect(AP_NAME);
  }
  while (WiFi.status() != WL_CONNECTED) delay(100);
  Serial.println("WiFi connected");
  lastWifiState = true;
}

// Rewritten to be Non-Blocking
bool connectMQTT(){
  Serial.print("MQTT connect... ");
  if (mqttClient.connect("Ebeep_Device", MQTT_USERNAME_STR, MQTT_PASSWORD_STR)) {
    Serial.println("OK");
    mqttClient.subscribe((String(BEEPER_ID) + "/inbox").c_str());
    mqttClient.subscribe((String(BEEPER_ID) + "/games").c_str());
    lastMqttState = true;
    return true;
  } else {
    Serial.printf("failed rc=%d\n", mqttClient.state());
    return false;
  }
}

// ═══════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("=== Ebeep Boot ===");
  currentState = STATE_WIFI;
  needRefresh  = true;

  pinMode(EPD_CS,   OUTPUT);
  pinMode(EPD_DC,   OUTPUT);
  pinMode(EPD_RST,  OUTPUT);
  pinMode(EPD_BUSY, INPUT);
  pinMode(BTN_LEFT,   INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BTN_RIGHT,  INPUT_PULLUP);

  SPI.begin();
  display.init(115200, true, 50, false);
  display.setRotation(3);

  updateBattery();
  mqttClient.setServer(MQTT_SERVER_ADDR, MQTT_SERVER_PORT);
  mqttClient.setCallback(onMessageReceived);


  bool wokeFromTimer = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER);
  if (wokeFromTimer && quickCheckAndMaybeSleep()) return;

  // ── WiFi ─────────────────────────────────────────────────
  if (WiFi.status() != WL_CONNECTED) {
    //wm.resetSettings();  // uncomment to erase settings
    connectWifi();
    wifiClient.setInsecure();
  }
  WiFi.setAutoReconnect(true); // Let hardware handle background wifi reconnection
  WiFi.setSleep(WIFI_PS_MIN_MODEM);

  // ── MQTT ─────────────────────────────────────────────────
  if (!mqttClient.connected()) connectMQTT();
  syncNightClock();  // one-time NTP sync; RTC keeps time through deep sleep after that

  currentState = STATE_HOME;
  needRefresh  = true;
  markActivity();
}

// ═══════════════════════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════════════════════
static unsigned long lastStatusBarRefresh = 0;
static unsigned long lastBatteryRead      = 0;
static unsigned long now;
unsigned long wifiLostAt = 0;

void loop() {
  now = millis();

  checkButtons();

  bool currentWifiState = (WiFi.status() == WL_CONNECTED);
  bool currentMqttState = mqttClient.connected();

  if (currentWifiState != lastWifiState) {
    lastWifiState = currentWifiState;
    if (!currentWifiState) {
      wifiLostAt = now;                  // outage just started
    } else if (currentState == STATE_WIFI) {
      currentState = STATE_HOME;         // reconnected — leave the wifi screen
      needRefresh  = true;
      fastUpdate   = false;
    }
    needRefresh = true;
  }
  if (currentMqttState != lastMqttState) {
    lastMqttState = currentMqttState;
    needRefresh = true;
  }

  // Only take over the screen for outages longer than a brief hiccup,
  // and never mid-message or mid-game.
  if (!currentWifiState && currentState != STATE_WIFI &&
      now - wifiLostAt > WIFI_RECONNECT_GRACE_MS && !sleepWouldLoseState()) {
    currentState = STATE_WIFI;
    needRefresh  = true;
    fastUpdate   = false;
  }

  if (!currentWifiState) {
    // WiFi is down. Hardware is auto-reconnecting.
  }
  else if (!currentMqttState) {
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      if (connectMQTT()) {
        lastReconnectAttempt = 0;
      }
    }
  }
  else {
    mqttClient.loop();
  }

  if (now - lastBatteryRead > BATTERY_READ_INTERVAL_MS) {
    lastBatteryRead = now;
    updateBattery();
  }

  if (currentState == STATE_SENT && (millis() - sentEnteredAt >= SENT_DISPLAY_MS)) {
    currentState = STATE_HOME;
    needRefresh  = true;
    fastUpdate   = false;
  }

  if (needRefresh) {
    needRefresh = false;
    refreshDisplay();
    lastStatusBarRefresh = now;
  } else if (now - lastStatusBarRefresh > STATUS_REFRESH_MS) {
    lastStatusBarRefresh = now;
    refreshStatusBarOnly();
  }

  handlePowerState();
}