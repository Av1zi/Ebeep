#include <Arduino.h>
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>

#include <WiFi.h>
//#include <WiFiManager.h> add this later on when you move to esp32 right now use the normal wifi library and hardcode the credentials

#include "config.h"

// ═══════════════════════════════════════════════════════════════
//  DISPLAY DRIVER
//  Library: https://github.com/ZinggJM/GxEPD2
// ═══════════════════════════════════════════════════════════════
GxEPD2_BW<GxEPD2_290_BS, GxEPD2_290_BS::HEIGHT> display(
  GxEPD2_290_BS(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);

// ═══════════════════════════════════════════════════════════════
//  SCREEN STATE MACHINE
// ═══════════════════════════════════════════════════════════════
enum ScreenState {
  STATE_HOME,
  STATE_INBOX,
  STATE_COMPOSE,
  STATE_SENT,
  STATE_GAMES,
  STATE_WIFI
};

ScreenState currentState = STATE_WIFI;

// ── Refresh flags ─────────────────────────────────────────────
//  needRefresh  — set to true whenever the screen needs redrawn
//  fastUpdate   — set to true for partial refresh (content area only)
//                 false = full screen refresh (used on state changes)
bool needRefresh = false;
bool fastUpdate  = false;

// ── Message state ─────────────────────────────────────────────
bool hasUnreadMessage = false;
char lastReceivedMessage[MAX_MSG_LEN + 1] = "";  // set by MQTT callback
char typedMessage[MAX_MSG_LEN + 1]        = "";
int  messageLen       = 0;
int  currentLetterIdx = 0;

// ── Timers ────────────────────────────────────────────────────
unsigned long sentEnteredAt = 0;  // millis() when STATE_SENT was entered

// ── Compose sub-state ─────────────────────────────────────────
// Set to true when DEL is pressed on an empty message.
// Triggers a "Quit composing?" popup overlay inside drawCompose().
bool confirmLeaveCompose = false;

// ── Battery ───────────────────────────────────────────────────
//  TODO (ESP32): Read real voltage from a resistor-divider on an ADC pin.
//  Formula: batteryPct = (vBatt - 3.0) / (4.2 - 3.0) * 100, clamped 0–100.
int batteryPct = 62;  // placeholder

// ── Button state tracking ─────────────────────────────────────
bool lastLeft   = HIGH;
bool lastMid    = HIGH;
bool lastRight  = HIGH;

// ═══════════════════════════════════════════════════════════════
//  CODE INJECTION
// ═══════════════════════════════════════════════════════════════
#include "display_utils.h"   // drawText, drawCenteredText, drawStatusBar, drawButtonHints
#include "icons.h"           // drawIconEnvelope, drawIconCompose, drawIconGamepad
#include "HomeState.h"
#include "InboxState.h"
#include "ComposeState.h"    // also defines ALPHABET[], ALPHABET_SIZE, DEL_IDX
#include "SentState.h"
#include "GamesState.h"
#include "wifiState.h"


// ═══════════════════════════════════════════════════════════════
//  BUTTON READING
// ═══════════════════════════════════════════════════════════════
void checkButtons() {
  bool curLeft  = digitalRead(BTN_LEFT);
  bool curMid   = digitalRead(BTN_SELECT);
  bool curRight = digitalRead(BTN_RIGHT);

  // Edge detection: only fire on the transition HIGH→LOW (button just pressed)
  bool leftPressed  = (curLeft  == LOW && lastLeft  == HIGH);
  bool midPressed   = (curMid   == LOW && lastMid   == HIGH);
  bool rightPressed = (curRight == LOW && lastRight == HIGH);

  if (leftPressed || midPressed || rightPressed) {
    delay(DEBOUNCE_MS);  // simple debounce — good enough for e-ink response times

    switch (currentState) {
      case STATE_HOME:    handleHomeInput   (leftPressed, midPressed, rightPressed); break;
      case STATE_INBOX:   handleInboxInput  (leftPressed, midPressed, rightPressed); break;
      case STATE_COMPOSE: handleComposeInput(leftPressed, midPressed, rightPressed); break;
      case STATE_SENT:    handleSentInput   (leftPressed, midPressed, rightPressed); break;
      case STATE_GAMES:   handleGamesInput  (leftPressed, midPressed, rightPressed); break;
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
    // ── Partial refresh ─────────────────────────────────────
    // Only updates pixels below the status bar.
    // The status bar (y=0..22) stays untouched on screen — no flicker!
    // Used during letter scrolling in Compose for a snappier feel.
    display.setPartialWindow(0, CONTENT_Y+2, SCREEN_W, SCREEN_H - CONTENT_Y);
  } else {
    // ── Full refresh ────────────────────────────────────────
    // Clears the whole screen and redraws everything including the status bar.
    // Use this on every state transition.
    display.setFullWindow();
  }

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    // Status bar is drawn only on full refreshes.
    // On partial updates it's outside the active window, so it stays as-is.
    if (!fastUpdate) {
      drawStatusBar(batteryPct);
    }

    // State-specific content.
    // Each draw function is responsible for calling drawButtonHints().
    switch (currentState) {
      case STATE_HOME:    drawHome();    break;
      case STATE_INBOX:   drawInbox();   break;
      case STATE_COMPOSE: drawCompose(); break;
      case STATE_SENT:    drawSent();    break;
      case STATE_GAMES:   drawGames();   break;
      case STATE_WIFI:    drawWifi();    break;
    }

  } while (display.nextPage());

  fastUpdate = false;  // reset — next refresh defaults to full unless explicitly set
}

// ═══════════════════════════════════════════════════════════════
//  MQTT STUB
//  TODO: Replace this whole section with real WiFi + MQTT code.
//
// 
//    #include <PubSubClient.h>
//
//  Callback structure:
//    void onMessageReceived(char* topic, byte* payload, unsigned int length) {
//      strncpy(lastReceivedMessage, (char*)payload, MAX_MSG_LEN);
//      lastReceivedMessage[MAX_MSG_LEN] = '\0';
//      hasUnreadMessage = true;
//      currentState     = STATE_INBOX;
//      needRefresh      = true;
//      fastUpdate       = false;

//      // TODO: trigger buzzer melody here
//    }
// ═══════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  // Buttons
  pinMode(BTN_LEFT,   INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BTN_RIGHT,  INPUT_PULLUP);

  // Display init
  SPI.begin();
  display.init(115200, true, 50, false);
  display.setRotation(1);   // landscape
  display.clearScreen();

  // Initial draw
  refreshDisplay();

  // temporary wifi
  int status = WL_IDLE_STATUS;

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    while (true);
  }
  
    while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);

    // wait 3 sec
    delay(1000);
  }
  refreshDisplay();
  delay(3000);

  currentState = STATE_HOME;
  needRefresh = true;
  Serial.print("You're connected to the network");

}

// ═══════════════════════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════════════════════
void loop() {
  // 1. Read buttons
  checkButtons();

  // 2. Auto-dismiss Sent screen after the timer expires
  if (currentState == STATE_SENT && (millis() - sentEnteredAt >= SENT_DISPLAY_MS)) {
    currentState = STATE_HOME;
    needRefresh  = true;
    fastUpdate   = false;
  }

  // 3. Redraw if anything changed
  if (needRefresh) {
    refreshDisplay();
    needRefresh = false;
  }

  // 4. TODO (ESP32): Call mqttClient.loop() here to receive incoming messages.
  //    mqttClient.loop();
}
