#pragma once

// ═══════════════════════════════════════════════════════════════
//  WifiState.h
//  Placeholder. Add WiFi functionality here later!
//  LEFT button → Back to Home
// ═══════════════════════════════════════════════════════════════

void drawWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    drawCenteredText(SCREEN_W / 2, CONTENT_Y + 52, "WiFi Connected!", &FreeMonoBold12pt7b);
    drawText(2, CONTENT_Y + 91,  "name: ", &FreeMonoBold12pt7b);
    drawCenteredText(SCREEN_W / 2, CONTENT_Y + 91,  ssid, &FreeMonoBold12pt7b);
  } else {
    drawCenteredText(SCREEN_W / 2, CONTENT_Y + 52, "WiFi", &FreeMonoBold24pt7b);
    drawCenteredText(SCREEN_W / 2, CONTENT_Y + 91, "Connecting...", &FreeMonoBold12pt7b);
  }

  //drawButtonHints("", "", "");
}


// void handleWifiInput(bool leftPressed, bool midPressed, bool rightPressed) {
//   if (leftPressed) {
//     currentState = STATE_HOME;
//     needRefresh  = true;
//   }
// }
