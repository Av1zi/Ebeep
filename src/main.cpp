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

WiFiManager wm;
unsigned long lastReconnectAttempt = 0;
bool lastWifiState = false;
bool lastMqttState = false;

// WiFiManager setup hotspot (saved credentials are already tried by quickWifiConnect()).
// The hotspot gives up after WIFI_PORTAL_TIMEOUT_S — left running it empties the battery overnight.
bool connectWifi(){
  wm.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_S);
  bool ok;
  if (sizeof(HOTSPOT_PASSWORD) > 1) {
    ok = wm.autoConnect(AP_NAME, AP_password);
  } else {
    ok = wm.autoConnect(AP_NAME);
  }
  if (!ok) return false;
  Serial.println("WiFi connected");
  cacheWifiAp();
  lastWifiState = true;
  return true;
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
  // No delay here — this runs on every timer wake, and waiting for the
  // serial monitor used to cost 2 s of awake time each time.
  Serial.begin(115200);
  Serial.println("=== Ebeep Boot ===");
  currentState = STATE_WIFI;
  needRefresh  = true;

  // configTzTime() only sets TZ for this boot — without this, night hours
  // were checked in UTC after every deep sleep.
  setenv("TZ", TZ_STRING, 1);
  tzset();

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

  // Usually sleeps again without ever touching the display.
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) timerWakeCheck();

  initDisplay();

  // ── WiFi ─────────────────────────────────────────────────
  if (WiFi.status() != WL_CONNECTED && !quickWifiConnect()) {
    netWifiOk = netMqttOk = false;
    refreshDisplay();   // shows wifisate — only when we actually have to wait
    needRefresh = true;
    //wm.resetSettings();  // uncomment to erase save wifi credentials
    if (!connectWifi()) enterDeepSleep(currentWakeIntervalS());  // nobody set it up, try again later
  }
  WiFi.setAutoReconnect(true); // Let hardware handle background wifi reconnection
  WiFi.setSleep(WIFI_PS_MAX_MODEM);

  // ── MQTT ─────────────────────────────────────────────────
  if (!mqttClient.connected()) connectMQTT();
  syncNightClock();
  netWifiOk = (WiFi.status() == WL_CONNECTED);
  netMqttOk = mqttClient.connected();

  // TLS handshake is done — 80 MHz is plenty for drawing and button polling.
  setCpuFrequencyMhz(80);

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