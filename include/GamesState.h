#pragma once

// ═══════════════════════════════════════════════════════════════
//  GamesState.h  —  Carousel game selector
//
//  Center cart : 90×72, double-border selection ring
//  Side carts  : 90×60, checkerboard-dimmed, no title
//                x=-30 (left) / x=236 (right) — GxEPD2 clips naturally
//
//  Bitmap art:
//    GAME_ICONS[] entries must be exactly GCART_CW × GCART_CH (90×72)
//    and will be automatically centered/cropped vertically for side carts.
//    Set entry to nullptr to show the placeholder X instead.
//
//  To add a game : append title to GAME_TITLES[] and icon to GAME_ICONS[].
// ═══════════════════════════════════════════════════════════════

static const char* GAME_TITLES[] = {
  "TicTacToe",
  "Blackjack",
  "Connect_4",
};
static const int GAMES_COUNT = sizeof(GAME_TITLES) / sizeof(GAME_TITLES[0]);

static int gamesSelectedIdx = 0;

// ── Geometry ──────────────────────────────────────────────────
#define GCART_CW   90
#define GCART_CH   72
#define GCART_CX   ((SCREEN_W - GCART_CW) / 2)
#define GCART_CY   (CONTENT_Y + (CONTENT_H - GCART_CH) / 2)

#define GCART_SW   90
#define GCART_SH   60
#define GCART_LX   (-30)
#define GCART_RX   (SCREEN_W - 60)
#define GCART_SY   (CONTENT_Y + (CONTENT_H - GCART_SH) / 2)

#define GDOT_R    3
#define GDOT_GAP  12
#define GDOT_Y    (DIVIDER_BOT - 7)


// ─────────────────────────────────────────────────────────────
//  drawCartridgeArt
//  Bitmap fills the entire cart (x,y,w,h) — no inner padding.
//  Side carts center the 90x72 bitmap vertically, bleeding 6px
//  above and below to let GxEPD2 handle the clipping naturally.
// ─────────────────────────────────────────────────────────────
static void drawCartridgeArt(int16_t x, int16_t y, int16_t w, int16_t h,
                              int gameIdx) {
  if (gameIdx >= 0 && gameIdx < GAMES_COUNT && GAME_ICONS[gameIdx] != nullptr) {
    // Dynamically centers the 72px high bitmap. 
    // Center cart: (72 - 72) / 2 = 0px offset
    // Side cart:   (60 - 72) / 2 = -6px offset (shifts up)
    int16_t yOffset = (h - GCART_CH) / 2;
    
    display.drawBitmap(x, y + yOffset, GAME_ICONS[gameIdx], GCART_CW, GCART_CH, GxEPD_BLACK);
  } else {
    // Placeholder: X lines inside the cart
    display.drawLine(x,     y,     x+w-1, y+h-1, GxEPD_BLACK);
    display.drawLine(x+w-1, y,     x,     y+h-1, GxEPD_BLACK);
  }
}


// ─────────────────────────────────────────────────────────────
//  drawCartridge
//  title=nullptr → dotted empty-slot frame.
//  showTitle=false for side carts (avoids text bleed off-screen).
// ─────────────────────────────────────────────────────────────
static void drawCartridge(int16_t x, int16_t y, int16_t w, int16_t h,
                           const char* title, bool focused,
                           bool showTitle, int gameIdx) {

  if (title == nullptr) {
    int16_t x0 = max((int16_t)0,        x);
    int16_t x1 = min((int16_t)SCREEN_W, (int16_t)(x + w));
    for (int16_t dx = x0; dx < x1;  dx += 6) {
      display.drawPixel(dx, y,     GxEPD_BLACK);
      display.drawPixel(dx, y+h-1, GxEPD_BLACK);
    }
    for (int16_t dy = y; dy < y+h; dy += 6) {
      if (x     >= 0)        display.drawPixel(x,     dy, GxEPD_BLACK);
      if (x+w-1 < SCREEN_W)  display.drawPixel(x+w-1, dy, GxEPD_BLACK);
    }
    return;
  }

  // Bitmap fills the full cart rect — border drawn on top
  drawCartridgeArt(x, y, w, h, gameIdx);

  // Border on top of bitmap so edges are always clean
  display.drawRect(x, y, w, h, GxEPD_BLACK);
  if (focused) display.drawRect(x+4, y+4, w-8, h-8, GxEPD_BLACK);

  // Title — center cart only (side carts skip to avoid bleed)
  if (showTitle && GAME_ICONS[gameIdx] == nullptr)
    drawCenteredText(x + w/2, y + h - 3, title, &FreeMonoBold9pt7b);
}


// ─────────────────────────────────────────────────────────────
//  drawGames
// ─────────────────────────────────────────────────────────────
void drawGames() {
  int li = gamesSelectedIdx - 1;
  int ri = gamesSelectedIdx + 1;

  const char* leftTitle   = (li >= 0)          ? GAME_TITLES[li]  : nullptr;
  const char* centerTitle = GAME_TITLES[gamesSelectedIdx];
  const char* rightTitle  = (ri < GAMES_COUNT) ? GAME_TITLES[ri]  : nullptr;

  // Checkerboard dim on side carts — drawn first, border goes on top
  auto fillDim = [](int16_t x, int16_t y, int16_t w, int16_t h) {
    int16_t x0 = max((int16_t)0,        x);
    int16_t x1 = min((int16_t)SCREEN_W, (int16_t)(x + w));
    for (int16_t dy = 0; dy < h; dy += 2)
      for (int16_t dx = x0 + (dy%4==0 ? 0 : 1); dx < x1; dx += 2)
        display.drawPixel(dx, y+dy, GxEPD_BLACK);
  };

  if (leftTitle)  fillDim(GCART_LX, GCART_SY, GCART_SW, GCART_SH);
  if (rightTitle) fillDim(GCART_RX, GCART_SY, GCART_SW, GCART_SH);

  drawCartridge(GCART_LX, GCART_SY, GCART_SW, GCART_SH, leftTitle,   false, false, li);
  drawCartridge(GCART_RX, GCART_SY, GCART_SW, GCART_SH, rightTitle,  false, false, ri);
  drawCartridge(GCART_CX, GCART_CY, GCART_CW, GCART_CH, centerTitle, true,  true,  gamesSelectedIdx);

  // Position dots
  int16_t dotStartX = SCREEN_W/2 - ((GAMES_COUNT-1) * GDOT_GAP) / 2;
  for (int i = 0; i < GAMES_COUNT; i++) {
    int16_t dx = dotStartX + i * GDOT_GAP;
    if (i == gamesSelectedIdx) display.fillCircle(dx, GDOT_Y, GDOT_R, GxEPD_BLACK);
    else                       display.drawCircle(dx, GDOT_Y, GDOT_R, GxEPD_BLACK);
  }

  drawButtonHints("<", "Play", ">");
}


// ─────────────────────────────────────────────────────────────
//  handleGameSelect — uncomment when game states exist
// ─────────────────────────────────────────────────────────────
static void handleGameSelect(int idx) {
  switch (idx) {
    case 0: /* currentState = STATE_TICTACTOE;   needRefresh = true; */ break;
    case 1: /* currentState = STATE_BLACKJACK;   needRefresh = true; */ break;
    case 2: /* currentState = STATE_CONNECT4;    needRefresh = true; */ break;
  }
}


// ─────────────────────────────────────────────────────────────
//  handleGamesInput
// ─────────────────────────────────────────────────────────────
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
    }
  } else if (midPressed) {
    handleGameSelect(gamesSelectedIdx);
  }
}