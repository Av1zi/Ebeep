#pragma once

void drawInbox() {
  drawText(10, 48, "Last Message:", &FreeMonoBold9pt7b);
  drawText(10, 78, lastReceivedMessage, &FreeMonoBold12pt7b);

  drawText(12, 120, "[Back]", &FreeMonoBold9pt7b);
  drawText(112, 120, "", &FreeMonoBold9pt7b);
  drawText(212, 120, "[Reply]", &FreeMonoBold9pt7b);
}

void handleInboxInput(bool leftPressed, bool selectPressed, bool rightPressed) {
  if (leftPressed) {
    currentState = STATE_HOME;
    needRefresh = true;
  } else if (rightPressed) {
    currentState = STATE_COMPOSE;
    typedMessage[0] = '\0';
    messageLen = 0;
    currentLetterIdx = 0;
    needRefresh = true;
  }
}