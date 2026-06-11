#pragma once

// ═══════════════════════════════════════════════════════════════
//  wifiState.h
//  Splash shown while WiFi/MQTT are connecting at boot.
//  Not interactive — WiFiManager handles everything in setup().
// ═══════════════════════════════════════════════════════════════

void drawWifi() {
  drawCenteredText(SCREEN_W / 2, CONTENT_Y + 52, "WiFi",         &FreeMonoBold24pt7b);
  drawCenteredText(SCREEN_W / 2, CONTENT_Y + 91, "Connecting...", &FreeMonoBold12pt7b);
}
