#pragma once

// ═══════════════════════════════════════════════════════════════
//  SentState.h
//  Shown briefly after a message is sent.
//  Auto-returns to Home after SENT_DISPLAY_MS milliseconds.
//  Any button press also dismisses it early.
// ═══════════════════════════════════════════════════════════════

void drawSent() {
  drawCenteredText(SCREEN_W / 2, CONTENT_Y + 52, "Sent!", &FreeMonoBold24pt7b);

  // Subtle hint that it auto-dismisses
  drawCenteredText(SCREEN_W / 2, CONTENT_Y + 91, "returning home...", &FreeMonoBold12pt7b);

  // No button hints — any button dismisses via handleSentInput
  //drawButtonHints("", "", "");
}


void handleSentInput(bool leftPressed, bool midPressed, bool rightPressed) {
  // Any button press skips the timer and goes home immediately
  if (leftPressed || midPressed || rightPressed) {
    currentState = STATE_HOME;
    needRefresh  = true;
    fastUpdate   = false;
  }
}
