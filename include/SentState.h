#pragma once

// ═══════════════════════════════════════════════════════════════
//  SentState.h
//  Shown briefly after sending. Auto-returns to Home after
//  SENT_DISPLAY_MS. Any button press dismisses it early.
// ═══════════════════════════════════════════════════════════════

void drawSent() {
  drawCenteredText(SCREEN_W / 2, CONTENT_Y + 52, "Sent!",           &FreeMonoBold24pt7b);
  drawCenteredText(SCREEN_W / 2, CONTENT_Y + 91, "returning home...", &FreeMonoBold12pt7b);
}


void handleSentInput(bool leftPressed, bool midPressed, bool rightPressed) {
  if (leftPressed || midPressed || rightPressed) {
    currentState = STATE_HOME;
    needRefresh  = true;
    fastUpdate   = false;
  }
}
