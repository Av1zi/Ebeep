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

// ═══════════════════════════════════════════════════════════════
//  Functions
// ═══════════════════════════════════════════════════════════════

unsigned long lastReconnectAttempt = 0;
bool lastWifiState = false;
bool lastMqttState = false;

// Last network → every known network in range → setup hotspot.
// Only gives the e-ink something to do if the quick reconnect doesn't land.
bool connectWifi(){
  const bool fastStarted = beginLastNetwork();
  if (fastStarted && waitForWifi(1200)) {
    cacheWifiAp();
    return true;
  }

  currentState = STATE_WIFI;
  fastUpdate   = false;
  refreshDisplay();   // "Connecting..." — runs while the radio keeps trying

  if (connectKnownWifi(fastStarted)) return true;
  return runWifiPortal();
}

// build once instead of allocating a String on every single reconnect attempt.
static char INBOX_TOPIC[24] = "";
static char GAMES_TOPIC[24] = "";

bool connectMQTT(){
  if (INBOX_TOPIC[0] == '\0') {
    snprintf(INBOX_TOPIC, sizeof(INBOX_TOPIC), "%s/inbox", BEEPER_ID);
    snprintf(GAMES_TOPIC, sizeof(GAMES_TOPIC), "%s/games", BEEPER_ID);
  }
  Serial.print("MQTT connect... ");
  if (mqttClient.connect(BEEPER_ID, MQTT_USERNAME_STR, MQTT_PASSWORD_STR)) {
    Serial.println("OK");
    mqttClient.subscribe(INBOX_TOPIC);
    mqttClient.subscribe(GAMES_TOPIC);
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
  // Every boot is a button wake (or power-on) — there's no timer wake.
  // No serial-monitor delay: it used to cost 2 s of awake time per wake.
  Serial.begin(115200);
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

  updateBattery();   // before WiFi, so the reading isn't dragged down by TX current
  mqttClient.setServer(MQTT_SERVER_ADDR, MQTT_SERVER_PORT);
  mqttClient.setCallback(onMessageReceived);
  wifiClient.setInsecure();
  wifiClient.setTimeout(WIFI_CONNECT_TIMEOUT_MS);
  mqttClient.setSocketTimeout(WIFI_CONNECT_TIMEOUT_MS / 1000);

  const esp_reset_reason_t resetReason = esp_reset_reason();
  const esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
  Serial.printf("Reset reason %d, wake cause %d, ext1 pins 0x%llx\n",
                (int)resetReason, (int)wakeCause, esp_sleep_get_ext1_wakeup_status());

  // Only used when both wake buttons read stuck-pressed at sleep time —
  // just re-check them, no reason to bring the radio up.
  if (wakeCause == ESP_SLEEP_WAKEUP_TIMER) enterDeepSleep(false);

  // The battery couldn't hold up the WiFi radio last time. Trying again would
  // just brown out again in a loop — say so and wait for a charge.
  if (resetReason == ESP_RST_BROWNOUT) {
    batteryEmpty = true;
    enterDeepSleep();
  }

  loadKnownNetworks();
  initDisplay();

  // ── WiFi ─────────────────────────────────────────────────
  if (!connectWifi()) enterDeepSleep();   // portal cancelled / timed out — press again to retry
  lastWifiState = true;
  WiFi.setAutoReconnect(true); // Let hardware handle background wifi reconnection
  WiFi.setSleep(WIFI_PS_MAX_MODEM);

  // ── MQTT ─────────────────────────────────────────────────
  if (!mqttClient.connected()) connectMQTT();
  netWifiOk = (WiFi.status() == WL_CONNECTED);
  netMqttOk = mqttClient.connected();

  // TLS handshake is done — 80 MHz is plenty for drawing and button polling.
  setCpuFrequencyMhz(80);

  currentState = STATE_HOME;
  needRefresh  = true;
  markActivity();

  // The press that woke us may still be held — don't let it count as a
  // press on the Home screen.
  lastLeft  = digitalRead(BTN_LEFT);
  lastMid   = digitalRead(BTN_SELECT);
  lastRight = digitalRead(BTN_RIGHT);
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
  bool iconsStale = (currentWifiState != lastWifiState) || (currentMqttState != lastMqttState);

  if (currentWifiState != lastWifiState) {
    lastWifiState = currentWifiState;
    if (!currentWifiState) wifiLostAt = now;   // outage just started
  }
  lastMqttState = currentMqttState;
  netWifiOk     = currentWifiState;
  netMqttOk     = currentMqttState;

  const bool fullyConnected = currentWifiState && currentMqttState;

  if (currentState == STATE_WIFI && fullyConnected) {
    currentState = STATE_HOME;
    needRefresh  = true;
    fastUpdate   = false;
  } else if (currentState != STATE_WIFI && !currentWifiState &&
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
    flushInboxClear();
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
  } else if (iconsStale || now - lastStatusBarRefresh > STATUS_REFRESH_MS) {
    lastStatusBarRefresh = now;
    refreshStatusBarOnly();   // icons only — no reason to redraw the whole screen for this
  }

  handlePowerState();

  // Let the idle task park the CPU between polls instead of spinning flat out.
  // 10 ms is still far quicker than any button press.
  delay(10);
}