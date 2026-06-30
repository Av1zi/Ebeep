#pragma once

#include <secret.h>  // HOTSPOT_PASSWORD, MQTT_SERVER, MQTT_PORT, MQTT_PASSWORD

// ═══════════════════════════════════════════════════════════════
//  EBEEP — config.h
//  Hardware pins, layout constants, and app settings.
//  All values are compile-time constants — nothing mutable here.
// ═══════════════════════════════════════════════════════════════

// ── E-Ink Display SPI Pins ────────────────────────────────────
//
//  XIAO ESP32C6 → WeAct E-Paper 2.9" display wiring:
//
//    XIAO D10 (MOSI) ──> SDA / DIN
//    XIAO D8  (SCK)  ──> SCL / CLK
//    XIAO D7         ──> CS
//    XIAO D6         ──> D/C
//    XIAO D5         ──> RES / RST
//    XIAO D4         ──> BUSY
//    3.3V            ──> VCC
//    GND             ──> GND

#define EPD_CS    17   // D7
#define EPD_DC    16   // D6
#define EPD_RST   23   // D5
#define EPD_BUSY  22   // D4

// ── Button Pins ───────────────────────────────────────────────
#define BTN_LEFT    1   // D1
#define BTN_SELECT  2   // D2
#define BTN_RIGHT  21   // D3

// ── Display Dimensions (2.9", landscape) ─────────────────────
#define SCREEN_W  296
#define SCREEN_H  128

// ── Vertical Layout ───────────────────────────────────────────
//   y=  0  ╔═══ Status bar ══════════════════════════════════╗
//   y= 22  ╠════════════════════════════════════════════════╣
//   y= 23  ║           CONTENT AREA  (85 px)                ║
//   y=107  ║                                                 ║
//   y=108  ╠════════════════════════════════════════════════╣
//   y=109  ║   [ Left ]      [ Mid ]      [ Right ]   (19px)║
//   y=127  ╚════════════════════════════════════════════════╝
#define DIVIDER_TOP    22
#define CONTENT_Y      23
#define CONTENT_H      85
#define DIVIDER_BOT   108
#define BTN_BAR_Y     109
#define BTN_TEXT_Y    124
#define STATUS_TEXT_Y  17

// ── 3-Button Bar Column Split ────────────────────────────────
#define COL_SPLIT1   (SCREEN_W / 3)
#define COL_SPLIT2   (2 * SCREEN_W / 3)
#define COL1_CX      (COL_SPLIT1 / 2)
#define COL2_CX      (COL_SPLIT1 + (COL_SPLIT2 - COL_SPLIT1) / 2)
#define COL3_CX      (COL_SPLIT2 + (SCREEN_W - COL_SPLIT2) / 2)

// ── Home Screen Tiles (78×60, 16px gap, 15px side margins) ───
#define TILE_W    78
#define TILE_H    60
#define TILE_Y    (CONTENT_Y + (CONTENT_H - TILE_H) / 2)
#define TILE1_X   15
#define TILE2_X   109
#define TILE3_X   203

// ── App Settings ──────────────────────────────────────────────
#define MAX_MSG_LEN      30
#define SENT_DISPLAY_MS  3000UL
#define DEBOUNCE_MS      50
#define HOLD_DELAY_MS    500
#define HOLD_REPEAT_MS   150

// ── WiFi / MQTT ───────────────────────────────────────────────
// Credentials come from secret.h (not checked into VCS).
// AP_pass: leave as "" in secret.h for an open access point.
#define AP_NAME   "Ebeep_1_config"

constexpr const char* MQTT_SERVER_ADDR  = MQTT_SERVER;
constexpr uint16_t    MQTT_SERVER_PORT  = MQTT_PORT;

constexpr const char* BEEPER_ID  = "Beeper_1";
constexpr const char* RECIVER_ID  = "Beeper_2";

constexpr const char* MQTT_USERNAME_STR = MQTT_USERNAME;
constexpr const char* MQTT_PASSWORD_STR = MQTT_PASSWORD;


// ═══════════════════════════════════════════════════════════════
//  DISPLAY
// ═══════════════════════════════════════════════════════════════
GxEPD2_BW<GxEPD2_290_BS, GxEPD2_290_BS::HEIGHT> display(
  GxEPD2_290_BS(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);

// ═══════════════════════════════════════════════════════════════
//  NETWORK
// ═══════════════════════════════════════════════════════════════
WiFiClientSecure wifiClient;
PubSubClient     mqttClient(wifiClient);

// ═══════════════════════════════════════════════════════════════
//  STATE MACHINE
// ═══════════════════════════════════════════════════════════════
enum ScreenState : uint8_t {
  STATE_WIFI,
  STATE_HOME,
  STATE_INBOX,
  STATE_COMPOSE,
  STATE_SENT,
  STATE_GAMES,
  STATE_TICTACTOE
};

ScreenState currentState = STATE_WIFI;

// ── Display refresh flags ─────────────────────────────────────
bool needRefresh = false;   // true  → redraw this loop tick
bool fastUpdate  = false;   // true  → partial refresh (content only)
                            // false → full refresh (status bar included)

// ── Message buffers ───────────────────────────────────────────
bool hasUnreadMessage                    = false;
char lastReceivedMessage[MAX_MSG_LEN+1]  = "";
char typedMessage[MAX_MSG_LEN+1]         = "";
uint8_t messageLen                       = 0;
uint8_t currentLetterIdx                 = 0;

// ── Misc UI state ─────────────────────────────────────────────
bool          confirmLeaveCompose = false;
unsigned long sentEnteredAt       = 0;
int           batteryPct          = 100;

// ── Button / hold-scroll state ────────────────────────────────
bool          lastLeft        = HIGH;
bool          lastMid         = HIGH;
bool          lastRight       = HIGH;
unsigned long lastBtnTime     = 0;
unsigned long holdStartTime   = 0;
unsigned long lastRepeatAt    = 0;
int8_t        heldButton      = 0;   // 0=none, -1=left, +1=right

