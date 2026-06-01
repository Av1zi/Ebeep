#include <Arduino.h>
#include "helper.h"

// =====================
// YOUR SCREEN HERE
// =====================
void drawMyScreen() {
  beginDraw();
  do {
    // Draw text
    drawText(10, 30, "Hello!");
    //drawText(10, 60, "It works!", &FreeMonoBold9pt7b);

    // Draw shapes
    //display.fillCircle(250, 64, 30, GxEPD_BLACK);
    //display.drawRect(5, 5, 100, 40, GxEPD_BLACK);

    // Draw heart bitmap
    //drawBitmap(0, 0, AVIZI_bitmap);

  } while (endDraw());
}

// =====================
// SETUP
// =====================
void setup() {
  Serial.begin(9600);
  while (!Serial) delay(10);

  SPI.begin();
  display.init(9600, true, 50, false);
  display.setRotation(1); // landscape
  display.clearScreen();

  drawMyScreen();

  display.hibernate();
  Serial.println("Done.");
}

void loop() {
  // Empty loop
}