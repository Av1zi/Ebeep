#pragma once

// ═══════════════════════════════════════════════════════════════
//  display_utils.h
//  Shared drawing primitives used by all state screens.
// ═══════════════════════════════════════════════════════════════


// Place text at pixel (x, y) — y is the font baseline.
void drawText(int16_t x, int16_t y, const char* text, const GFXfont* font) {
  display.setFont(font);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(x, y);
  display.print(text);
}

// Horizontally center text around cx at baseline y.
void drawCenteredText(int16_t cx, int16_t y, const char* text, const GFXfont* font) {
  display.setFont(font);
  display.setTextColor(GxEPD_BLACK);
  int16_t tbx, tby;
  uint16_t tbw, tbh;
  display.getTextBounds(text, 0, 0, &tbx, &tby, &tbw, &tbh);
  display.setCursor(cx - tbw / 2 - tbx, y);
  display.print(text);
}

// draw the top part
void drawStatusBar(int battPct) {
  battPct = constrain(battPct, 0, 100);

  drawText(8, STATUS_TEXT_Y, "Ebeep", &FreeMonoBold9pt7b);

  // Battery body + terminal nub
  constexpr int16_t bx = 248, by = 5, bw = 36, bh = 13;
  display.drawRect(bx, by, bw, bh, GxEPD_BLACK);
  display.fillRect(bx + bw, by + 4, 4, 5, GxEPD_BLACK);

  // Fill proportional to charge
  const int fillW = (battPct * (bw - 4)) / 100;
  if (fillW > 0) display.fillRect(bx + 2, by + 2, fillW, bh - 4, GxEPD_BLACK);

  // Percentage label, right-aligned against the battery icon
  char buf[6];
  snprintf(buf, sizeof(buf), "%d%%", battPct);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  int16_t tbx, tby;
  uint16_t tbw, tbh;
  display.getTextBounds(buf, 0, 0, &tbx, &tby, &tbw, &tbh);
  display.setCursor(bx - 5 - tbw - tbx, STATUS_TEXT_Y);
  display.print(buf);

  display.drawFastHLine(0, DIVIDER_TOP, SCREEN_W, GxEPD_BLACK);
}

void drawButtonHints(const char* left, const char* mid, const char* right) {
  display.drawFastHLine(0, DIVIDER_BOT, SCREEN_W, GxEPD_BLACK);
  display.drawFastVLine(COL_SPLIT1, DIVIDER_BOT, SCREEN_H - DIVIDER_BOT, GxEPD_BLACK);
  display.drawFastVLine(COL_SPLIT2, DIVIDER_BOT, SCREEN_H - DIVIDER_BOT, GxEPD_BLACK);
  drawCenteredText(COL1_CX, BTN_TEXT_Y, left,  &FreeMonoBold9pt7b);
  drawCenteredText(COL2_CX, BTN_TEXT_Y, mid,   &FreeMonoBold9pt7b);
  drawCenteredText(COL3_CX, BTN_TEXT_Y, right, &FreeMonoBold9pt7b);
}


