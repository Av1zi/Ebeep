#pragma once

// ═══════════════════════════════════════════════════════════════
//  InboxState.h
//  Shows the last received message.
//    LEFT  → Home
//    RIGHT → Reply (opens Compose)
// ═══════════════════════════════════════════════════════════════

void drawInbox() {
  drawCenteredText(SCREEN_W / 2, CONTENT_Y + 16, "Last message:", &FreeMonoBold9pt7b);
  display.drawFastHLine(20, CONTENT_Y + 20, SCREEN_W - 40, GxEPD_BLACK);

  const char* body = (lastReceivedMessage[0] != '\0')
                     ? lastReceivedMessage
                     : "No messages yet.";
  drawCenteredText(SCREEN_W / 2, CONTENT_Y + 52, body, &FreeMonoBold9pt7b);

  drawButtonHints("Back", "", "Reply");
}


void handleInboxInput(bool leftPressed, bool midPressed, bool rightPressed) {
  if (leftPressed) {
    currentState = STATE_HOME;
    needRefresh  = true;
  } else if (rightPressed) {
    typedMessage[0]  = '\0';
    messageLen       = 0;
    currentLetterIdx = 0;
    currentState     = STATE_COMPOSE;
    needRefresh      = true;
  }
}
