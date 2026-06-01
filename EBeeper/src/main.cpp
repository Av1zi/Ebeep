#include <SPI.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold12pt7b.h>

#define EPD_CS    10
#define EPD_DC    9
#define EPD_RST   8
#define EPD_BUSY  7

GxEPD2_BW<GxEPD2_290_BS, GxEPD2_290_BS::HEIGHT> display(
  GxEPD2_290_BS(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);

void setup() {
  Serial.begin(9600);
  while (!Serial) { delay(10); }
  Serial.println("Starting...");

  SPI.begin();
  display.init(9600, true, 10, false);
  display.setRotation(1);

  // Flush with black first to sharpen pixels
  display.setFullWindow();
  display.firstPage();
  delay(2000);

  // Now draw the actual content
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setFont(&FreeMonoBold12pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(50, 30);
    display.print("Hello! It works!");
  } while (display.nextPage());

  display.hibernate();
  Serial.println("Done.");
}

void loop() {}