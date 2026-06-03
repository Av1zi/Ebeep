#pragma once

// ═══════════════════════════════════════════════════════════════
//  HomeState.h
//  Three icon tiles: Inbox (with unread dot), Compose, Games.
//  LEFT button → Inbox
//  MID  button → Compose
//  RIGHT button → Games
// ═══════════════════════════════════════════════════════════════

void drawHome() {
  // ── Tile 1: Inbox ─────────────────────────────────────────
  display.drawRect(TILE1_X, TILE_Y, TILE_W, TILE_H, GxEPD_BLACK);
  drawIconEnvelope(TILE1_X + TILE_W / 2, TILE_Y + TILE_H / 2);

  // Unread notification dot — filled circle at top-right of tile
  if (hasUnreadMessage) {
    display.fillCircle(TILE1_X + TILE_W - 8, TILE_Y + 8, 6, GxEPD_BLACK);
  }

  // ── Tile 2: Compose ───────────────────────────────────────
  display.drawRect(TILE2_X, TILE_Y, TILE_W, TILE_H, GxEPD_BLACK);
  drawIconCompose(TILE2_X + TILE_W / 2, TILE_Y + TILE_H / 2);

  // ── Tile 3: Games ─────────────────────────────────────────
  display.drawRect(TILE3_X, TILE_Y, TILE_W, TILE_H, GxEPD_BLACK);
  drawIconGamepad(TILE3_X + TILE_W / 2, TILE_Y + TILE_H / 2);

  // ── Button hints ──────────────────────────────────────────
  drawButtonHints("Inbox", "Write", "Games");
}


void handleHomeInput(bool leftPressed, bool midPressed, bool rightPressed) {
  if (leftPressed) {
    hasUnreadMessage = false;
    currentState     = STATE_INBOX;
    needRefresh      = true;

  } else if (midPressed) {
    // Reset compose state before entering
    typedMessage[0]  = '\0';
    messageLen       = 0;
    currentLetterIdx = 0;
    currentState     = STATE_COMPOSE;
    needRefresh      = true;

  } else if (rightPressed) {
    currentState = STATE_GAMES;
    needRefresh  = true;
  }
}
