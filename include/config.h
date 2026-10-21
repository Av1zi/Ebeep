#pragma once

#include <secret.h>  // HOTSPOT_PASSWORD, MQTT_SERVER, MQTT_PORT, MQTT_PASSWORD
// not even needed can just write the values directly in config.h, but this keeps them out of git.

// ═══════════════════════════════════════════════════════════════
//  config.h - Hardware pins, layout constants, and app settings.
// ═══════════════════════════════════════════════════════════════

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

// ── Battery Sensing ───────────────────────────────────────────
//   2×200kΩ divider (1:2) on A0
#define BATTERY_DIVIDER_RATIO     2.0f
#define BATTERY_MIN_MV            3000UL   // 0%
#define BATTERY_MAX_MV            4200UL   // 100%
#define BATTERY_READ_INTERVAL_MS  3000UL   // how often we sample the ADC
#define BATTERY_EMA_ALPHA         0.15f

// ── Power Management ──────────────────────────────────────────
//   Asleep : deep sleep with "Sleeping" on screen, no timer wake at all.
//            BTN_LEFT / BTN_SELECT wakes it up and it connects.
//   Active : awake, WiFi modem-sleep, for ACTIVE_TIMEOUT_MS after the last press.
//   Messages are published retained, so the last one sent while we slept
//   is waiting on the broker when we wake.
#define ACTIVE_TIMEOUT_MS         (3UL * 60 * 1000)    // stay awake this long after a button press
#define IDLE_HARD_TIMEOUT_MS      (15UL * 60 * 1000)   // mid-game / mid-draft: give up and sleep after this
#define STATUS_REFRESH_MS         (2UL * 60 * 1000)    // battery-only partial refresh cadence
#define LOW_BATT_PCT              10                   // below this: status bar warning
#define WIFI_FAST_CONNECT_MS      2500UL               // reconnect to last network (no scan)
#define WIFI_CONNECT_TIMEOUT_MS   5000UL               // per known network found in a scan
#define WIFI_PORTAL_TIMEOUT_S     300                  // setup hotspot gives up (and sleeps) after this
#define MAX_KNOWN_NETWORKS        5                    // remembered WiFi networks, oldest dropped first


// ── Device Identity ───────────────────────────────────────────
//  THE ONLY LINE TO CHANGE BETWEEN DEVICES.
#define DEVICE_NUM  1   // 1 or 2

#if DEVICE_NUM == 1
  #define AP_NAME    "Ebeep_1_config"
  constexpr const char* BEEPER_ID  = "Beeper_1";
  constexpr const char* RECIVER_ID = "Beeper_2";
#elif DEVICE_NUM == 2
  #define AP_NAME    "Ebeep_2_config"
  constexpr const char* BEEPER_ID  = "Beeper_2";
  constexpr const char* RECIVER_ID = "Beeper_1";
#else
  #error "DEVICE_NUM must be 1 or 2"
#endif    

// ── WiFi / MQTT ───────────────────────────────────────────────
// most of these are defined in a diffrent file (secret.h), to keep them out of git.
#define WIFI_RECONNECT_GRACE_MS   (10UL * 1000)
#define AP_password HOTSPOT_PASSWORD // AP_password: leave empty to create an open acess point.

// MQTT username matches BEEPER_ID
// Password is shared between both devices for simplicity.
constexpr const char* MQTT_SERVER_ADDR  = MQTT_SERVER;
constexpr uint16_t    MQTT_SERVER_PORT  = MQTT_PORT;
constexpr const char* MQTT_USERNAME_STR = BEEPER_ID;
constexpr const char* MQTT_PASSWORD_STR = MQTT_PASSWORD;


// Defined in main.cpp but declared here so PowerState.h can call them
bool connectMQTT();
void markInboxRead();


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
  STATE_TICTACTOE,
  STATE_BLACKJACK,
  STATE_CONNECT4,
  STATE_PORTAL,   // setup hotspot open, no known WiFi in range
  STATE_SLEEP     // drawn right before deep sleep
};

ScreenState currentState = STATE_WIFI;

// ── Display refresh flags ─────────────────────────────────────
bool needRefresh = false;   // true  -> redraw this loop tick
bool fastUpdate  = false;   // true  -> partial refresh (content only)
                            // false -> full refresh (status bar included)

// ── Message buffers ───────────────────────────────────────────
// RTC_DATA_ATTR = survives deep sleep, so a retained message re-delivered
// on every wake can be recognised as one we already have.
RTC_DATA_ATTR bool hasUnreadMessage                    = false;
RTC_DATA_ATTR char lastReceivedMessage[MAX_MSG_LEN+1]  = "";
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

// ── Network status shown in the status bar ────────────────────
// Kept in variables (not read live) so the sleep screen can draw them after the radio is off.
bool netWifiOk = false;
bool netMqttOk = false;

// ── Power state ────────────────────────────────────────────────
unsigned long lastActivityMillis = 0;   // reset on any button press or message — see PowerState.h markActivity()