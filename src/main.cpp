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

  pinMode(EPD_CS,   OUTPUT);
  pinMode(EPD_DC,   OUTPUT);
  pinMode(EPD_RST,  OUTPUT);
  pinMode(EPD_BUSY, INPUT);
  pinMode(BTN_LEFT,   INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BTN_RIGHT,  INPUT_PULLUP);

  SPI.begin();
  display.init(115200, true, 50, false);
  display.setRotation(1);
  
  currentState = STATE_WIFI;
  needRefresh = true;

  // ── WiFi ─────────────────────────────────────────────────
  //wm.resetSettings();  // uncomment to erase settings
  connectWifi();
  wifiClient.setInsecure();
  WiFi.setAutoReconnect(true); // Let hardware handle background wifi reconnection
  WiFi.setSleep(WIFI_PS_MIN_MODEM);

  // ── MQTT ─────────────────────────────────────────────────
  mqttClient.setServer(MQTT_SERVER_ADDR, MQTT_SERVER_PORT);
  mqttClient.setCallback(onMessageReceived);
  connectMQTT();

  currentState = STATE_HOME;
  needRefresh  = true;
}

// ═══════════════════════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════════════════════
void loop() {
  unsigned long now = millis();
  
  checkButtons();
  
  bool currentWifiState = (WiFi.status() == WL_CONNECTED);
  bool currentMqttState = mqttClient.connected();

  if (currentWifiState != lastWifiState || currentMqttState != lastMqttState) {
    lastWifiState = currentWifiState;
    lastMqttState = currentMqttState;
    needRefresh = true; // Connection changed, update the icons visually!
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
    // Everything is healthy, process incoming/outgoing packets
    mqttClient.loop();
  }

  // Auto-dismiss Sent screen.
  if (currentState == STATE_SENT && (millis() - sentEnteredAt >= SENT_DISPLAY_MS)) {
    currentState = STATE_HOME;
    needRefresh  = true;
    fastUpdate   = false;
  }

  if (needRefresh) {
    needRefresh = false;
    refreshDisplay();
  }
}