#pragma once
#include <esp_sleep.h>
#include <time.h>

// ═══════════════════════════════════════════════════════════════
//  Callbacks.h
//  Buttons, display refresh, MQTT callbacks, and deep-sleep power
//  management — merged into one file so every function is defined
//  before it's used (checkButtons() and drawCurrentScreen() are
//  needed by the power-management refresh helpers below them).
//  Must be included AFTER all state files.
// ═══════════════════════════════════════════════════════════════

void updateBattery() {
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) sum += analogReadMilliVolts(A0);
  // avg mV at ADC, ×2 for divider → vBat in mV
  uint32_t vBat = (sum / 16) * 2;
  // 3000–4200 mV range → 0–100%
  batteryPct = (int)(((int)vBat - 3000) * 100 / 1200);
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
        default: break;
      }
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


// Draws whatever the current screen state is. Shared by the normal
// content refresh and the periodic status-only refresh below, so the
// two never drift out of sync with each other.
void drawCurrentScreen() {
  switch (currentState) {
    case STATE_HOME:      drawHome();      break;
    case STATE_INBOX:     drawInbox();     break;
    case STATE_COMPOSE:   drawCompose();   break;
    case STATE_SENT:      drawSent();      break;
    case STATE_GAMES:     drawGames();     break;
    case STATE_WIFI:      drawWifi();      break;
    case STATE_TICTACTOE: drawTicTacToe(); break;
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
    // dispatch manually
    switch (currentState) {
      case STATE_HOME:      handleHomeInput(l, m, r);       break;
      case STATE_INBOX:     handleInboxInput(l, m, r);      break;
      case STATE_COMPOSE:   handleComposeInput(l, m, r);    break;
      case STATE_SENT:      handleSentInput(l, m, r);       break;
      case STATE_GAMES:     handleGamesInput(l, m, r);      break;
      case STATE_TICTACTOE: handleTicTacToeInput(l, m, r);  break;
      default: break;
    }
  }
}


// ═══════════════════════════════════════════════════════════════
//  Power Management
//
//  Tiers, checked in priority order:
//    1. Active   — no deep sleep, WiFi modem-sleep only, for
//                  ACTIVE_TIMEOUT_MS after a button press or message.
//    2. Low batt — wake every WAKE_INTERVAL_LOWBATT_S  (batt < LOW_BATT_PCT)
//    3. Night    — wake every WAKE_INTERVAL_NIGHT_S    (00:00-08:00)
//    4. Default  — wake every WAKE_INTERVAL_DEFAULT_S
// ═══════════════════════════════════════════════════════════════

RTC_DATA_ATTR bool clockSynced = false;  // NTP only needs to succeed once — RTC keeps ticking through deep sleep

// True during 00:00–08:00. Relies on the RTC clock rather than a fresh
// NTP call every wake, since the RTC keeps accurate time through deep sleep.
bool isNightHours() {
  time_t now = time(nullptr);
  if (now < 100000) return false;  // clock never synced yet — assume day
  struct tm t;
  localtime_r(&now, &t);
  return t.tm_hour >= NIGHT_START_HOUR && t.tm_hour < NIGHT_END_HOUR;
}

void syncNightClock() {
  if (clockSynced) return;
  configTzTime(TZ_STRING, "pool.ntp.org", "time.nist.gov");
  struct tm t;
  if (getLocalTime(&t, 2000)) clockSynced = true;  // 2s best-effort, don't hang the boot on a bad network
}

unsigned long currentWakeIntervalS() {
  if (batteryPct < LOW_BATT_PCT) return WAKE_INTERVAL_LOWBATT_S;
  if (isNightHours())            return WAKE_INTERVAL_NIGHT_S;
  return WAKE_INTERVAL_DEFAULT_S;
}

void refreshStatusBarOnly() {
  display.setPartialWindow(0, 0, SCREEN_W, SCREEN_H);
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    drawStatusBar(batteryPct);
    drawCurrentScreen();
    checkButtons();  // don't miss a press while this partial window is transferring
  } while (display.nextPage());
}

// WiFi with a hard timeout instead of WiFiManager's blocking portal —
// credentials are already saved by now, this is just "is the radio good".
bool quickWifiConnect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin();
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(50);
  }
  return WiFi.status() == WL_CONNECTED;
}

// Never returns — reboots the chip into deep sleep, waking on a timer
// or on any button press.
void enterDeepSleep(unsigned long seconds) {
  WiFi.disconnect(true);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON); // Keeps the buttons powered during sleep.
  esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000000ULL);
  uint64_t sleep_wake_mask = (1ULL << BTN_LEFT) | (1ULL << BTN_SELECT);
  esp_deep_sleep_enable_gpio_wakeup(sleep_wake_mask, ESP_GPIO_WAKEUP_GPIO_LOW);
  esp_deep_sleep_start();
}

//Connects just long enough to catch a pending message repaint the battery readout and
// go straight back to sleep.
bool quickCheckAndMaybeSleep() {
  updateBattery();

  bool gotMessage = false;
  if (quickWifiConnect()) {
    wifiClient.setInsecure();
    if (connectMQTT()) {
      unsigned long start = millis();
      while (millis() - start < MQTT_LISTEN_WINDOW_MS) {
        mqttClient.loop();
        if (hasUnreadMessage) { gotMessage = true; break; }
        delay(10);  // don't spin the CPU at full power for this whole window
      }
    }
  }

  if (gotMessage) return false;

  currentState = STATE_HOME;
  refreshStatusBarOnly();
  enterDeepSleep(currentWakeIntervalS());
  return true;  // unreachable
}

bool sleepWouldLoseState() {
  return inTicTacToe || TTT_pendingStart || messageLen > 0;
}

// Called every loop() tick. Drops to deep sleep once the active window
// (ACTIVE_TIMEOUT_MS since the last button press or message) has elapsed.
void handlePowerState() {
  if (millis() - lastActivityMillis < ACTIVE_TIMEOUT_MS) return;
  if (sleepWouldLoseState()) return;
  enterDeepSleep(currentWakeIntervalS());
}


// ── MQTT Callbacks ────────────────────────────────────────────
// Handles the broad "<id>/games" channel — invite-level presence messages
// (TTT_START / TTT_START_ACK / TTT_LEFT).
void gameReqHandler(char* payload) {
  if (strcmp(payload, "TTT_START") == 0) {
    if (TTT_pendingStart) {
      // Both sides pressed Play at the same moment — neither has seen the
      // other's invite yet. Deterministic tiebreak so exactly one becomes
      // initiator (X) and the other joins (O); see TTT_pendingStart usage.
      if (strcmp(BEEPER_ID, RECIVER_ID) < 0) TTT_becomeInitiator();
      else                                    TTT_joinAsOpponent();
    } else if (!inTicTacToe) {
      TTT_hasOpponent = true;  // they invited us — show the notification dot
    }
    // else: already in a confirmed game — stray message, ignore.
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
  if (currentState == STATE_HOME || currentState == STATE_GAMES || currentState == STATE_TICTACTOE) {
    needRefresh = true;
    fastUpdate  = true;
  }
}

void onMessageReceived(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  markActivity();  // any inbound message keeps the device awake to let the user respond

  const size_t idLen = strlen(BEEPER_ID);
  char* subTopic = topic + idLen;

  if (strcmp(subTopic, "/inbox") == 0) {
    Serial.println("Got msg in Inbox");
    uint8_t msgLen = (length < MAX_MSG_LEN) ? (uint8_t)length : MAX_MSG_LEN;
    memcpy(lastReceivedMessage, payload, msgLen);
    lastReceivedMessage[msgLen] = '\0';
    hasUnreadMessage = true;
    if (currentState == STATE_HOME) {
      needRefresh = true;
      fastUpdate  = true;
    } else if (currentState == STATE_INBOX) {
      needRefresh = true;
      fastUpdate  = false;
    }
  } else if (strcmp(subTopic, "/games") == 0) {
    Serial.println("Got msg in games");
    gameReqHandler((char*)payload);
  } else if (strncmp(subTopic, "/games/", 7) == 0) {
    if (strcmp(subTopic + 7, "TTT") == 0) {
      Serial.println("Got msg in TTT");
      checkTicTacToeMessages((char*)payload);
    }
  }
}