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
//  DISPLAY
// ═══════════════════════════════════════════════════════════════
GxEPD2_BW<GxEPD2_290_BS, GxEPD2_290_BS::HEIGHT> display(
  GxEPD2_290_BS(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);

// ═══════════════════════════════════════════════════════════════
//  NETWORK
// ═══════════════════════════════════════════════════════════════
WiFiClientSecure wifiClient;
PubSubClient     mqttClient(wifiClient);

// ═══════════════════════════════════════════════════════════════
//  STATE MACHINE
// ═══════════════════════════════════════════════════════════════
enum ScreenState : uint8_t {
  STATE_WIFI,
  STATE_HOME,
  STATE_INBOX,
  STATE_COMPOSE,
  STATE_SENT,
  STATE_GAMES,
};

ScreenState currentState = STATE_WIFI;

// ── Display refresh flags ─────────────────────────────────────
bool needRefresh = false;   // true  → redraw this loop tick
bool fastUpdate  = false;   // true  → partial refresh (content only)
                            // false → full refresh (status bar included)

// ── Message buffers ───────────────────────────────────────────
bool hasUnreadMessage                    = false;
char lastReceivedMessage[MAX_MSG_LEN+1]  = "";
char typedMessage[MAX_MSG_LEN+1]         = "";
uint8_t messageLen                       = 0;
uint8_t currentLetterIdx                 = 0;

// ── Misc UI state ─────────────────────────────────────────────
bool          confirmLeaveCompose = false;
unsigned long sentEnteredAt       = 0;
int           batteryPct          = 67;   // placeholder — real read TODO

// ── Button / hold-scroll state ────────────────────────────────
bool          lastLeft        = HIGH;
bool          lastMid         = HIGH;
bool          lastRight       = HIGH;
unsigned long lastBtnTime     = 0;
unsigned long holdStartTime   = 0;
unsigned long lastRepeatAt    = 0;
int8_t        heldButton      = 0;   // 0=none, -1=left, +1=right

// ═══════════════════════════════════════════════════════════════
//  CODE INJECTION  (order matters — each file uses symbols above)
// ═══════════════════════════════════════════════════════════════
#include "display_utils.h"
#include "icons.h"
#include "HomeState.h"
#include "InboxState.h"
#include "ComposeState.h"
#include "SentState.h"
#include "GamesState.h"
#include "wifiState.h"

// ═══════════════════════════════════════════════════════════════
//  MQTT CALLBACK
// ═══════════════════════════════════════════════════════════════
void onMessageReceived(char* topic, byte* payload, unsigned int length) {
  uint8_t msgLen = (length < MAX_MSG_LEN) ? (uint8_t)length : MAX_MSG_LEN;
  memcpy(lastReceivedMessage, payload, msgLen);
  lastReceivedMessage[msgLen] = '\0';
  hasUnreadMessage = true;

  // Trigger a redraw only for states that show message status.
  if (currentState == STATE_HOME) {
    needRefresh = true;
    fastUpdate  = true;
  } else if (currentState == STATE_INBOX) {
    needRefresh = true;
    fastUpdate  = false;
  }
  // TODO: trigger buzzer melody here
}

// ═══════════════════════════════════════════════════════════════
//  BUTTON HANDLER
// ═══════════════════════════════════════════════════════════════
void checkButtons() {
  bool curLeft  = digitalRead(BTN_LEFT);
  bool curMid   = digitalRead(BTN_SELECT);
  bool curRight = digitalRead(BTN_RIGHT);

  bool leftPressed  = (curLeft  == LOW && lastLeft  == HIGH);
  bool midPressed   = (curMid   == LOW && lastMid   == HIGH);
  bool rightPressed = (curRight == LOW && lastRight == HIGH);

  const unsigned long now = millis();

  if ((leftPressed || midPressed || rightPressed) &&
      (now - lastBtnTime > DEBOUNCE_MS)) {
    lastBtnTime = now;

    // Track left/right holds for fast-scroll; mid cancels.
    if (leftPressed || rightPressed) {
      heldButton    = leftPressed ? -1 : 1;
      holdStartTime = now;
      lastRepeatAt  = now;
    } else {
      heldButton = 0;
    }

    switch (currentState) {
      case STATE_HOME:    handleHomeInput   (leftPressed, midPressed, rightPressed); break;
      case STATE_INBOX:   handleInboxInput  (leftPressed, midPressed, rightPressed); break;
      case STATE_COMPOSE: handleComposeInput(leftPressed, midPressed, rightPressed); break;
      case STATE_SENT:    handleSentInput   (leftPressed, midPressed, rightPressed); break;
      case STATE_GAMES:   handleGamesInput  (leftPressed, midPressed, rightPressed); break;
      default: break;
    }
  }

  // Hold-scroll repeat (Compose & Games only)
  if (heldButton != 0 &&
      (currentState == STATE_COMPOSE || currentState == STATE_GAMES)) {
    const bool stillHeld = (heldButton == -1) ? (curLeft == LOW) : (curRight == LOW);

    if (!stillHeld) {
      heldButton = 0;
    } else if ((now - holdStartTime >= HOLD_DELAY_MS) &&
               (now - lastRepeatAt  >= HOLD_REPEAT_MS)) {
      lastRepeatAt = now;
      const bool fL = (heldButton == -1);
      const bool fR = (heldButton ==  1);
      if (currentState == STATE_COMPOSE) handleComposeInput(fL, false, fR);
      else                               handleGamesInput  (fL, false, fR);
    }
  }

  lastLeft  = curLeft;
  lastMid   = curMid;
  lastRight = curRight;
}

// ═══════════════════════════════════════════════════════════════
//  DISPLAY REFRESH
// ═══════════════════════════════════════════════════════════════
void refreshDisplay() {
  if (fastUpdate) {
    // Partial: only the content area below the status bar.
    display.setPartialWindow(0, CONTENT_Y + 2, SCREEN_W, SCREEN_H - CONTENT_Y);
  } else {
    display.setFullWindow();
  }

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    if (!fastUpdate) drawStatusBar(batteryPct);

    switch (currentState) {
      case STATE_HOME:    drawHome();    break;
      case STATE_INBOX:   drawInbox();   break;
      case STATE_COMPOSE: drawCompose(); break;
      case STATE_SENT:    drawSent();    break;
      case STATE_GAMES:   drawGames();   break;
      case STATE_WIFI:    drawWifi();    break;
    }
  } while (display.nextPage());

  fastUpdate = false;
}

// ═══════════════════════════════════════════════════════════════
//  BATTERY VOLTAGE READING
// ═══════════════════════════════════════════════════════════════

// void updateBatteryPercentage() {
//   // analogReadMilliVolts reads the pin using internal ADC calibration
//   // Assumes a 1:1 voltage divider (e.g. two 100k resistors) cutting battery voltage in half
//   uint32_t pinMilliVolts = analogReadMilliVolts(BATT_PIN);
//   uint32_t batteryMilliVolts = pinMilliVolts * 2; 

//   float vBatt = batteryMilliVolts / 1000.0;

//   // Standard LiPo mapping: 3.2V (empty) to 4.2V (full)
//   float pct = (vBatt - 3.2) / (4.2 - 3.2) * 100.0;
//   batteryPct = constrain((int)pct, 0, 100);
// }

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
  // wm.resetSettings();  // uncomment to wipe saved credentials

  if (sizeof(HOTSPOT_PASSWORD) > 1) {   // non-empty password in secret.h
    wm.autoConnect(AP_NAME, HOTSPOT_PASSWORD);
  } else {
    wm.autoConnect(AP_NAME);
  }
  while (WiFi.status() != WL_CONNECTED) delay(100);
  Serial.println("WiFi connected");

  // ── MQTT ─────────────────────────────────────────────────
  wifiClient.setInsecure();
  mqttClient.setServer(MQTT_SERVER_ADDR, MQTT_SERVER_PORT);
  mqttClient.setCallback(onMessageReceived);

  while (!mqttClient.connected()) {
    Serial.print("MQTT connect... ");
    if (mqttClient.connect("Ebeep_Device", MQTT_USERNAME_STR, MQTT_PASSWORD_STR)) {
      Serial.println("OK");
      mqttClient.subscribe(MQTT_INBOX_TOPIC);
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
