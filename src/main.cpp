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
//  CODE INJECTION  (order matters)
// ═══════════════════════════════════════════════════════════════

#include "display_utils.h"
#include "icons.h"
#include "TicTacToeState.h"
#include "Connect4State.h"
#include "BlackjackState.h"
#include "HomeState.h"
#include "InboxState.h"
#include "ComposeState.h"
#include "SentState.h"
#include "GamesState.h"
#include "WifiState.h"
#include "Callbacks.h"
#include "Connection.h"
#include "Power.h"

// ═══════════════════════════════════════════════════════════════
//  SETUP
//  Every boot is a button wake (or power-on): connect, show Home.
// ═══════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);   // no wait for the monitor — that cost 2 s per wake

  pinMode(EPD_CS,     OUTPUT);
  pinMode(EPD_DC,     OUTPUT);
  pinMode(EPD_RST,    OUTPUT);
  pinMode(EPD_BUSY,   INPUT);
  pinMode(BTN_LEFT,   INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BTN_RIGHT,  INPUT_PULLUP);
  SPI.begin();

  // Why we're awake. Printed again at the end of setup, since this early
  // the serial monitor usually hasn't reconnected yet.
  const esp_reset_reason_t       resetReason = esp_reset_reason();
  const esp_sleep_wakeup_cause_t wakeCause   = esp_sleep_get_wakeup_cause();
  char bootInfo[48];
  snprintf(bootInfo, sizeof(bootInfo), "reset %d, wake %d, pins 0x%llx",
           (int)resetReason, (int)wakeCause, esp_sleep_get_gpio_wakeup_status());
  Serial.printf("=== Ebeep boot: %s ===\n", bootInfo);

  // Timer wakes only happen when a wake button was stuck at sleep time —
  // just re-check the buttons, no reason to bring the radio up.
  if (wakeCause == ESP_SLEEP_WAKEUP_TIMER) enterDeepSleep(false);

  updateBattery();   // before WiFi, so the reading isn't dragged down by TX current

  // The battery couldn't hold up the WiFi radio last time. Trying again
  // would only brown out again — say so and wait for a charge.
  if (resetReason == ESP_RST_BROWNOUT) {
    batteryEmpty = true;
    enterDeepSleep();
  }

  initDisplay();
  loadKnownNetworks();
  setupMqtt();

  if (!connectWifi()) enterDeepSleep();   // setup cancelled / timed out — press again to retry
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(WIFI_PS_MAX_MODEM);
  connectMQTT();
  netWifiOk = (WiFi.status() == WL_CONNECTED);
  netMqttOk = mqttClient.connected();

  // TLS handshake is done — 80 MHz is plenty for drawing and button polling.
  setCpuFrequencyMhz(80);

  currentState = STATE_HOME;
  needRefresh  = true;
  markActivity();

  // The press that woke us may still be held — don't count it on Home.
  lastLeft  = digitalRead(BTN_LEFT);
  lastMid   = digitalRead(BTN_SELECT);
  lastRight = digitalRead(BTN_RIGHT);

  Serial.printf("Awake (%s)\n", bootInfo);
}

// ═══════════════════════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════════════════════
unsigned long lastStatusBarRefresh = 0;
unsigned long lastBatteryRead      = 0;
unsigned long lastMqttAttempt      = 0;
unsigned long wifiLostAt           = 0;

// Keeps netWifiOk / netMqttOk current, reconnects MQTT, and switches to
// the "Connecting..." screen if WiFi stays down. Returns true when the
// status bar icons need redrawing.
bool updateConnection(unsigned long now) {
  const bool wifiOk = (WiFi.status() == WL_CONNECTED);
  const bool mqttOk = mqttClient.connected();
  const bool changed = (wifiOk != netWifiOk) || (mqttOk != netMqttOk);

  if (netWifiOk && !wifiOk) wifiLostAt = now;   // outage just started
  netWifiOk = wifiOk;
  netMqttOk = mqttOk;

  if (currentState == STATE_WIFI && wifiOk && mqttOk) {
    currentState = STATE_HOME;
    needRefresh  = true;
    fastUpdate   = false;
  } else if (currentState != STATE_WIFI && !wifiOk && !userIsBusy() &&
             now - wifiLostAt > WIFI_RECONNECT_GRACE_MS) {
    currentState = STATE_WIFI;
    needRefresh  = true;
    fastUpdate   = false;
  }

  if (wifiOk && !mqttOk && now - lastMqttAttempt > MQTT_RETRY_MS) {
    lastMqttAttempt = now;
    connectMQTT();
  } else if (mqttOk) {
    mqttClient.loop();
    flushInboxClear();
  }
  return changed;
}

void loop() {
  const unsigned long now = millis();

  checkButtons();
  const bool iconsStale = updateConnection(now);

  if (now - lastBatteryRead > BATTERY_READ_INTERVAL_MS) {
    lastBatteryRead = now;
    updateBattery();
  }

  if (currentState == STATE_SENT && now - sentEnteredAt >= SENT_DISPLAY_MS) {
    currentState = STATE_HOME;
    needRefresh  = true;
    fastUpdate   = false;
  }

  if (needRefresh) {
    needRefresh = false;
    refreshDisplay();
    lastStatusBarRefresh = now;
  } else if (iconsStale || now - lastStatusBarRefresh > STATUS_REFRESH_MS) {
    lastStatusBarRefresh = now;
    refreshStatusBarOnly();
  }

  handlePowerState();

  delay(10);   // lets the CPU idle between polls; still far quicker than any press
}
