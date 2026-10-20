#pragma once
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <time.h>

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
//  Tiers, checked in priority order:
//    1. Active   — no deep sleep, WiFi modem-sleep, for
//                  ACTIVE_TIMEOUT_MS after a button press.
//    2. Low batt — wake every WAKE_INTERVAL_LOWBATT_S  (batt < LOW_BATT_PCT)
//    3. Night    — wake every WAKE_INTERVAL_NIGHT_S    (00:00-08:00)
//    4. Default  — wake every WAKE_INTERVAL_DEFAULT_S
//
//  A timer wake is kept as short as possible: fast WiFi reconnect,
//  grab the retained inbox message, radio off, and only touch the
//  e-ink if something on it actually changed.
// ═══════════════════════════════════════════════════════════════

RTC_DATA_ATTR time_t  lastClockSync  = 0;   // RTC keeps ticking through deep sleep, but drifts — re-sync every CLOCK_RESYNC_S
RTC_DATA_ATTR uint8_t wifiFailStreak = 0;   // consecutive timer wakes without WiFi, drives the backoff
RTC_DATA_ATTR int32_t cachedChannel  = 0;   // 0 = nothing cached, do a normal scan
RTC_DATA_ATTR uint8_t cachedBssid[6];

bool displayAwake      = false;  // display.init() ran since this boot
bool newInboxMessage   = false;  // a genuinely new message arrived this boot
bool pendingInboxClear = false;  // retained inbox copy still needs wiping on the broker
RTC_DATA_ATTR bool inboxRetainCleared = true;  // broker has no retained copy of lastReceivedMessage

bool isNightHours() {
  time_t now = time(nullptr);
  if (now < 100000) return false;
  struct tm t;
  localtime_r(&now, &t);
  return t.tm_hour >= NIGHT_START_HOUR && t.tm_hour < NIGHT_END_HOUR;
}

void syncNightClock() {
  if (lastClockSync != 0 && time(nullptr) - lastClockSync < (time_t)CLOCK_RESYNC_S) return;
  configTzTime(TZ_STRING, "pool.ntp.org", "time.nist.gov");
  struct tm t;
  if (getLocalTime(&t, 2000)) lastClockSync = time(nullptr);
}

unsigned long currentWakeIntervalS() {
  unsigned long s = WAKE_INTERVAL_DEFAULT_S;
  if (batteryPct < LOW_BATT_PCT) s = WAKE_INTERVAL_LOWBATT_S;
  else if (isNightHours())       s = WAKE_INTERVAL_NIGHT_S;
  // No WiFi around (out of range, router off) — every failed wake burns a full
  // connect timeout, so back off instead of hammering it.
  return s << min<uint8_t>(wifiFailStreak, WIFI_FAIL_BACKOFF_MAX);
}

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

// Remember which AP/channel we're on so the next wake can skip the scan.
void cacheWifiAp() {
  if (WiFi.status() != WL_CONNECTED) return;
  memcpy(cachedBssid, WiFi.BSSID(), sizeof(cachedBssid));
  cachedChannel = WiFi.channel();
}

// WiFi with a hard timeout instead of WiFiManager's blocking portal —
// credentials are already saved by now, this is just "is the radio good".
// Uses the cached BSSID/channel first, which skips the ~1-2 s channel scan.
bool quickWifiConnect() {
  WiFi.mode(WIFI_STA);

  wifi_config_t conf;
  if (esp_wifi_get_config(WIFI_IF_STA, &conf) != ESP_OK || conf.sta.ssid[0] == '\0') return false;
  char ssid[33] = {0}, pass[65] = {0};
  memcpy(ssid, conf.sta.ssid,     sizeof(conf.sta.ssid));
  memcpy(pass, conf.sta.password, sizeof(conf.sta.password));

  // RAM only — the channel/BSSID pin must never end up in the saved config.
  esp_wifi_set_storage(WIFI_STORAGE_RAM);

  for (uint8_t attempt = 0; attempt < 2; attempt++) {
    const bool useCache = (attempt == 0 && cachedChannel > 0);
    if (attempt == 1 && cachedChannel == 0) break;   // already did the plain attempt
    if (attempt == 1) { cachedChannel = 0; WiFi.disconnect(); }

    if (useCache) WiFi.begin(ssid, pass, cachedChannel, cachedBssid);
    else          WiFi.begin(ssid, pass);

    const unsigned long start = millis();
    const unsigned long limit = useCache ? WIFI_CONNECT_TIMEOUT_MS / 2 : WIFI_CONNECT_TIMEOUT_MS;
    while (WiFi.status() != WL_CONNECTED && millis() - start < limit) delay(20);
    if (WiFi.status() == WL_CONNECTED) break;
  }

  esp_wifi_set_storage(WIFI_STORAGE_FLASH);  // so the WiFiManager portal can still save new creds
  const bool ok = (WiFi.status() == WL_CONNECTED);
  if (ok) cacheWifiAp();
  return ok;
}

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

bool anyButtonDown() {
  return digitalRead(BTN_LEFT) == LOW || digitalRead(BTN_SELECT) == LOW || digitalRead(BTN_RIGHT) == LOW;
}

// Never returns — drops the chip into deep sleep, waking on a timer
// or on a BTN_LEFT / BTN_SELECT press.
void enterDeepSleep(unsigned long seconds) {
  if (mqttClient.connected()) mqttClient.disconnect();   // flushes anything we just published
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  // Put the panel's controller into its own deep sleep. Without this the booster
  // stays powered after a partial refresh and drains the battery the whole night.
  if (displayAwake) display.hibernate();

  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON); // keeps the button pull-ups powered during sleep

  esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000000ULL);
  const uint64_t wakeMask = (1ULL << BTN_LEFT) | (1ULL << BTN_SELECT);
  esp_sleep_enable_ext1_wakeup(wakeMask, ESP_EXT1_WAKEUP_ANY_LOW);

  esp_deep_sleep_start();
}

// Runs on a timer wake. Goes straight back to sleep unless someone is
// waiting for us in a game or a button is being pressed — then it returns
// with WiFi + MQTT already up and setup() carries on into active mode.
void timerWakeCheck() {
  const bool wifiOk = quickWifiConnect();
  bool mqttOk = false;
  if (wifiOk && connectMQTT()) {
    mqttOk = true;
    const unsigned long start = millis();
    while (millis() - start < MQTT_LISTEN_WINDOW_MS && !newInboxMessage) {
      mqttClient.loop();
      delay(10);
    }
    flushInboxClear();
  }
  wifiFailStreak = wifiOk ? 0 : min<uint8_t>(wifiFailStreak + 1, 8);
  netWifiOk = wifiOk;
  netMqttOk = mqttOk;

  if (TTT_hasOpponent || C4_hasOpponent || anyButtonDown()) return;

  // Radio off before the (slow) e-ink refresh — nothing below needs it.
  if (mqttClient.connected()) mqttClient.disconnect();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  const bool battMoved   = abs(batteryPct - shownBattPct) >= BATTERY_REDRAW_STEP;
  const bool lowFlipped  = (batteryPct <= LOW_BATT_PCT) != (shownBattPct <= LOW_BATT_PCT);
  const bool netFlipped  = (wifiOk != shownWifiOk) || (mqttOk != shownMqttOk);
  if (newInboxMessage || battMoved || lowFlipped || netFlipped || shownBattPct < 0) {
    currentState = wifiOk ? STATE_HOME : STATE_WIFI;
    fastUpdate   = false;
    initDisplay();
    refreshDisplay();
  }

  enterDeepSleep(currentWakeIntervalS());
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
    currentState    = STATE_HOME;
    needRefresh     = true;
  }

  if (needRefresh) {
    needRefresh = false;
    refreshDisplay();
  }
  enterDeepSleep(currentWakeIntervalS());
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
    newInboxMessage    = true;
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