#pragma once

// ═══════════════════════════════════════════════════════════════
//  ComposeState.h
//
//  Layout (inside content area y=23..107):
//
//    y≈51  ┌─ typed text (well below the status bar) ─────────┐
//    y≈55  └─ underline ─────────────────────────────────────-─┘
//
//    y≈90       <       [ H ]       >          ← selector
//
//  Button bar:  [  <  ]   [  OK  ]   [  >  ]
//
//  DEL on empty message → leave-confirm popup overlay.
// ═══════════════════════════════════════════════════════════════

// Frequency-ordered alphabet (space first = most common).
// DEL is a virtual entry past the end — not a real character.
const char ALPHABET[]    = " ABCDEFGHIJKLMNOPQRSTUVWXYZ.!?<3";
const int  ALPHABET_SIZE = sizeof(ALPHABET) - 1;
const int  DEL_IDX       = ALPHABET_SIZE;        // virtual DEL
const int  TOTAL_ENTRIES = ALPHABET_SIZE + 1;


// ─────────────────────────────────────────────────────────────
void drawCompose() {

  // ── Leave-confirm popup (overlay) ─────────────────────────
  if (confirmLeaveCompose) {
    const int16_t pw = 210, ph = 48;
    const int16_t px = (SCREEN_W - pw) / 2;
    const int16_t py = CONTENT_Y + (CONTENT_H - ph) / 2;  // centered in content

    display.fillRect(px, py, pw, ph, GxEPD_WHITE);   // wipe background
    display.drawRect(px,     py,     pw,     ph,     GxEPD_BLACK);  // outer border
    display.drawRect(px + 2, py + 2, pw - 4, ph - 4, GxEPD_BLACK);  // inner border

    drawCenteredText(SCREEN_W / 2, py + 30, "Quit writing?", &FreeMonoBold9pt7b);

    drawButtonHints("No", "", "Yes");
    return;
  }

  // ── Typed text ────────────────────────────────────────────
  // Baseline pushed well below the status bar so partial refresh
  // never clips the top bar. Max 22 chars shown; older chars scroll off left.
  char textBuf[28];
  if (messageLen == 0) {
    strcpy(textBuf, "_");
  } else if (messageLen <= 22) {
    snprintf(textBuf, sizeof(textBuf), "%s_", typedMessage);
  } else {
    snprintf(textBuf, sizeof(textBuf), "...%s_", typedMessage + messageLen - 19);
  }
  drawCenteredText(SCREEN_W / 2, CONTENT_Y + 28, textBuf, &FreeMonoBold9pt7b);

  // Underline below text
  display.drawFastHLine(8, CONTENT_Y + 33, SCREEN_W - 16, GxEPD_BLACK);

  // ── Letter selector ───────────────────────────────────────
  const int16_t selCX = SCREEN_W / 2;
  const int16_t selCY = CONTENT_Y + 67;   // further down, away from text area

  // Selector box (44 wide × 32 tall)
  display.drawRect(selCX - 22, selCY - 16, 44, 32, GxEPD_BLACK);

  // Selected character (or label if special)
  if (currentLetterIdx == DEL_IDX) {
    drawCenteredText(selCX, selCY + 5,  "DEL", &FreeMonoBold9pt7b);
  } else if (ALPHABET[currentLetterIdx] == ' ') {
    drawCenteredText(selCX, selCY + 5,  "SPC", &FreeMonoBold9pt7b);
  } else {
    char charBuf[2] = { ALPHABET[currentLetterIdx], '\0' };
    drawCenteredText(selCX, selCY + 9,  charBuf, &FreeMonoBold12pt7b);
  }

  // Flanking arrows
  drawCenteredText(selCX - 58, selCY + 9, "<", &FreeMonoBold12pt7b);
  drawCenteredText(selCX + 58, selCY + 9, ">", &FreeMonoBold12pt7b);

  // ── Button hints ──────────────────────────────────────────
  drawButtonHints("<", "OK", ">");
}


// ─────────────────────────────────────────────────────────────
void handleComposeInput(bool leftPressed, bool midPressed, bool rightPressed) {

  // ── Handle popup first ────────────────────────────────────
  if (confirmLeaveCompose) {
    if (leftPressed) {
      // "No" — go back to composing
      confirmLeaveCompose = false;
      needRefresh = true;
      fastUpdate  = true;
    } else if (rightPressed) {
      // "Yes" — discard and go home
      confirmLeaveCompose = false;
      currentState = STATE_HOME;
      needRefresh  = true;
      fastUpdate   = false;
    }
    return;
  }

  // ── Normal compose input ──────────────────────────────────
  if (leftPressed) {
    currentLetterIdx--;
    if (currentLetterIdx < 0) currentLetterIdx = DEL_IDX;
    needRefresh = true;
    fastUpdate  = true;

  } else if (rightPressed) {
    currentLetterIdx++;
    if (currentLetterIdx > DEL_IDX) currentLetterIdx = 0;
    needRefresh = true;
    fastUpdate  = true;

  } else if (midPressed) {

    if (currentLetterIdx == DEL_IDX) {
      if (messageLen > 0) {
        // Normal backspace
        typedMessage[--messageLen] = '\0';
        needRefresh = true;
        fastUpdate  = true;
      } else {
        // Nothing to delete — ask if they want to leave
        confirmLeaveCompose = true;
        needRefresh = true;
        fastUpdate  = false;   // full refresh so status bar redraws cleanly
      }

    } else {
      char c = ALPHABET[currentLetterIdx];

      if (c == ' ' && messageLen > 0 && typedMessage[messageLen - 1] == ' ') {
        // Double-space → send
        typedMessage[--messageLen] = '\0';   // strip trailing space

        // TODO (MQTT): publish typedMessage to partner's topic here.
        //   mqttClient.publish(TOPIC_SEND, typedMessage);

        currentState  = STATE_SENT;
        sentEnteredAt = millis();
        needRefresh   = true;
        fastUpdate    = false;

      } else if (messageLen < MAX_MSG_LEN) {
        typedMessage[messageLen++] = c;
        typedMessage[messageLen]   = '\0';
        needRefresh = true;
        fastUpdate  = true;
      }
    }
  }
}
