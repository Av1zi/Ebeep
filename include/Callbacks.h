#pragma once
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <Preferences.h>

// ═══════════════════════════════════════════════════════════════
//  Callbacks.h
//  Buttons, display refresh, MQTT callbacks, and deep-sleep power
//  management — merged into one file so every function is defined
//  before it's used (checkButtons() and drawCurrentScreen() are
//  needed by the power-management refresh helpers below them).
//  Must be included AFTER all state files.
// ═══════════════════════════════════════════════════════════════

RTC_DATA_ATTR static float battVoltageEma = 0;  // mV, actual pack voltage (post-divider math). Kept across deep sleep so the % doesn't jump around between wakes

void updateBattery() {
  // 8 samples is enough noise averaging on its own — the EMA below does
  // the real smoothing across calls, so we don't burn extra ADC time here.
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogReadMilliVolts(A0);
  const float vBatSample = (sum / 8.0f) * BATTERY_DIVIDER_RATIO;

  battVoltageEma = (battVoltageEma == 0)
                     ? vBatSample
                     : battVoltageEma + BATTERY_EMA_ALPHA * (vBatSample - battVoltageEma);

  batteryPct = (int)((battVoltageEma - BATTERY_MIN_MV) * 100.0f / (BATTERY_MAX_MV - BATTERY_MIN_MV));
  batteryPct = constrain(batteryPct, 0, 100);
}

// Resets the active-mode timer. Call on any button press or incoming message.
void markActivity() {
  lastActivityMillis = millis();
}


bool pendingLeft  = false;
bool pendingMid   = false;
bool pendingRight = false;
bool isRefreshing = false;

// ── Button Handler ────────────────────────────────────────────
// Defined first so refreshDisplay() can call it.
void checkButtons() {
  bool curLeft  = digitalRead(BTN_LEFT);
  bool curMid   = digitalRead(BTN_SELECT);
  bool curRight = digitalRead(BTN_RIGHT);

  bool leftPressed  = (curLeft  == LOW && lastLeft  == HIGH);
  bool midPressed   = (curMid   == LOW && lastMid   == HIGH);
  bool rightPressed = (curRight == LOW && lastRight == HIGH);

  const unsigned long now = millis();

  if ((leftPressed || midPressed || rightPressed) && (now - lastBtnTime > DEBOUNCE_MS)) {
    lastBtnTime = now;
    markActivity();  // any press keeps the device in the active (modem-sleep) tier
    if (isRefreshing) {
      pendingLeft  |= leftPressed;
      pendingMid   |= midPressed;
      pendingRight |= rightPressed;
    } else {
      if (leftPressed || rightPressed) {
        heldButton    = leftPressed ? -1 : 1;
        holdStartTime = now;
        lastRepeatAt  = now;
      } else {
        heldButton = 0;
      }

      switch (currentState) {
        case STATE_HOME:      handleHomeInput      (leftPressed, midPressed, rightPressed); break;
        case STATE_INBOX:     handleInboxInput     (leftPressed, midPressed, rightPressed); break;
        case STATE_COMPOSE:   handleComposeInput   (leftPressed, midPressed, rightPressed); break;
        case STATE_SENT:      handleSentInput      (leftPressed, midPressed, rightPressed); break;
        case STATE_GAMES:     handleGamesInput     (leftPressed, midPressed, rightPressed); break;
        case STATE_TICTACTOE: handleTicTacToeInput (leftPressed, midPressed, rightPressed); break;
        case STATE_CONNECT4:  handleConnect4Input  (leftPressed, midPressed, rightPressed); break;
        case STATE_BLACKJACK: handleBlackjackInput(leftPressed, midPressed, rightPressed); break;
        default: break;
      }
    }
  }

  // Hold-scroll repeat (Compose, Games, and in-game selectors)
  if (heldButton != 0 &&
      (currentState == STATE_COMPOSE || currentState == STATE_GAMES ||
       currentState == STATE_TICTACTOE || currentState == STATE_CONNECT4 ||
       currentState == STATE_BLACKJACK)) {
    const bool stillHeld = (heldButton == -1) ? (curLeft == LOW) : (curRight == LOW);

    if (!stillHeld) {
      heldButton = 0;
    } else if ((now - holdStartTime >= HOLD_DELAY_MS) &&
               (now - lastRepeatAt  >= HOLD_REPEAT_MS)) {
      lastRepeatAt = now;
      const bool fL = (heldButton == -1);
      const bool fR = (heldButton ==  1);
      switch (currentState) {
        case STATE_COMPOSE:   handleComposeInput   (fL, false, fR); break;
        case STATE_GAMES:     handleGamesInput     (fL, false, fR); break;
        case STATE_TICTACTOE: handleTicTacToeInput (fL, false, fR); break;
        case STATE_CONNECT4:  handleConnect4Input  (fL, false, fR); break;
        case STATE_BLACKJACK: handleBlackjackInput (fL, false, fR); break;
        default: break;
      }
    }
  }

  lastLeft  = curLeft;
  lastMid   = curMid;
  lastRight = curRight;
}


// Draws whatever the current screen state is.
void drawCurrentScreen() {
  switch (currentState) {
    case STATE_HOME:      drawHome();      break;
    case STATE_INBOX:     drawInbox();     break;
    case STATE_COMPOSE:   drawCompose();   break;
    case STATE_SENT:      drawSent();      break;
    case STATE_GAMES:     drawGames();     break;
    case STATE_WIFI:      drawWifi();      break;
    case STATE_PORTAL:    drawPortal();    break;
    case STATE_SLEEP:     drawSleeping();  break;
    case STATE_TICTACTOE: drawTicTacToe(); break;
    case STATE_CONNECT4:  drawConnect4();  break;
    case STATE_BLACKJACK: drawBlackjack(); break;
  }
}


// ── Display Refresh ───────────────────────────────────────────
// Calls checkButtons() mid-refresh so button presses aren't lost
// while the e-ink panel is transferring data.
void refreshDisplay() {
  isRefreshing = true;

  if (fastUpdate) {
    display.setPartialWindow(0, CONTENT_Y + 2, SCREEN_W, SCREEN_H - CONTENT_Y);
  } else {
    display.setFullWindow();
  }

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    if (!fastUpdate) drawStatusBar(batteryPct);
    drawCurrentScreen();
    checkButtons();
  } while (display.nextPage());

  fastUpdate = false;
  isRefreshing = false;

  // Replay any button pressed during refresh
   if (pendingLeft || pendingMid || pendingRight) {
    bool l = pendingLeft, m = pendingMid, r = pendingRight;
    pendingLeft = pendingMid = pendingRight = false;
    switch (currentState) {
      case STATE_HOME:      handleHomeInput(l, m, r);       break;
      case STATE_INBOX:     handleInboxInput(l, m, r);      break;
      case STATE_COMPOSE:   handleComposeInput(l, m, r);    break;
      case STATE_SENT:      handleSentInput(l, m, r);       break;
      case STATE_GAMES:     handleGamesInput(l, m, r);      break;
      case STATE_TICTACTOE: handleTicTacToeInput(l, m, r);  break;
      case STATE_CONNECT4:  handleConnect4Input  (l, m, r); break;
      case STATE_BLACKJACK: handleBlackjackInput(l, m, r); break;
      default: break;
    }
  }
}


// ═══════════════════════════════════════════════════════════════
//  Power Management
//
//  The device is deep asleep unless someone is using it:
//    - A press on BTN_LEFT / BTN_SELECT wakes it and it connects.
//    - It stays awake for ACTIVE_TIMEOUT_MS after the last press
//      (or incoming message), then shows "Sleeping" and powers down.
//  There is no timer wake, so a sleeping device draws only the
//  chip's deep-sleep current. Messages are retained on the broker,
//  so the last one is waiting for us on the next wake.
// ═══════════════════════════════════════════════════════════════

bool displayAwake      = false;  // display.init() ran since this boot
bool pendingInboxClear = false;  // retained inbox copy still needs wiping on the broker
RTC_DATA_ATTR bool inboxRetainCleared = true;  // broker has no retained copy of lastReceivedMessage

void initDisplay() {
  if (displayAwake) return;
  display.init(115200, true, 50, false);
  display.setRotation(3);
  displayAwake = true;
}

// Cheap periodic refresh (battery %, WiFi/MQTT icons only). Content is left
// alone — it only ever changes via an explicit needRefresh trigger, never
// silently, so redrawing it here on a timer would be wasted e-ink time and power.
void refreshStatusBarOnly() {
  display.setPartialWindow(0, 0, SCREEN_W, DIVIDER_TOP + 1);
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    drawStatusBar(batteryPct);
    checkButtons();
  } while (display.nextPage());
}

bool anyButtonDown() {
  return digitalRead(BTN_LEFT) == LOW || digitalRead(BTN_SELECT) == LOW || digitalRead(BTN_RIGHT) == LOW;
}


// ── Known WiFi networks ───────────────────────────────────────
// Every network that ever worked is kept in flash (most recent first),
// so moving between home / work / a phone hotspot needs no portal.

struct KnownNetwork {
  char ssid[33];
  char pass[65];
};

KnownNetwork knownNets[MAX_KNOWN_NETWORKS];
uint8_t      knownCount = 0;
Preferences  wifiPrefs;

// Last network that worked — survives deep sleep, so the usual case
// (same place as last time) connects without scanning.
RTC_DATA_ATTR char    cachedSsid[33] = "";
RTC_DATA_ATTR int32_t cachedChannel  = 0;
RTC_DATA_ATTR uint8_t cachedBssid[6];

void loadKnownNetworks() {
  wifiPrefs.begin("wifi", true);
  knownCount = min<uint8_t>(wifiPrefs.getUChar("n", 0), MAX_KNOWN_NETWORKS);
  for (uint8_t i = 0; i < knownCount; i++) {
    char key[4];
    snprintf(key, sizeof(key), "s%u", i);
    wifiPrefs.getString(key, knownNets[i].ssid, sizeof(knownNets[i].ssid));
    snprintf(key, sizeof(key), "p%u", i);
    wifiPrefs.getString(key, knownNets[i].pass, sizeof(knownNets[i].pass));
  }
  wifiPrefs.end();
}

void storeKnownNetworks() {
  wifiPrefs.begin("wifi", false);
  wifiPrefs.putUChar("n", knownCount);
  for (uint8_t i = 0; i < knownCount; i++) {
    char key[4];
    snprintf(key, sizeof(key), "s%u", i);
    wifiPrefs.putString(key, knownNets[i].ssid);
    snprintf(key, sizeof(key), "p%u", i);
    wifiPrefs.putString(key, knownNets[i].pass);
  }
  wifiPrefs.end();
}

// Moves (or adds) a network to the front of the list. The oldest one falls
// off once the list is full.
void rememberNetwork(const char* ssid, const char* pass) {
  if (ssid[0] == '\0') return;
  int8_t found = -1;
  for (uint8_t i = 0; i < knownCount; i++) {
    if (strcmp(knownNets[i].ssid, ssid) == 0) { found = i; break; }
  }
  if (found == 0 && strcmp(knownNets[0].pass, pass) == 0) return;  // already first, nothing to write

  const uint8_t shiftFrom = (found >= 0) ? found : min<uint8_t>(knownCount, MAX_KNOWN_NETWORKS - 1);
  for (int8_t i = shiftFrom; i > 0; i--) knownNets[i] = knownNets[i - 1];
  if (found < 0 && knownCount < MAX_KNOWN_NETWORKS) knownCount++;

  strlcpy(knownNets[0].ssid, ssid, sizeof(knownNets[0].ssid));
  strlcpy(knownNets[0].pass, pass, sizeof(knownNets[0].pass));
  storeKnownNetworks();
}

void cacheWifiAp() {
  if (WiFi.status() != WL_CONNECTED) return;
  strlcpy(cachedSsid, WiFi.SSID().c_str(), sizeof(cachedSsid));
  memcpy(cachedBssid, WiFi.BSSID(), sizeof(cachedBssid));
  cachedChannel = WiFi.channel();
}

const KnownNetwork* findKnown(const char* ssid) {
  for (uint8_t i = 0; i < knownCount; i++) {
    if (strcmp(knownNets[i].ssid, ssid) == 0) return &knownNets[i];
  }
  return nullptr;
}

bool waitForWifi(unsigned long timeoutMs) {
  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) delay(20);
  return WiFi.status() == WL_CONNECTED;
}

// Step 1 — kick off a connect to the last network that worked, without
// waiting. Lets the "Connecting..." screen draw while the radio works.
bool beginLastNetwork() {
  WiFi.mode(WIFI_STA);

  // First boot after updating: pick up the network the old firmware /
  // WiFiManager saved, so nobody has to go through the portal again.
  if (knownCount == 0) {
    wifi_config_t conf;
    if (esp_wifi_get_config(WIFI_IF_STA, &conf) == ESP_OK && conf.sta.ssid[0] != '\0') {
      char ssid[33] = {0}, pass[65] = {0};
      memcpy(ssid, conf.sta.ssid,     sizeof(conf.sta.ssid));
      memcpy(pass, conf.sta.password, sizeof(conf.sta.password));
      rememberNetwork(ssid, pass);
    }
  }
  // Our own list is the source of truth — keep the driver from rewriting
  // its flash copy (channel/BSSID pins included) on every connect.
  esp_wifi_set_storage(WIFI_STORAGE_RAM);

  const KnownNetwork* last = (cachedChannel > 0) ? findKnown(cachedSsid) : nullptr;
  if (!last) return false;
  WiFi.begin(last->ssid, last->pass, cachedChannel, cachedBssid);
  return true;
}

// Step 2 — finish the fast attempt, then scan and try every known network
// in range, strongest first.
bool connectKnownWifi(bool fastAttemptStarted) {
  if (fastAttemptStarted && waitForWifi(WIFI_FAST_CONNECT_MS)) { cacheWifiAp(); return true; }
  cachedChannel = 0;   // that AP isn't around (or changed) — don't try it blind next time
  if (knownCount == 0) return false;

  WiFi.disconnect();
  const int16_t found = WiFi.scanNetworks();   // the driver returns them strongest first
  for (int16_t i = 0; i < found; i++) {
    const KnownNetwork* net = findKnown(WiFi.SSID(i).c_str());
    if (!net) continue;
    Serial.printf("Trying %s\n", net->ssid);
    WiFi.begin(net->ssid, net->pass, WiFi.channel(i), WiFi.BSSID(i));
    if (waitForWifi(WIFI_CONNECT_TIMEOUT_MS)) {
      WiFi.scanDelete();
      rememberNetwork(net->ssid, net->pass);   // most recently used goes to the front
      cacheWifiAp();
      return true;
    }
    WiFi.disconnect();
  }
  WiFi.scanDelete();
  return false;
}

// Step 3 — nothing known in range: open the setup hotspot and say so on
// screen. Any button press cancels it; it also gives up on its own after
// WIFI_PORTAL_TIMEOUT_S. Returns true once the user picked a network.
WiFiManager wm;

bool runWifiPortal() {
  currentState = STATE_PORTAL;
  fastUpdate   = false;
  refreshDisplay();

  wm.setConfigPortalBlocking(false);
  wm.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_S);
  if (sizeof(HOTSPOT_PASSWORD) > 1) wm.startConfigPortal(AP_NAME, AP_password);
  else                              wm.startConfigPortal(AP_NAME);

  const unsigned long start = millis();
  bool armed = !anyButtonDown();   // ignore the press that woke us until it's released
  while (true) {
    if (wm.process()) break;   // connected with the new credentials
    if (!wm.getConfigPortalActive() || millis() - start > WIFI_PORTAL_TIMEOUT_S * 1000UL) return false;
    const bool down = anyButtonDown();
    if (!armed && !down) armed = true;
    else if (armed && down) { wm.stopConfigPortal(); return false; }
    delay(10);
  }

  wm.stopConfigPortal();
  WiFi.mode(WIFI_STA);   // hotspot off
  rememberNetwork(WiFi.SSID().c_str(), WiFi.psk().c_str());
  cacheWifiAp();
  return true;
}


// ── Retained inbox ────────────────────────────────────────────
// Wipes the retained copy of our inbox on the broker, so the next wake
// doesn't get handed the same message again.
void flushInboxClear() {
  if (!pendingInboxClear || !mqttClient.connected()) return;
  static char inboxTopic[24] = "";
  if (inboxTopic[0] == '\0') snprintf(inboxTopic, sizeof(inboxTopic), "%s/inbox", BEEPER_ID);
  if (mqttClient.publish(inboxTopic, "", true)) {
    pendingInboxClear  = false;
    inboxRetainCleared = true;
  }
}

void markInboxRead() {
  hasUnreadMessage  = false;
  pendingInboxClear = true;
  flushInboxClear();   // if we're offline, loop() retries once MQTT is back
}


// ── Sleep ─────────────────────────────────────────────────────
// Never returns — shows the "Sleeping" screen, then drops the chip into
// deep sleep until BTN_LEFT / BTN_SELECT is pressed.
void enterDeepSleep() {
  if (mqttClient.connected()) mqttClient.disconnect();   // flushes anything we just published
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  netWifiOk = netMqttOk = false;
  currentState = STATE_SLEEP;
  fastUpdate   = false;
  initDisplay();
  refreshDisplay();

  // Put the panel's controller into its own deep sleep. Without this the booster
  // stays powered after a partial refresh and drains the battery the whole night.
  display.hibernate();

  // Don't go down while a button is still held — ext1 would wake us instantly.
  const unsigned long heldSince = millis();
  while (anyButtonDown() && millis() - heldSince < 3000) delay(10);

  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON); // keeps the button pull-ups powered during sleep
  const uint64_t wakeMask = (1ULL << BTN_LEFT) | (1ULL << BTN_SELECT);
  esp_sleep_enable_ext1_wakeup(wakeMask, ESP_EXT1_WAKEUP_ANY_LOW);

  esp_deep_sleep_start();
}

bool sleepWouldLoseState() {
  return inTicTacToe || TTT_pendingStart || inConnect4 || C4_pendingStart || messageLen > 0;
}

// Called every loop() tick. Drops to deep sleep once the active window
// (ACTIVE_TIMEOUT_MS since the last button press or message) has elapsed.
void handlePowerState() {
  const unsigned long idle = millis() - lastActivityMillis;
  if (idle < ACTIVE_TIMEOUT_MS) return;

  if (sleepWouldLoseState()) {
    if (idle < IDLE_HARD_TIMEOUT_MS) return;
    // Nobody has touched it in ages — drop the game / draft instead of
    // keeping WiFi on all night waiting for a move that isn't coming.
    if (inTicTacToe || TTT_pendingStart) leaveTicTacToe();
    if (inConnect4  || C4_pendingStart)  leaveConnect4();
    typedMessage[0] = '\0';
    messageLen      = 0;
  }
  enterDeepSleep();
}

// ── MQTT Callbacks ────────────────────────────────────────────
void gameReqHandler(char* payload) {
  if (strcmp(payload, "TTT_START") == 0) {
    if (TTT_pendingStart) {
      if (strcmp(BEEPER_ID, RECIVER_ID) < 0) TTT_becomeInitiator();
      else                                   TTT_joinAsOpponent();
    } else if (inTicTacToe) {
      inTicTacToe     = false;
      TTT_hasOpponent = true;
      TTT_flags       = TTT_NONE;
      if (currentState == STATE_TICTACTOE) {
        currentState = STATE_HOME;
        needRefresh  = true;
        fastUpdate   = false;
      }
    } else {
      TTT_hasOpponent = true;
    }
  } else if (strcmp(payload, "TTT_START_ACK") == 0) {
    if (TTT_pendingStart) TTT_becomeInitiator();
  } else if (strcmp(payload, "TTT_LEFT") == 0) {
    TTT_hasOpponent  = false;
    TTT_pendingStart = false;
    if (currentState == STATE_TICTACTOE) {
      inTicTacToe  = false;
      TTT_flags    = TTT_NONE;
      currentState = STATE_HOME;
      needRefresh  = true;
      fastUpdate   = false;
      return;
    }
  }

  // ── Connect 4 Logic ──
  else if (strcmp(payload, "C4_START") == 0) {
    if (C4_pendingStart) {
      if (strcmp(BEEPER_ID, RECIVER_ID) < 0) C4_becomeInitiator();
      else                                   C4_joinAsOpponent();
    } else if (inConnect4) {
      inConnect4     = false;
      C4_hasOpponent = true;
      C4_flags       = C4_NONE;
      if (currentState == STATE_CONNECT4) {
        currentState = STATE_HOME;
        needRefresh  = true;
        fastUpdate   = false;
      }
    } else {
      C4_hasOpponent = true;
    }
  } else if (strcmp(payload, "C4_START_ACK") == 0) {
    if (C4_pendingStart) C4_becomeInitiator();
  } else if (strcmp(payload, "C4_LEFT") == 0) {
    C4_hasOpponent  = false;
    C4_pendingStart = false;
    if (currentState == STATE_CONNECT4) {
      inConnect4   = false;
      C4_flags     = C4_NONE;
      currentState = STATE_HOME;
      needRefresh  = true;
      fastUpdate   = false;
      return;
    }
  }

  if (currentState == STATE_HOME || currentState == STATE_GAMES || 
      currentState == STATE_TICTACTOE || currentState == STATE_CONNECT4) {
    needRefresh = true;
    fastUpdate  = true;
  }
}

void onMessageReceived(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';

  const size_t idLen = strlen(BEEPER_ID);
  char* subTopic = topic + idLen;

  if (strcmp(subTopic, "/inbox") == 0) {
    // Empty payload = our own "clear retained" echoing back, not a message.
    if (length == 0) { inboxRetainCleared = true; return; }

    char incoming[MAX_MSG_LEN + 1];
    uint8_t msgLen = (length < MAX_MSG_LEN) ? (uint8_t)length : MAX_MSG_LEN;
    memcpy(incoming, payload, msgLen);
    incoming[msgLen] = '\0';

    // The broker hands us the retained copy on every wake until it's cleared —
    // that's the same message, not a new one, so don't wake up for it.
    if (strcmp(incoming, lastReceivedMessage) == 0 && (hasUnreadMessage || !inboxRetainCleared)) {
      if (!hasUnreadMessage) pendingInboxClear = true;   // read already, the clear just never made it
      return;
    }

    Serial.println("Got msg in Inbox");
    memcpy(lastReceivedMessage, incoming, msgLen + 1);
    hasUnreadMessage   = true;
    inboxRetainCleared = false;
    markActivity();
    if (currentState == STATE_HOME) {
      needRefresh = true;
      fastUpdate  = true;
    } else if (currentState == STATE_INBOX) {
      needRefresh = true;
      fastUpdate  = false;
    }
  } else if (strcmp(subTopic, "/games") == 0) {
    Serial.println("Got msg in games");
    markActivity();
    gameReqHandler((char*)payload);
  } else if (strncmp(subTopic, "/games/", 7) == 0) {
    markActivity();
    if (strcmp(subTopic + 7, "TTT") == 0) {
      Serial.println("Got msg in TTT");
      checkTicTacToeMessages((char*)payload);
    } else if (strcmp(subTopic + 7, "C4") == 0) {
      Serial.println("Got msg in C4");
      checkConnect4Messages((char*)payload);
    }
  }
}