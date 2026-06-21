// ═══════════════════════════════════════════════════════════════
//  Callbacks.h
//  Must be included AFTER all state files.
//  Order: checkButtons → refreshDisplay → MQTT callbacks
// ═══════════════════════════════════════════════════════════════

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

    switch (currentState) {
      case STATE_HOME:      drawHome();      break;
      case STATE_INBOX:     drawInbox();     break;
      case STATE_COMPOSE:   drawCompose();   break;
      case STATE_SENT:      drawSent();      break;
      case STATE_GAMES:     drawGames();     break;
      case STATE_WIFI:      drawWifi();      break;
      case STATE_TICTACTOE: drawTicTacToe(); break;
    }

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


// ── MQTT Callbacks ────────────────────────────────────────────
// Handles the broad "<id>/games" channel — invite-level presence messages
// (TTT_START / TTT_START_ACK / TTT_LEFT). Both devices are subscribed to
// this from boot, regardless of whether they're currently in a game, so
// it's the only channel guaranteed to reach someone who hasn't joined yet.
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
  }
  if (currentState == STATE_HOME || currentState == STATE_GAMES || currentState == STATE_TICTACTOE) {
    needRefresh = true;
    fastUpdate  = true;
  }
}

void onMessageReceived(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
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