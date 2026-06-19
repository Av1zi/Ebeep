#define STARTS_WITH(str, prefix) (strncmp(str, prefix, strlen(prefix)) == 0)

bool inTicTacToe = false;
bool TTT_hasOpponent = false;
char TTT_board[3][3];
bool TTT_myTurn = false;
int8_t TTT_selected = 0;
char TTT_SYMBOL = 'X';

// Square cells: size is the smaller of the two possible dimensions
// divided by 3, with a fixed offset to center the board
static const int16_t TTT_CELL  = min(SCREEN_W, CONTENT_H) / 3;
static const int16_t TTT_LEFT  = (SCREEN_W  - TTT_CELL * 3) / 2;
static const int16_t TTT_TOP   = CONTENT_Y + (CONTENT_H - TTT_CELL * 3) / 2;

void TTT_cellRect(int r, int c, int16_t &x, int16_t &y, int16_t &w, int16_t &h) {
  w = TTT_CELL;
  h = TTT_CELL;
  x = TTT_LEFT + c * TTT_CELL;
  y = TTT_TOP  + r * TTT_CELL;
}

void drawTicTacToe() {
  if (!TTT_hasOpponent) {
    drawCenteredText(SCREEN_W / 2, CONTENT_Y + 52, "Waiting for opponent...", &FreeMonoBold12pt7b);
    drawButtonHints("exit", "", "");
    return;
  }

  // Grid lines (use board bounds, not full screen)
  int16_t boardRight  = TTT_LEFT + TTT_CELL * 3;
  int16_t boardBottom = TTT_TOP  + TTT_CELL * 3;
  display.drawLine(TTT_LEFT + TTT_CELL,     TTT_TOP, TTT_LEFT + TTT_CELL,     boardBottom, GxEPD_BLACK);
  display.drawLine(TTT_LEFT + TTT_CELL * 2, TTT_TOP, TTT_LEFT + TTT_CELL * 2, boardBottom, GxEPD_BLACK);
  display.drawLine(TTT_LEFT, TTT_TOP + TTT_CELL,     boardRight, TTT_TOP + TTT_CELL,     GxEPD_BLACK);
  display.drawLine(TTT_LEFT, TTT_TOP + TTT_CELL * 2, boardRight, TTT_TOP + TTT_CELL * 2, GxEPD_BLACK);

  // Selection outline
  if (TTT_myTurn) {
    int16_t rx, ry, rw, rh;
    TTT_cellRect(TTT_selected / 3, TTT_selected % 3, rx, ry, rw, rh);
    const int PAD = 3;
    display.drawRect(rx + PAD, ry + PAD, rw - PAD * 2, rh - PAD * 2, GxEPD_BLACK);
  }

  // Symbols — FreeMonoBold12pt7b instead of 24pt
  for (int r = 0; r < 3; r++) {
    for (int c = 0; c < 3; c++) {
      char cell = TTT_board[r][c];
      if (cell == 'X' || cell == 'O') {
        const char* text = (cell == 'X') ? "X" : "O";
        int16_t rx, ry, rw, rh;
        TTT_cellRect(r, c, rx, ry, rw, rh);
        drawCenteredText(rx + rw / 2, ry + rh / 2 + 5, text, &FreeMonoBold12pt7b);
      }
    }
  }

  drawButtonHints("exit", "select", ">");
}

void startGameTTT() {
  inTicTacToe = true;
  needRefresh = true;
}

void enterTicTacToe() {
  // Reset all state cleanly on every entry
  memset(TTT_board, '\0', sizeof(TTT_board));
  TTT_selected = 0;
  if(TTT_hasOpponent){
    startGameTTT();
  } else {
    mqttClient.publish((String(RECIVER_ID) + "/games").c_str(), "TTT_START");
  }
  mqttClient.subscribe((String(BEEPER_ID) + "/games/TTT").c_str());
}


void checkTicTacToeMessages(char* payload) {
  Serial.println(payload);
  if (STARTS_WITH(payload, "TTT_START")) {
    TTT_hasOpponent = true;
    TTT_myTurn = true;
    startGameTTT();
  } else if (STARTS_WITH(payload, "TTT_LEFT")) {
    TTT_hasOpponent = false;
    needRefresh = true;
  } else if (payload[1] == ',' && inTicTacToe && !TTT_myTurn) {  // "R,C..." — just check the comma
    int row = payload[0] - '0';
    int col = payload[2] - '0';
    if (row >= 0 && row < 3 && col >= 0 && col < 3 && TTT_board[row][col] == '\0') {
      TTT_board[row][col] = 'O';
      TTT_myTurn = true;
      needRefresh = true;
      fastUpdate = true;
    }
  }
}

void leaveTicTacToe() {
  mqttClient.publish((String(RECIVER_ID) + "/games/TTT").c_str(), "TTT_LEFT");
  mqttClient.unsubscribe((String(BEEPER_ID) + "/games/TTT").c_str());
  inTicTacToe = false;
  currentState = STATE_HOME;
  needRefresh = true;
}

void handleTicTacToeInput(bool leftPressed, bool midPressed, bool rightPressed) {
  if (leftPressed) { //leave
    leaveTicTacToe();
  } else if (midPressed) { // confirm
    if (!TTT_myTurn) return;
    int row = TTT_selected / 3;
    int col = TTT_selected % 3;
    if (TTT_board[row][col] != '\0') return;
    TTT_board[row][col] = TTT_SYMBOL;
    TTT_myTurn = false;
    mqttClient.publish(
      (String(RECIVER_ID) + "/games/TTT").c_str(),
      (String(row) + "," + String(col)).c_str()
    );
    needRefresh = true;
    fastUpdate = true;
  } else if (rightPressed) { // Advance to next empty cell, skipping occupied ones
    int next = (TTT_selected + 1) % 9;
    int checked = 0;
    while (checked < 9) {
      int r = next / 3, c = next % 3;
      if (TTT_board[r][c] == '\0') break; // found an empty cell
      next = (next + 1) % 9;
      checked++;
    }
    // If all 9 are occupied, checked == 9 and we just stay put
    if (checked < 9) {
      TTT_selected = next;
      needRefresh = true;
      fastUpdate = true;
    }
  }
}