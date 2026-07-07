#pragma once

// ═══════════════════════════════════════════════════════════════
//  HomeState.h
//  Three icon tiles: Inbox (with unread dot), Compose, Games.
//    LEFT  → Inbox
//    MID   → Compose
//    RIGHT → Games
// ═══════════════════════════════════════════════════════════════

void drawHome() {
  // Inbox tile
  display.drawRect(TILE1_X, TILE_Y, TILE_W, TILE_H, GxEPD_BLACK);
  drawIconEnvelope(TILE1_X + TILE_W / 2, TILE_Y + TILE_H / 2);
  if (hasUnreadMessage) {
    display.fillCircle(TILE1_X + TILE_W - 8, TILE_Y + 8, 6, GxEPD_BLACK);
  }

  // Compose tile
  display.drawRect(TILE2_X, TILE_Y, TILE_W, TILE_H, GxEPD_BLACK);
  drawIconCompose(TILE2_X + TILE_W / 2, TILE_Y + TILE_H / 2);

  // Games tile
  display.drawRect(TILE3_X, TILE_Y, TILE_W, TILE_H, GxEPD_BLACK);
  drawIconGamepad(TILE3_X + TILE_W / 2, TILE_Y + TILE_H / 2);
  if(TTT_hasOpponent)
  {
    display.fillCircle(TILE3_X + TILE_W - 8, TILE_Y + 8, 6, GxEPD_BLACK);
  }

  drawButtonHints("Inbox", "Write", "Games");
}


void handleHomeInput(bool leftPressed, bool midPressed, bool rightPressed) {
  if (leftPressed) {
    hasUnreadMessage = false;
    currentState     = STATE_INBOX;
    needRefresh      = true;
  } else if (midPressed) {
    if (!mqttClient.connected()) { currentState = STATE_WIFI; needRefresh = true; return; }
    typedMessage[0]  = '\0';
    messageLen       = 0;
    currentLetterIdx = 0;
    currentState     = STATE_COMPOSE;
    needRefresh      = true;
  } else if (rightPressed) {
    if (!mqttClient.connected()) { currentState = STATE_WIFI; needRefresh = true; return; }
    currentState = STATE_GAMES;
    needRefresh  = true;
  }
}
