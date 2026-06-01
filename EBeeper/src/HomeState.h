#pragma once

void drawHome() {
  drawText(15, 65, "Inbox", &FreeMonoBold9pt7b);
  drawText(115, 65, "Write", &FreeMonoBold9pt7b);
  drawText(215, 65, "Games", &FreeMonoBold9pt7b);

  if (hasUnreadMessage) {
    display.fillCircle(75, 58, 5, GxEPD_BLACK);
  }

  drawText(12, 120, "[Inbox]", &FreeMonoBold9pt7b);
  drawText(112, 120, "[Write]", &FreeMonoBold9pt7b);
  drawText(212, 120, "[Games]", &FreeMonoBold9pt7b);
}

void handleHomeInput(bool leftPressed, bool selectPressed, bool rightPressed) {
  if (leftPressed) {
    currentState = STATE_INBOX;
    hasUnreadMessage = false; 
    needRefresh = true;
  } else if (selectPressed) {
    currentState = STATE_COMPOSE;
    typedMessage[0] = '\0';   
    messageLen = 0;
    currentLetterIdx = 0;
    needRefresh = true;
  } else if (rightPressed) {
    currentState = STATE_GAMES;
    needRefresh = true;
  }
}