#pragma once

void drawGames() {
  drawText(40, 65, "Games Coming Soon!", &FreeMonoBold9pt7b);
  drawText(12, 120, "[Back]", &FreeMonoBold9pt7b);
}

void handleGamesInput(bool leftPressed, bool selectPressed, bool rightPressed) {
  if (leftPressed) {
    currentState = STATE_HOME;
    needRefresh = true;
  }
}