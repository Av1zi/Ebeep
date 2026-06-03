#pragma once

// ═══════════════════════════════════════════════════════════════
//  GamesState.h
//  Placeholder. Add mini-games here later!
//  LEFT button → Back to Home
// ═══════════════════════════════════════════════════════════════

void drawGames() {
  drawCenteredText(SCREEN_W / 2, CONTENT_Y + 28, "Games",          &FreeMonoBold12pt7b);
  drawCenteredText(SCREEN_W / 2, CONTENT_Y + 56, "Coming soon :)", &FreeMonoBold9pt7b);

  drawButtonHints("Back", "", "");
}


void handleGamesInput(bool leftPressed, bool midPressed, bool rightPressed) {
  if (leftPressed) {
    currentState = STATE_HOME;
    needRefresh  = true;
  }
}
