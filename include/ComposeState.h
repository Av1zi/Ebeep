#pragma once

// ═══════════════════════════════════════════════════════════════
//  ComposeState.h
//
//  Layout (content area y=23..107):
//    y≈51  typed text + cursor underline
//    y≈35  progress bar (fills as you type, inverts at limit)
//    y≈90  < [ selected char ] >  selector
//
//  Double-space to send. DEL on empty → leave-confirm popup.
//  At MAX_MSG_LEN: only DEL and double-space (send) work.
// ═══════════════════════════════════════════════════════════════

// Frequency-ordered: space first for fastest access.
// DEL is a virtual slot past the end — not a stored character.
static const char ALPHABET[] PROGMEM = " ABCDEFGHIJKLMNOPQRSTUVWXYZ.!?<3";
static const uint8_t ALPHABET_SIZE   = sizeof(ALPHABET) - 1;  // excludes '\0'
static const uint8_t DEL_IDX         = ALPHABET_SIZE;
static const uint8_t TOTAL_ENTRIES   = ALPHABET_SIZE + 1;

// ─────────────────────────────────────────────────────────────
void drawCompose() {

  // ── Leave-confirm popup ───────────────────────────────────
  if (confirmLeaveCompose) {
    constexpr int16_t pw = 210, ph = 48;
    constexpr int16_t px = (SCREEN_W - pw) / 2;
    const     int16_t py = CONTENT_Y + (CONTENT_H - ph) / 2;

    display.fillRect(px,     py,     pw,     ph,     GxEPD_WHITE);
    display.drawRect(px,     py,     pw,     ph,     GxEPD_BLACK);
    display.drawRect(px + 2, py + 2, pw - 4, ph - 4, GxEPD_BLACK);
    drawCenteredText(SCREEN_W / 2, py + 30, "Quit writing?", &FreeMonoBold9pt7b);
    drawButtonHints("No", "", "Yes");
    return;
  }

  // ── Typed text with scrolling cursor ─────────────────────
  // Max 22 chars shown; older characters scroll off the left.
  char textBuf[28];
  if (messageLen == 0) {
    textBuf[0] = '_'; textBuf[1] = '\0';
  } else if (messageLen <= 22) {
    snprintf(textBuf, sizeof(textBuf), "%s_", typedMessage);
  } else {
    snprintf(textBuf, sizeof(textBuf), "...%.19s_", typedMessage + messageLen - 19);
  }
  drawCenteredText(SCREEN_W / 2, CONTENT_Y + 28, textBuf, &FreeMonoBold9pt7b);

  // ── Progress bar ──────────────────────────────────────────
  constexpr int16_t barX = 8;
  constexpr int16_t barY = CONTENT_Y + 35;
  constexpr int16_t barW = SCREEN_W - 16;
  constexpr int16_t barH = 5;

  const bool atLimit = (messageLen >= MAX_MSG_LEN);
  const int16_t fillW = atLimit ? barW : (int16_t)((messageLen * barW) / MAX_MSG_LEN);

  if (atLimit) {
    // Fully filled + invert to make it obvious
    display.fillRect(barX, barY, barW, barH, GxEPD_BLACK);
  } else {
    display.drawRect(barX, barY, barW, barH, GxEPD_BLACK);
    if (fillW > 0) display.fillRect(barX, barY, fillW, barH, GxEPD_BLACK);
  }

  // ── Letter selector ───────────────────────────────────────
  constexpr int16_t selCX = SCREEN_W / 2;
  constexpr int16_t selCY = CONTENT_Y + 67;

  display.drawRect(selCX - 22, selCY - 16, 44, 32, GxEPD_BLACK);

  const char ch = (char)pgm_read_byte(&ALPHABET[currentLetterIdx]);
  if (currentLetterIdx == DEL_IDX) {
    drawCenteredText(selCX, selCY + 5, "DEL", &FreeMonoBold9pt7b);
  } else if (ch == ' ') {
    drawCenteredText(selCX, selCY + 5, "SPC", &FreeMonoBold9pt7b);
  } else {
    char charBuf[2] = { ch, '\0' };
    drawCenteredText(selCX, selCY + 9, charBuf, &FreeMonoBold12pt7b);
  }

  drawCenteredText(selCX - 58, selCY + 9, "<", &FreeMonoBold12pt7b);
  drawCenteredText(selCX + 58, selCY + 9, ">", &FreeMonoBold12pt7b);

  drawButtonHints("<", "OK", ">");
}


// ─────────────────────────────────────────────────────────────
void handleComposeInput(bool leftPressed, bool midPressed, bool rightPressed) {

  // ── Leave-confirm popup ───────────────────────────────────
  if (confirmLeaveCompose) {
    if (leftPressed) {
      confirmLeaveCompose = false;
      needRefresh = true;
      fastUpdate  = true;
    } else if (rightPressed) {
      confirmLeaveCompose = false;
      currentState = STATE_HOME;
      needRefresh  = true;
      fastUpdate   = false;
    }
    return;
  }

  // ── Scroll letter selector ────────────────────────────────
  if (leftPressed) {
    currentLetterIdx = (currentLetterIdx == 0) ? DEL_IDX : currentLetterIdx - 1;
    needRefresh = true;
    fastUpdate  = true;
  } else if (rightPressed) {
    currentLetterIdx = (currentLetterIdx >= DEL_IDX) ? 0 : currentLetterIdx + 1;
    needRefresh = true;
    fastUpdate  = true;

  // ── Confirm / backspace ───────────────────────────────────
  } else if (midPressed) {
    if (currentLetterIdx == DEL_IDX) {
      if (messageLen > 0) {
        typedMessage[--messageLen] = '\0';
        needRefresh = true;
        fastUpdate  = true;
      } else {
        confirmLeaveCompose = true;
        needRefresh = true;
        fastUpdate  = false;
      }
    } else {
      const char ch = (char)pgm_read_byte(&ALPHABET[currentLetterIdx]);

      // Double-space → send (allowed even at limit — last char must already be space,
      // but at limit you can't add one, so handle: if at limit and last char is space,
      // a second space press sends. We check BEFORE the length guard.)
      if (ch == ' ' && messageLen > 0 && typedMessage[messageLen - 1] == ' ') {
        typedMessage[--messageLen] = '\0';   // strip trailing space
        mqttClient.publish((String(RECIVER_ID) + "/inbox").c_str(), typedMessage);
        currentState  = STATE_SENT;
        sentEnteredAt = millis();
        needRefresh   = true;
        fastUpdate    = false;
      } else if (messageLen >= MAX_MSG_LEN) {
        // At limit — only DEL and double-space (above) work; ignore everything else.
        return;
      } else {
        typedMessage[messageLen++] = ch;
        typedMessage[messageLen]   = '\0';
        needRefresh = true;
        fastUpdate  = true;
      }
    }
  }
}
