#include <Arduino.h>
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>


#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);



#include "config.h"

// ═══════════════════════════════════════════════════════════════
//  DISPLAY DRIVER
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
int batteryPct = 67;  // placeholder


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
//  BUTTON READING
// ═══════════════════════════════════════════════════════════════
void checkButtons() {
  bool curLeft  = digitalRead(BTN_LEFT);
  bool curMid   = digitalRead(BTN_SELECT);
  bool curRight = digitalRead(BTN_RIGHT);

  bool leftPressed  = (curLeft  == LOW && lastLeft  == HIGH);
  bool midPressed   = (curMid   == LOW && lastMid   == HIGH);
  bool rightPressed = (curRight == LOW && lastRight == HIGH);

  unsigned long now = millis();

  // ── Fresh press (edge detection) ──────────────────────────
  if ((leftPressed || midPressed || rightPressed) &&
      (now - lastBtnTime > DEBOUNCE_MS)) {
    lastBtnTime = now;

    // Start tracking a hold if it's left or right
    if (leftPressed || rightPressed) {
      heldButton    = leftPressed ? -1 : 1;
      holdStartTime = now;
      lastRepeatAt  = now;
    } else {
      heldButton = 0;  // mid press cancels any hold
    }

    switch (currentState) {
      case STATE_HOME:    handleHomeInput   (leftPressed, midPressed, rightPressed); break;
      case STATE_INBOX:   handleInboxInput  (leftPressed, midPressed, rightPressed); break;
      case STATE_COMPOSE: handleComposeInput(leftPressed, midPressed, rightPressed); break;
      case STATE_SENT:    handleSentInput   (leftPressed, midPressed, rightPressed); break;
      case STATE_GAMES:   handleGamesInput  (leftPressed, midPressed, rightPressed); break;
    }
  }

  // ── Hold-scroll repeat ────────────────────────────────────
  // Only fires for Compose and Games, only on left/right hold
  if (heldButton != 0 &&
      (currentState == STATE_COMPOSE || currentState == STATE_GAMES)) {

    bool stillHeld = (heldButton == -1) ? (curLeft  == LOW)
                                        : (curRight == LOW);

    if (!stillHeld) {
      // Button released — cancel hold
      heldButton = 0;
    } else if ((now - holdStartTime >= HOLD_DELAY_MS) &&
               (now - lastRepeatAt  >= HOLD_REPEAT_MS)) {
      // Held long enough — fire a repeat
      lastRepeatAt = now;
      bool fakeLeft  = (heldButton == -1);
      bool fakeRight = (heldButton ==  1);

      switch (currentState) {
        case STATE_COMPOSE: handleComposeInput(fakeLeft, false, fakeRight); break;
        case STATE_GAMES:   handleGamesInput  (fakeLeft, false, fakeRight); break;
        default: break;
      }
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
//  MQTT
// ═══════════════════════════════════════════════════════════════


  void onMessageReceived(char* topic, byte* payload, unsigned int length) {
    unsigned int msgLen = (length < MAX_MSG_LEN) ? length : MAX_MSG_LEN;
    memcpy(lastReceivedMessage, payload, msgLen);
    lastReceivedMessage[msgLen] = '\0';
    hasUnreadMessage = true;
    if (currentState == STATE_HOME) {
      needRefresh = true;
      fastUpdate = true;
    } else if (currentState == STATE_INBOX) {
      needRefresh = true;
      fastUpdate = false;
    }

    // TODO: trigger buzzer melody here
  }



// ═══════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(3000); // give time for serial monitor to connect
  Serial.println("=========Boot==========");

  pinMode(EPD_CS,   OUTPUT);
  pinMode(EPD_DC,   OUTPUT);
  pinMode(EPD_RST,  OUTPUT);
  pinMode(EPD_BUSY, INPUT);

  // Buttons
  pinMode(BTN_LEFT,   INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BTN_RIGHT,  INPUT_PULLUP);

  // Display init
  SPI.begin();
  display.init(115200, true, 50, false);
  display.setRotation(1);
  //display.clearScreen();

  //===========WIFI SETUP===========
  currentState = STATE_WIFI;
  refreshDisplay();
  Serial.print("Connecting to WiFi...");
  WiFiManager wifiManager;

// reset settings - wipe stored credentials for testing
//wifiManager.resetSettings();

  bool res;
  //res = wifiManager.autoConnect(AP_name, AP_pass);
  if (AP_pass[0] != '\0') {
    res = wifiManager.autoConnect(AP_name, AP_pass); // password protected ap
    Serial.print("password");
  } else {
    res = wifiManager.autoConnect(AP_name);
    Serial.print("no password");
  }
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("Connected to WiFi!");


  // ===========MQTT SETUP===========
  wifiClient.setInsecure();
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(onMessageReceived);
  while (!mqttClient.connected()) {
      Serial.print("Connecting to MQTT... server: ");
      Serial.print(mqttServer);
      Serial.print(" port: ");
      Serial.println(mqttPort);
      
      if (mqttClient.connect("Ebeep_Device", mqttUser, mqttPass)) {
          Serial.println("connected!");
          mqttClient.subscribe(mqttInboxTopic);
      } else {
          Serial.print("failed, rc=");
          Serial.print(mqttClient.state());
          Serial.println(" — retrying in 1s");
          delay(1000);
      }
  }

  currentState = STATE_HOME;
  //updateBatteryPercentage();
  needRefresh = true;
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
    // if (!fastUpdate) {
    //   updateBatteryPercentage();
    // }
  }

  //Call mqttClient.loop() here to receive incoming messages.
  mqttClient.loop();
}
