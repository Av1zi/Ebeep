#pragma once

#include <secret.h>  // WiFi credentials and MQTT server details (not checked into version control)


// ═══════════════════════════════════════════════════════════════
//  EBEEP — config.h
//  All hardware pins and layout constants live here.
//  When porting to ESP32C3: only change the pin numbers below.
// ═══════════════════════════════════════════════════════════════

// ── E-Ink Display SPI Pins ────────────────────────────────────
//
//  XIAO ESP32C6 → WeAct E-Paper 2.9" display wiring:
//
//    XIAO D10 (MOSI) ──→ SDA / DIN
//    XIAO D8  (SCK)  ──→ SCL / CLK
//    XIAO D7         ──→ CS
//    XIAO D6         ──→ DC
//    XIAO D5         ──→ RST
//    XIAO D4         ──→ BUSY
//    3.3V            ──→ VCC
//    GND             ──→ GND
//
#define EPD_CS    D7
#define EPD_DC    D6
#define EPD_RST   D5
#define EPD_BUSY  D4

// ── Button Pins ───────────────────────────────────────────────
// Note: D2 (GPIO4) and D3 (GPIO5) are strapping pins. 
// Avoid holding these down while booting or hitting reset.
#define BTN_LEFT    D1
#define BTN_SELECT  D2
#define BTN_RIGHT   D3

// ── Analog Battery Pin ────────────────────────────────────────
//#define BATT_PIN    D0
// ADD THIS WHEN CONNECTING BATTERY

// ── Display Dimensions (2.9", landscape) ─────────────────────
#define SCREEN_W  296
#define SCREEN_H  128

// ── Vertical Layout ───────────────────────────────────────────
//
//   y=  0  ╔══════════════════════════════════════════════════╗
//          ║  Ebeep                        72%  [████░]       ║
//   y= 22  ╠══════════════════════════════════════════════════╣
//   y= 23  ║                                                  ║
//          ║                 CONTENT AREA          (85px)     ║
//   y=107  ║                                                  ║
//   y=108  ╠══════════════════════════════════════════════════╣
//   y=109  ║   [ Left ]          [ Mid ]          [ Right ]   ║  (19px)
//   y=127  ╚══════════════════════════════════════════════════╝

#define DIVIDER_TOP    22
#define CONTENT_Y      23
#define CONTENT_H      85    // DIVIDER_BOT - CONTENT_Y
#define DIVIDER_BOT   108
#define BTN_BAR_Y     109
#define BTN_TEXT_Y    124    // text baseline inside button bar
#define STATUS_TEXT_Y  17    // text baseline inside status bar

// ── Horizontal Column Split (3-button bar) ────────────────────
#define COL_SPLIT1   (SCREEN_W / 3)
#define COL_SPLIT2   (2 * SCREEN_W / 3)
#define COL1_CX      (COL_SPLIT1 / 2)
#define COL2_CX      (COL_SPLIT1 + (COL_SPLIT2 - COL_SPLIT1) / 2)
#define COL3_CX      (COL_SPLIT2 + (SCREEN_W - COL_SPLIT2) / 2)

// ── Home Screen Tile Layout ───────────────────────────────────
//  Tile size: 78x60.  Gap between tiles: 16px.  Side margins: 15px.
#define TILE_W    78
#define TILE_H    60
#define TILE_Y    ((CONTENT_Y) + ((CONTENT_H) - (TILE_H)) / 2)  // vertically centered = 35
#define TILE1_X   15
#define TILE2_X   109
#define TILE3_X   203

// ── App Settings ──────────────────────────────────────────────
#define MAX_MSG_LEN      30
#define SENT_DISPLAY_MS  3000UL
#define DEBOUNCE_MS      50

// ── WiFi Settings ─────────────────────────────────────────────
// credentials are stored in secret.h (not checked into version control for security)
char ssid[] = WIFI_SSID;        // your network SSID (name)
char pass[] = WIFI_PASSWORD;

// ── MQTT Settings ─────────────────────────────────────────────
// credentials are stored in secret.h (not checked into version control for security)

// #define MQTT_TOPIC_SUB  "ebeep/inbox"
// #define MQTT_TOPIC_PUB  "ebeep/outbox"

// char mqttServer[] = MQTT_SERVER;
// uint16_t mqttPort = MQTT_PORT;

// char mqttInboxTopic[] = "Beeper_1";
// char mqttOutboxTopic[] = "Beeper_2";

// char mqttUser[] = MQTT_USERNAME;
// char mqttPass[] = MQTT_PASSWORD;