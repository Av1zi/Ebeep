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
//  SETUP
// ═══════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("=== Ebeep Boot ===");

  // Lower CPU frequency to 80 MHz — saves ~20 mA with no UI impact.
  setCpuFrequencyMhz(80);

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

  // Show "Connecting..." screen while WiFi/MQTT come up.
  currentState = STATE_WIFI;
  refreshDisplay();

  // ── WiFi ─────────────────────────────────────────────────
  WiFiManager wm;
  //wm.resetSettings();  // uncomment to wipe saved credentials

  if (sizeof(HOTSPOT_PASSWORD) > 1) {   // non-empty password in secret.h
    wm.autoConnect(AP_NAME, HOTSPOT_PASSWORD);
  } else {
    wm.autoConnect(AP_NAME);
  }
  while (WiFi.status() != WL_CONNECTED) delay(100);
  Serial.println("WiFi connected");

  WiFi.setSleep(WIFI_PS_MIN_MODEM);

  // ── MQTT ─────────────────────────────────────────────────
  wifiClient.setInsecure();
  mqttClient.setServer(MQTT_SERVER_ADDR, MQTT_SERVER_PORT);
  mqttClient.setCallback(onMessageReceived);

  while (!mqttClient.connected()) {
    Serial.print("MQTT connect... ");
    if (mqttClient.connect("Ebeep_Device", MQTT_USERNAME_STR, MQTT_PASSWORD_STR)) {
      Serial.println("OK");
      mqttClient.subscribe((String(BEEPER_ID) + "/inbox").c_str());
      mqttClient.subscribe((String(BEEPER_ID) + "/games").c_str());
    } else {
      Serial.printf("failed rc=%d, retry in 1s\n", mqttClient.state());
      delay(1000);
    }
  }

  currentState = STATE_HOME;
  needRefresh  = true;
}

// ═══════════════════════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════════════════════
void loop() {
  checkButtons();
  mqttClient.loop();

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
