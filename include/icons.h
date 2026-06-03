#pragma once

// ═══════════════════════════════════════════════════════════════
//  icons.h  —  Icon drawing using display primitives.
//  All icons centered at (cx, cy), tuned for TILE_W=78, TILE_H=60.
// ═══════════════════════════════════════════════════════════════


// ── Envelope (Inbox) ──────────────────────────────────────────
//  Rectangle with a clean V-fold from top corners to center.
//  Both lines meet at exactly one point — no gaps, no mismatch.
void drawIconEnvelope(int16_t cx, int16_t cy) {
  const int16_t W = 44, H = 28;
  int16_t x = cx - W / 2;
  int16_t y = cy - H / 2;

  // Envelope body
  display.drawRect(x, y, W, H, GxEPD_BLACK);

  // V-fold: both diagonal lines meet cleanly at (cx, cy) — the center
  display.drawLine(x,         y, cx, cy, GxEPD_BLACK);
  display.drawLine(x + W - 1, y, cx, cy, GxEPD_BLACK);
}


// ── Pencil on Paper (Compose) ─────────────────────────────────
void drawIconCompose(int16_t cx, int16_t cy) {
  // Paper — offset slightly left to leave room for pencil
  const int16_t PW = 28, PH = 36;
  int16_t px = cx - PW / 2 - 5;
  int16_t py = cy - PH / 2;

  display.drawRect(px, py, PW, PH, GxEPD_BLACK);

  // Ruled lines
  display.drawFastHLine(px + 4, py +  8, PW - 8, GxEPD_BLACK);
  display.drawFastHLine(px + 4, py + 14, PW - 8, GxEPD_BLACK);
  display.drawFastHLine(px + 4, py + 20, PW - 8, GxEPD_BLACK);
  display.drawFastHLine(px + 4, py + 26, PW - 12, GxEPD_BLACK);

  // Pencil — shaft as a parallelogram, tip as a small triangle
  int16_t ex = cx + 16, ey = cy - 14;  // eraser end (top-right)
  int16_t tx = cx +  2, ty = cy +  8;  // tip start  (bottom-left)

  display.drawLine(ex,     ey,     tx,     ty,     GxEPD_BLACK);  // left shaft edge
  display.drawLine(ex + 5, ey - 4, tx + 5, ty - 4, GxEPD_BLACK);  // right shaft edge
  display.drawLine(ex,     ey,     ex + 5, ey - 4, GxEPD_BLACK);  // eraser cap

  // Ferrule band (visible seam near eraser)
  display.drawLine(ex - 3, ey + 4, ex + 2, ey, GxEPD_BLACK);

  // Tip triangle
  display.drawLine(tx,     ty,     tx + 3, ty + 6, GxEPD_BLACK);
  display.drawLine(tx + 5, ty - 4, tx + 3, ty + 6, GxEPD_BLACK);
}


// ── Game Controller (Games) ───────────────────────────────────
void drawIconGamepad(int16_t cx, int16_t cy) {
  const int16_t W = 48, H = 24;
  int16_t x = cx - W / 2;
  int16_t y = cy - H / 2;

  // Body
  display.drawRoundRect(x, y, W, H, 5, GxEPD_BLACK);

  // Left grip bump
  display.drawLine(x + 8,  y,     x + 4,  y - 5, GxEPD_BLACK);
  display.drawLine(x + 4,  y - 5, x + 14, y - 5, GxEPD_BLACK);
  display.drawLine(x + 14, y - 5, x + 14, y,     GxEPD_BLACK);

  // Right grip bump
  display.drawLine(x + W - 9,  y,     x + W - 14, y - 5, GxEPD_BLACK);
  display.drawLine(x + W - 14, y - 5, x + W - 4,  y - 5, GxEPD_BLACK);
  display.drawLine(x + W - 4,  y - 5, x + W - 4,  y,     GxEPD_BLACK);

  // D-pad cross (left side)
  int16_t dpx = x + 13, dpy = cy;
  display.drawFastHLine(dpx - 5, dpy,     10, GxEPD_BLACK);
  display.drawFastVLine(dpx,     dpy - 5, 10, GxEPD_BLACK);

  // Face buttons — diamond of 4 filled circles (right side)
  int16_t bx = cx + 13, by = cy;
  display.fillCircle(bx,     by - 5, 2, GxEPD_BLACK);
  display.fillCircle(bx + 5, by,     2, GxEPD_BLACK);
  display.fillCircle(bx,     by + 5, 2, GxEPD_BLACK);
  display.fillCircle(bx - 5, by,     2, GxEPD_BLACK);

  // Center button
  display.fillCircle(cx, cy, 2, GxEPD_BLACK);
}
