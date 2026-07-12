#pragma once

// ═══════════════════════════════════════════════════════════════
//  GamesState.h — Carousel game selector
//
//  Center cart : 90×72, double-border selection ring
//  Side carts  : 90×60, checkerboard-dimmed, no title
//                x=-30 (left) / x=236 (right) — GxEPD2 clips naturally
//
//  Bitmap art:
//    GAME_ICONS[] entries must be 90×72 (GCART_CW × GCART_CH) PROGMEM.
//    nullptr → placeholder X.
//
//  To add a game: append to GAME_TITLES[] and GAME_ICONS[].
// ═══════════════════════════════════════════════════════════════

static const char* const GAME_TITLES[] = {
  "TicTacToe",
  "Blackjack",
  "Connect_4",
};
static constexpr int GAMES_COUNT = sizeof(GAME_TITLES) / sizeof(GAME_TITLES[0]);

static int8_t gamesSelectedIdx = 0;

// ── Geometry ──────────────────────────────────────────────────
// Center cartridge
constexpr int16_t GCART_CW = 90;
constexpr int16_t GCART_CH = 72;
constexpr int16_t GCART_CX = (SCREEN_W - GCART_CW) / 2;
constexpr int16_t GCART_CY = CONTENT_Y + (CONTENT_H - GCART_CH) / 2;

// Side cartridges
constexpr int16_t GCART_SW = 90;
constexpr int16_t GCART_SH = 60;
constexpr int16_t GCART_LX = -30;
constexpr int16_t GCART_RX = SCREEN_W - 60;
constexpr int16_t GCART_SY = CONTENT_Y + (CONTENT_H - GCART_SH) / 2;

// Position dots
constexpr int16_t GDOT_R   = 3;
constexpr int16_t GDOT_GAP = 12;
constexpr int16_t GDOT_Y   = DIVIDER_BOT - 7;


// ─────────────────────────────────────────────────────────────
//  Checkerboard dim fill over a (possibly partially off-screen) rect.
// ─────────────────────────────────────────────────────────────
static void fillDim(int16_t x, int16_t y, int16_t w, int16_t h) {
  const int16_t x0 = max((int16_t)0,        x);
  const int16_t x1 = min((int16_t)SCREEN_W, (int16_t)(x + w));
  for (int16_t dy = 0; dy < h; dy += 2) {
    const int16_t xStart = x0 + ((dy % 4 == 0) ? 0 : 1);
    for (int16_t dx = xStart; dx < x1; dx += 2)
      display.drawPixel(dx, y + dy, GxEPD_BLACK);
  }
}


// ─────────────────────────────────────────────────────────────
//  Draw the bitmap (or placeholder X) inside a cart rect.
//  Side carts (h < GCART_CH) centre the 72px bitmap vertically;
//  GxEPD2 clips the 6px bleed at each edge automatically.
// ─────────────────────────────────────────────────────────────
static void drawCartridgeArt(int16_t x, int16_t y, int16_t w, int16_t h,
                             int gameIdx) {
  if (gameIdx >= 0 && gameIdx < GAMES_COUNT && GAME_ICONS[gameIdx] != nullptr) {
    const int16_t yOffset = (h - GCART_CH) / 2;
    display.drawBitmap(x, y + yOffset, GAME_ICONS[gameIdx],
                       GCART_CW, GCART_CH, GxEPD_BLACK);
  } else {
    // Placeholder X
    display.drawLine(x,     y,     x+w-1, y+h-1, GxEPD_BLACK);
    display.drawLine(x+w-1, y,     x,     y+h-1, GxEPD_BLACK);
  }
}


// ─────────────────────────────────────────────────────────────
//  Draw one cartridge card.
//  title=nullptr → dotted empty-slot frame.
//  showTitle    → center-cart label (skipped on side carts).
//  focused      → draws the inner selection ring.
// ─────────────────────────────────────────────────────────────
static void drawCartridge(int16_t x, int16_t y, int16_t w, int16_t h,
                          const char* title, bool focused,
                          bool showTitle, int gameIdx) {
  if (title == nullptr) {
    // Empty slot: dotted border
    const int16_t x0 = max((int16_t)0,        x);
    const int16_t x1 = min((int16_t)SCREEN_W, (int16_t)(x + w));
    for (int16_t dx = x0; dx < x1; dx += 6) {
      display.drawPixel(dx, y,     GxEPD_BLACK);
      display.drawPixel(dx, y+h-1, GxEPD_BLACK);
    }
    for (int16_t dy = y; dy < y+h; dy += 6) {
      if (x     >= 0)        display.drawPixel(x,     dy, GxEPD_BLACK);
      if (x+w-1 < SCREEN_W)  display.drawPixel(x+w-1, dy, GxEPD_BLACK);
    }
    return;
  }

  drawCartridgeArt(x, y, w, h, gameIdx);
  display.drawRect(x, y, w, h, GxEPD_BLACK);
  if (focused) display.drawRect(x+4, y+4, w-8, h-8, GxEPD_BLACK);

  // Title only on center cart and only when there's no bitmap
  if (showTitle && GAME_ICONS[gameIdx] == nullptr)
    drawCenteredText(x + w/2, y + h - 3, title, &FreeMonoBold9pt7b);
}


// ─────────────────────────────────────────────────────────────
//  drawGames
// ─────────────────────────────────────────────────────────────
void drawGames() {
  const int li = gamesSelectedIdx - 1;
  const int ri = gamesSelectedIdx + 1;

  const char* leftTitle   = (li >= 0)          ? GAME_TITLES[li]             : nullptr;
  const char* rightTitle  = (ri < GAMES_COUNT) ? GAME_TITLES[ri]             : nullptr;
  const char* centerTitle = GAME_TITLES[gamesSelectedIdx];

  // Dim side carts before drawing borders on top
  if (leftTitle)  fillDim(GCART_LX, GCART_SY, GCART_SW, GCART_SH);
  if (rightTitle) fillDim(GCART_RX, GCART_SY, GCART_SW, GCART_SH);
  drawCartridge(GCART_LX, GCART_SY, GCART_SW, GCART_SH, leftTitle,   false, false, li);
  drawCartridge(GCART_RX, GCART_SY, GCART_SW, GCART_SH, rightTitle,  false, false, ri);
  drawCartridge(GCART_CX, GCART_CY, GCART_CW, GCART_CH, centerTitle, true,  true,  gamesSelectedIdx);
  // Opponent-waiting indicator dot on TicTacToe cartridge (index 0)
  if (gamesSelectedIdx == 0 && TTT_hasOpponent) {
    display.fillCircle(GCART_CX + GCART_CW - 8, GCART_CY + 8, 6, GxEPD_BLACK);
  }

  // Position indicator dots
  const int16_t dotStartX = SCREEN_W/2 - ((GAMES_COUNT - 1) * GDOT_GAP) / 2;
  for (int i = 0; i < GAMES_COUNT; i++) {
    const int16_t dx = dotStartX + i * GDOT_GAP;
    if (i == gamesSelectedIdx) display.fillCircle(dx, GDOT_Y, GDOT_R, GxEPD_BLACK);
    else                       display.drawCircle(dx, GDOT_Y, GDOT_R, GxEPD_BLACK);
  }

  drawButtonHints("<", "Play", ">");
}


// ─────────────────────────────────────────────────────────────
//  handleGamesInput
// ─────────────────────────────────────────────────────────────
static void handleGameSelect(int8_t idx) {
  switch (idx) {
    case 0:  currentState = STATE_TICTACTOE; needRefresh = true; enterTicTacToe(); break;
    case 1: /* currentState = STATE_BLACKJACK; needRefresh = true; */ break;
    case 2:  currentState = STATE_CONNECT4;  needRefresh = true; enterConnect4();  break;
  }
}

void handleGamesInput(bool leftPressed, bool midPressed, bool rightPressed) {
  if (leftPressed) {
    if (gamesSelectedIdx > 0) {
      gamesSelectedIdx--;
      needRefresh = true;
      fastUpdate  = true;
    } else {
      currentState = STATE_HOME;
      needRefresh  = true;
      fastUpdate   = false;
    }
  } else if (rightPressed) {
    if (gamesSelectedIdx < GAMES_COUNT - 1) {
      gamesSelectedIdx++;
      needRefresh = true;
      fastUpdate  = true;
    } else if (gamesSelectedIdx == GAMES_COUNT - 1) {
      gamesSelectedIdx = 0;
      needRefresh  = true;
      fastUpdate   = true;
    }
  } else if (midPressed) {
    handleGameSelect(gamesSelectedIdx);
  }
}
