#pragma once

// ═══════════════════════════════════════════════════════════════
//  Callbacks.h
//  Buttons, display refresh, and MQTT message handling.
//  Must be included AFTER all state files (it dispatches to them).
// ═══════════════════════════════════════════════════════════════

// Resets the stay-awake timer. Call on any button press or incoming message.
void markActivity() {
  lastActivityMillis = millis();
}


// ── Input dispatch ────────────────────────────────────────────
// Sends a press to whichever screen is showing.
void dispatchInput(bool left, bool mid, bool right) {
  switch (currentState) {
    case STATE_HOME:      handleHomeInput      (left, mid, right); break;
    case STATE_INBOX:     handleInboxInput     (left, mid, right); break;
    case STATE_COMPOSE:   handleComposeInput   (left, mid, right); break;
    case STATE_SENT:      handleSentInput      (left, mid, right); break;
    case STATE_GAMES:     handleGamesInput     (left, mid, right); break;
    case STATE_TICTACTOE: handleTicTacToeInput (left, mid, right); break;
    case STATE_CONNECT4:  handleConnect4Input  (left, mid, right); break;
    case STATE_BLACKJACK: handleBlackjackInput (left, mid, right); break;
    default: break;   // WiFi / portal / sleep screens take no input
  }
}

// Screens where holding left/right keeps scrolling.
bool screenHasHoldRepeat() {
  return currentState == STATE_COMPOSE   || currentState == STATE_GAMES ||
         currentState == STATE_TICTACTOE || currentState == STATE_CONNECT4 ||
         currentState == STATE_BLACKJACK;
}

bool anyButtonDown() {
  return digitalRead(BTN_LEFT) == LOW || digitalRead(BTN_SELECT) == LOW || digitalRead(BTN_RIGHT) == LOW;
}


// ── Button Handler ────────────────────────────────────────────
// Presses made while the e-ink is refreshing are queued and replayed
// once it's done, so nothing typed during a refresh gets lost.
bool pendingLeft  = false;
bool pendingMid   = false;
bool pendingRight = false;
bool isRefreshing = false;

void checkButtons() {
  const bool curLeft  = digitalRead(BTN_LEFT);
  const bool curMid   = digitalRead(BTN_SELECT);
  const bool curRight = digitalRead(BTN_RIGHT);

  const bool leftPressed  = (curLeft  == LOW && lastLeft  == HIGH);
  const bool midPressed   = (curMid   == LOW && lastMid   == HIGH);
  const bool rightPressed = (curRight == LOW && lastRight == HIGH);

  const unsigned long now = millis();

  if ((leftPressed || midPressed || rightPressed) && (now - lastBtnTime > DEBOUNCE_MS)) {
    lastBtnTime = now;
    markActivity();
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
      dispatchInput(leftPressed, midPressed, rightPressed);
    }
  }

  // Hold-scroll repeat
  if (heldButton != 0 && screenHasHoldRepeat()) {
    const bool stillHeld = (heldButton == -1) ? (curLeft == LOW) : (curRight == LOW);
    if (!stillHeld) {
      heldButton = 0;
    } else if (now - holdStartTime >= HOLD_DELAY_MS && now - lastRepeatAt >= HOLD_REPEAT_MS) {
      lastRepeatAt = now;
      markActivity();
      dispatchInput(heldButton == -1, false, heldButton == 1);
    }
  }

  lastLeft  = curLeft;
  lastMid   = curMid;
  lastRight = curRight;
}


// ── Display ───────────────────────────────────────────────────
bool displayAwake = false;   // display.init() has run since this boot

void initDisplay() {
  if (displayAwake) return;
  display.init(115200, true, 50, false);
  display.setRotation(3);
  displayAwake = true;
}

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

// Redraws the current screen — partial (content only) if fastUpdate is set,
// full otherwise. Keeps polling the buttons while the panel is busy.
void refreshDisplay() {
  isRefreshing = true;

  if (fastUpdate) display.setPartialWindow(0, CONTENT_Y + 2, SCREEN_W, SCREEN_H - CONTENT_Y);
  else            display.setFullWindow();

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    if (!fastUpdate) drawStatusBar(batteryPct);
    drawCurrentScreen();
    checkButtons();
  } while (display.nextPage());

  fastUpdate   = false;
  isRefreshing = false;

  if (pendingLeft || pendingMid || pendingRight) {
    const bool l = pendingLeft, m = pendingMid, r = pendingRight;
    pendingLeft = pendingMid = pendingRight = false;
    dispatchInput(l, m, r);
  }
}

// Status bar only (battery %, WiFi/MQTT icons). Content only ever changes
// through needRefresh, so there's no reason to redraw it on a timer.
void refreshStatusBarOnly() {
  display.setPartialWindow(0, 0, SCREEN_W, DIVIDER_TOP + 1);
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    drawStatusBar(batteryPct);
    checkButtons();
  } while (display.nextPage());
}


// ── Retained inbox ────────────────────────────────────────────
// Messages are published retained, so the broker keeps the last one for
// us while we sleep and hands it over again on every connect until we
// clear it. These flags tell that re-delivery apart from a new message.
bool pendingInboxClear = false;                // our retained copy still needs wiping
RTC_DATA_ATTR bool inboxRetainCleared = true;  // broker holds no copy of lastReceivedMessage

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

void onInboxMessage(const byte* payload, unsigned int length) {
  // Empty payload = our own "clear retained" echoing back, not a message.
  if (length == 0) { inboxRetainCleared = true; return; }

  char incoming[MAX_MSG_LEN + 1];
  const uint8_t msgLen = (length < MAX_MSG_LEN) ? (uint8_t)length : MAX_MSG_LEN;
  memcpy(incoming, payload, msgLen);
  incoming[msgLen] = '\0';

  // The retained copy of a message we already have — not a new one.
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
}

void onMessageReceived(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  const char* subTopic = topic + strlen(BEEPER_ID);

  if (strcmp(subTopic, "/inbox") == 0) {
    onInboxMessage(payload, length);
  } else if (strcmp(subTopic, "/games") == 0) {
    Serial.println("Got msg in games");
    markActivity();
    gameReqHandler((char*)payload);
  } else if (strncmp(subTopic, "/games/", 7) == 0) {
    markActivity();   // the opponent moving keeps us awake for the game
    if (strcmp(subTopic + 7, "TTT") == 0) {
      Serial.println("Got msg in TTT");
      checkTicTacToeMessages((char*)payload);
    } else if (strcmp(subTopic + 7, "C4") == 0) {
      Serial.println("Got msg in C4");
      checkConnect4Messages((char*)payload);
    }
  }
}
