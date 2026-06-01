#include <Arduino.h>
#include "Helper.h"

// =====================
// HARDWARE PIN CONFIG
// =====================
#define BTN_LEFT    2
#define BTN_SELECT  3
#define BTN_RIGHT   4

// =====================
// STATE MACHINE CORE DEFINITIONS
// =====================
enum ScreenState {
  STATE_HOME,
  STATE_INBOX,
  STATE_COMPOSE,
  STATE_SENT,
  STATE_GAMES
};

ScreenState currentState = STATE_HOME;
bool needRefresh = false;
bool fastUpdate = false; // Tracks if we are doing a fast, partial refresh
bool hasUnreadMessage = true;

const char alphabet[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ.!?<3";
const int alphabetSize = sizeof(alphabet) - 1;
int currentLetterIdx = 0;

char typedMessage[32] = "";
int messageLen = 0;
char lastReceivedMessage[32] = "I MISS YOU! <3";

bool lastLeftState   = HIGH;
bool lastSelectState = HIGH;
bool lastRightState  = HIGH;

// =====================
// CODE INJECTION POINTS
// =====================
#include "HomeState.h"
#include "InboxState.h"
#include "ComposeState.h"
#include "SentState.h"
#include "GamesState.h"

// =====================
// INPUT HANDLER DEBOUNCING
// =====================
void checkButtons() {
  bool currentLeft   = digitalRead(BTN_LEFT);
  bool currentSelect = digitalRead(BTN_SELECT);
  bool currentRight  = digitalRead(BTN_RIGHT);

  bool leftPressed   = (currentLeft == LOW && lastLeftState == HIGH);
  bool selectPressed = (currentSelect == LOW && lastSelectState == HIGH);
  bool rightPressed  = (currentRight == LOW && lastRightState == HIGH);

  if (leftPressed || selectPressed || rightPressed) {
    delay(50); // Simple hardware filter debounce
    
    switch (currentState) {
      case STATE_HOME:    handleHomeInput(leftPressed, selectPressed, rightPressed);    break;
      case STATE_INBOX:   handleInboxInput(leftPressed, selectPressed, rightPressed);   break;
      case STATE_COMPOSE: handleComposeInput(leftPressed, selectPressed, rightPressed); break;
      case STATE_GAMES:   handleGamesInput(leftPressed, selectPressed, rightPressed);   break;
      default: break;
    }
  }

  lastLeftState   = currentLeft;
  lastSelectState = currentSelect;
  lastRightState  = currentRight;
}

// =====================
// MAIN SCREEN CONTROLLER
// =====================
void drawMyScreen() {  
  if (fastUpdate) {
    // ONLY update the region below the static top bar!
    // Screen is 296x128. Top bar is 27 pixels tall. 
    display.setPartialWindow(0, 27, 296, 101);
  } else {
    // Full screen update to clear ghosting when switching major menus
    display.setFullWindow();
  }

  display.firstPage();
  do {
    // fillScreen only clears the area inside the active window!
    display.fillScreen(GxEPD_WHITE); 

    // GLOBAL STATUS BAR
    // We ONLY draw this if we are doing a full screen update.
    if (!fastUpdate) {
      drawText(10, 20, "LoveBox", &FreeMonoBold9pt7b);
      drawText(210, 20, "[98%]", &FreeMonoBold9pt7b);
      display.drawFastHLine(0, 26, 296, GxEPD_BLACK);
    }

    // 2. DYNAMIC BOTTOM NAVIGATION MENU GUIDES
    display.drawFastHLine(0, 102, 296, GxEPD_BLACK);
    
    switch (currentState) {
      case STATE_HOME:    drawHome();    break;
      case STATE_INBOX:   drawInbox();   break;
      case STATE_COMPOSE: drawCompose(); break;
      case STATE_SENT:    drawSent();    break;
      case STATE_GAMES:   drawGames();   break;
    }

  } while (display.nextPage()); // We also bypass endDraw() here
}

// =====================
// ARDUINO SETUP & LOOP
// =====================
void setup() {
  Serial.begin(9600);
  
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);

  SPI.begin();
  display.init(9600, true, 50, false);
  display.setRotation(1);
  display.clearScreen();

  drawMyScreen(); 
}

void loop() {
  checkButtons();

  if (needRefresh) {
    drawMyScreen();
    needRefresh = false;
    fastUpdate = false; // Always revert to full, clean refresh by default
  }

  if (currentState == STATE_SENT) {
    delay(3000); 
    currentState = STATE_HOME;
    needRefresh = true;
  }
}