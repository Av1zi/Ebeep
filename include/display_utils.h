#pragma once

// ═══════════════════════════════════════════════════════════════
//  display_utils.h
//  Shared drawing primitives used by all state screens.
//  Requires: display object, fonts, and config.h constants.
// ═══════════════════════════════════════════════════════════════


// ── drawText ──────────────────────────────────────────────────
//  Place text at (x, y). y is the font BASELINE, not the top edge.
void drawText(int16_t x, int16_t y, const char* text, const GFXfont* font) {
  display.setFont(font);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(x, y);
  display.print(text);
}


// ── drawCenteredText ──────────────────────────────────────────
//  Horizontally centers text around cx at baseline y.
void drawCenteredText(int16_t cx, int16_t y, const char* text, const GFXfont* font) {
  display.setFont(font);
  display.setTextColor(GxEPD_BLACK);
  int16_t tbx, tby;
  uint16_t tbw, tbh;
  display.getTextBounds(text, 0, 0, &tbx, &tby, &tbw, &tbh);
  display.setCursor(cx - tbw / 2 - tbx, y);
  display.print(text);
}


// ── drawStatusBar ─────────────────────────────────────────────
//  Title on the left, battery icon + percentage on the right.
//  Only called on full-screen refreshes (not partial updates).
void drawStatusBar(int battPct) {
  battPct = constrain(battPct, 0, 100);

  // Title
  drawText(8, STATUS_TEXT_Y, "Ebeep", &FreeMonoBold9pt7b);

  // Battery icon — body: 36x13 rect, nub: 4x5 on the right
  const int16_t bx = 248, by = 5, bw = 36, bh = 13;
  display.drawRect(bx, by, bw, bh, GxEPD_BLACK);
  display.fillRect(bx + bw, by + 4, 4, 5, GxEPD_BLACK);  // terminal nub

  // Fill level
  int fillW = (battPct * (bw - 4)) / 100;
  if (fillW > 0) {
    display.fillRect(bx + 2, by + 2, fillW, bh - 4, GxEPD_BLACK);
  }

  // Percentage label, right-aligned up to the battery icon
  char buf[6];
  snprintf(buf, sizeof(buf), "%d%%", battPct);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.getTextBounds(buf, 0, 0, &tbx, &tby, &tbw, &tbh);
  display.setCursor(bx - 5 - tbw - tbx, STATUS_TEXT_Y);
  display.print(buf);

  // Top separator line
  display.drawFastHLine(0, DIVIDER_TOP, SCREEN_W, GxEPD_BLACK);
}


// ── drawButtonHints ───────────────────────────────────────────
//  Bottom bar: draws the divider, vertical column separators,
//  and centered labels above each of the three buttons.
//  Pass "" to leave a slot blank.
void drawButtonHints(const char* left, const char* mid, const char* right) {
  // Bottom separator line
  display.drawFastHLine(0, DIVIDER_BOT, SCREEN_W, GxEPD_BLACK);

  // Vertical column dividers
  display.drawFastVLine(COL_SPLIT1, DIVIDER_BOT, SCREEN_H - DIVIDER_BOT, GxEPD_BLACK);
  display.drawFastVLine(COL_SPLIT2, DIVIDER_BOT, SCREEN_H - DIVIDER_BOT, GxEPD_BLACK);

  // Labels
  drawCenteredText(COL1_CX, BTN_TEXT_Y, left,  &FreeMonoBold9pt7b);
  drawCenteredText(COL2_CX, BTN_TEXT_Y, mid,   &FreeMonoBold9pt7b);
  drawCenteredText(COL3_CX, BTN_TEXT_Y, right, &FreeMonoBold9pt7b);
}
