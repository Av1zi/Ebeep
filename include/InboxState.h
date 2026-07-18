#pragma once

// ═══════════════════════════════════════════════════════════════
//  InboxState.h
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
  // Wipe retained copy on MQTT server if the user has read the message
  if (hasUnreadMessage) {
    static char inboxTopic[24] = "";
    if (inboxTopic[0] == '\0') snprintf(inboxTopic, sizeof(inboxTopic), "%s/inbox", BEEPER_ID);
    mqttClient.publish(inboxTopic, "", true);
    hasUnreadMessage = false;
  }
  if (leftPressed) {
    currentState = STATE_HOME;
    needRefresh  = true;
  } else if (rightPressed) {
    if (!mqttClient.connected()) { currentState = STATE_WIFI; needRefresh = true; return; }
    typedMessage[0]  = '\0';
    messageLen       = 0;
    currentLetterIdx = 0;
    currentState     = STATE_COMPOSE;
    needRefresh      = true;
  }
}
