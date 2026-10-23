#pragma once

// ═══════════════════════════════════════════════════════════════
//  wifiState.h
//  Connection-related screens. None are interactive except that any
//  button press closes the setup hotspot (handled in runWifiPortal()).
//    drawWifi()     — connecting to a known network
//    drawPortal()   — no known network in range, setup hotspot is open
//    drawSleeping() — shown while the device is in deep sleep
// ═══════════════════════════════════════════════════════════════

void drawWifi() {
  drawCenteredText(SCREEN_W / 2, CONTENT_Y + 52, "WiFi",         &FreeMonoBold24pt7b);
  drawCenteredText(SCREEN_W / 2, CONTENT_Y + 91, "Connecting...", &FreeMonoBold12pt7b);
}

void drawPortal() {
  drawCenteredText(SCREEN_W / 2, CONTENT_Y + 16, "No known WiFi here", &FreeMonoBold9pt7b);
  drawCenteredText(SCREEN_W / 2, CONTENT_Y + 34, "Connect your phone to:", &FreeMonoBold9pt7b);
  drawCenteredText(SCREEN_W / 2, CONTENT_Y + 58, AP_NAME, &FreeMonoBold12pt7b);
  if (sizeof(HOTSPOT_PASSWORD) > 1) {
    char buf[40];
    snprintf(buf, sizeof(buf), "password: %s", HOTSPOT_PASSWORD);
    drawCenteredText(SCREEN_W / 2, CONTENT_Y + 78, buf, &FreeMonoBold9pt7b);
  }
  drawButtonHints("", "Cancel", "");
}

void drawSleeping() {
  drawCenteredText(SCREEN_W / 2, CONTENT_Y + 48, "Sleeping", &FreeMonoBold24pt7b);
  drawCenteredText(SCREEN_W / 2, CONTENT_Y + 76,
                   batteryEmpty ? "battery empty - charge me" : "press to wake up", &FreeMonoBold9pt7b);
  // Only these two are wired as deep-sleep wake pins (BTN_RIGHT isn't an LP GPIO)
  drawButtonHints("Wake", "Wake", "");
}
