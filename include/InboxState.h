#pragma once

// ═══════════════════════════════════════════════════════════════
//  InboxState.h
//  Shows the last received message.
//  LEFT button → Back to Home
//  RIGHT button → Reply (opens Compose)
// ═══════════════════════════════════════════════════════════════

void drawInbox() {
  // ── Label ─────────────────────────────────────────────────
  drawCenteredText(SCREEN_W / 2, CONTENT_Y + 16, "Last message:", &FreeMonoBold9pt7b);

  // Thin underline below label
  display.drawFastHLine(20, CONTENT_Y + 20, SCREEN_W - 40, GxEPD_BLACK);

  // ── Message body ──────────────────────────────────────────
  if (strlen(lastReceivedMessage) == 0) {
    drawCenteredText(SCREEN_W / 2, CONTENT_Y + 52, "No messages yet.", &FreeMonoBold9pt7b);
  } else {
    drawCenteredText(SCREEN_W / 2, CONTENT_Y + 52, lastReceivedMessage, &FreeMonoBold9pt7b);
  }

  // ── Button hints ──────────────────────────────────────────
  drawButtonHints("Back", "", "Reply");
}


void handleInboxInput(bool leftPressed, bool midPressed, bool rightPressed) {
  if (leftPressed) {
    currentState = STATE_HOME;
    needRefresh  = true;

  } else if (rightPressed) {
    // Jump straight into compose as a reply
    typedMessage[0]  = '\0';
    messageLen       = 0;
    currentLetterIdx = 0;
    currentState     = STATE_COMPOSE;
    needRefresh      = true;
  }
}
