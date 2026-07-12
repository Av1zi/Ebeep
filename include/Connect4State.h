#pragma once

#define STARTS_WITH(str, prefix) (strncmp(str, prefix, strlen(prefix)) == 0)


// ═══════════════════════════════════════════════════════════════
//  Connect4State.h
// ═══════════════════════════════════════════════════════════════

bool    inConnect4      = false;
bool    C4_hasOpponent  = false;
bool    C4_pendingStart = false;   // sent C4_START, waiting for ACK / race resolution
bool    C4_isInitiator  = false;
char    C4_board[42];              // flat 7x6, row-major, row 0 = top
bool    C4_myTurn       = false;
int8_t  C4_selected     = 3;       // currently-highlighted column
uint8_t C4_moveCount    = 0;
int8_t  C4_winStart     = -1;      // flat board index of one end of the winning run, -1 = none
int8_t  C4_winEnd       = -1;
char    C4_SYMBOL       = 'R';     // assigned per-game: initiator='R', joiner='Y'

// ── Game-over / rematch / popup state, packed into one byte ─────
enum : uint8_t { C4_NONE, C4_WIN, C4_LOSE, C4_DRAW };
uint8_t C4_flags = C4_NONE;
#define C4_RESULT()           (C4_flags & 0x03)
#define C4_SET_RESULT(r)      (C4_flags = (C4_flags & ~0x03) | (r))
#define C4_I_WANT_REMATCH     (C4_flags & 0x04)
#define C4_CONFIRM_LEAVE      (C4_flags & 0x10)

// Cached MQTT topics — zero runtime heap allocation allocation
static char C4_topicOut[32]     = "";      // "<RECIVER_ID>/games/C4"
static char C4_topicGameOut[32] = "";      // "<RECIVER_ID>/games"
static char C4_topicIn[32]      = "";      // "<BEEPER_ID>/games/C4"

static constexpr int8_t C4_ROWS = 6;
static constexpr int8_t C4_COLS = 7;

static constexpr int16_t C4_TOP_GAP = 10;

static const int16_t C4_CELL = min(SCREEN_W / C4_COLS, (CONTENT_H - C4_TOP_GAP) / C4_ROWS);
static const int16_t C4_LEFT = (SCREEN_W - C4_CELL * C4_COLS) / 2;
static const int16_t C4_TOP  = CONTENT_Y + C4_TOP_GAP +
                                ((CONTENT_H - C4_TOP_GAP) - C4_CELL * C4_ROWS) / 2;

inline int8_t C4_idx(int8_t row, int8_t col) { return row * C4_COLS + col; }

inline void C4_cellRect(int8_t row, int8_t col, int16_t &x, int16_t &y) {
  x = C4_LEFT + col * C4_CELL;
  y = C4_TOP  + row * C4_CELL;
}

// Lowest empty row in `col` (gravity fills bottom-up), or -1 if full.
int8_t C4_dropRow(int8_t col) {
  for (int8_t row = C4_ROWS - 1; row >= 0; row--)
    if (C4_board[C4_idx(row, col)] == '\0') return row;
  return -1;
}

// 4-in-a-row check anchored on the just-placed piece at (row, col).
bool C4_checkWin(int8_t row, int8_t col) {
  static const int8_t DIRS[4][2] = { {1,0}, {0,1}, {1,1}, {1,-1} };
  const char sym = C4_board[C4_idx(row, col)];

  for (uint8_t d = 0; d < 4; d++) {
    const int8_t dr = DIRS[d][0], dc = DIRS[d][1];

    int8_t r1 = row, c1 = col;
    while (true) {
      const int8_t nr = r1 + dr, nc = c1 + dc;
      if (nr < 0 || nr >= C4_ROWS || nc < 0 || nc >= C4_COLS) break;
      if (C4_board[C4_idx(nr, nc)] != sym) break;
      r1 = nr; c1 = nc;
    }
    int8_t r2 = row, c2 = col;
    while (true) {
      const int8_t nr = r2 - dr, nc = c2 - dc;
      if (nr < 0 || nr >= C4_ROWS || nc < 0 || nc >= C4_COLS) break;
      if (C4_board[C4_idx(nr, nc)] != sym) break;
      r2 = nr; c2 = nc;
    }

    if (max(abs(r1 - r2), abs(c1 - c2)) + 1 >= 4) {
      C4_winStart = C4_idx(r1, c1);
      C4_winEnd   = C4_idx(r2, c2);
      return true;
    }
  }
  return false;
}

void C4_drawBoard() {
  for (int8_t c = 1; c < C4_COLS; c++)
    display.drawFastVLine(C4_LEFT + c * C4_CELL, C4_TOP, C4_CELL * C4_ROWS, GxEPD_BLACK);
  for (int8_t r = 1; r < C4_ROWS; r++)
    display.drawFastHLine(C4_LEFT, C4_TOP + r * C4_CELL, C4_CELL * C4_COLS, GxEPD_BLACK);
  display.drawRect(C4_LEFT, C4_TOP, C4_CELL * C4_COLS, C4_CELL * C4_ROWS, GxEPD_BLACK);

  const int16_t rad = C4_CELL / 2 - 2;
  for (int8_t row = 0; row < C4_ROWS; row++) {
    for (int8_t col = 0; col < C4_COLS; col++) {
      const char cell = C4_board[C4_idx(row, col)];
      if (cell == '\0') continue;
      int16_t x, y;
      C4_cellRect(row, col, x, y);
      const int16_t cx = x + C4_CELL / 2, cy = y + C4_CELL / 2;
      if (cell == C4_SYMBOL) display.fillCircle(cx, cy, rad, GxEPD_BLACK);
      else                   display.drawCircle(cx, cy, rad, GxEPD_BLACK);
    }
  }
}

void C4_drawSelector() {
  const int16_t cx   = C4_LEFT + C4_selected * C4_CELL + C4_CELL / 2;
  const int16_t topY = CONTENT_Y + 2;
  const int16_t botY = C4_TOP - 3;
  display.fillTriangle(cx - 4, topY, cx + 4, topY, cx, botY, GxEPD_BLACK);
}

bool C4_evaluateOutcome(int8_t row, int8_t col) {
  C4_moveCount++;

  if (C4_checkWin(row, col)) {
    C4_SET_RESULT(C4_board[C4_idx(row, col)] == C4_SYMBOL ? C4_WIN : C4_LOSE);
    inConnect4  = false;
    needRefresh = true;
    fastUpdate  = false;
    return true;
  }
  if (C4_moveCount >= (uint8_t)(C4_ROWS * C4_COLS)) {
    C4_SET_RESULT(C4_DRAW);
    inConnect4  = false;
    needRefresh = true;
    fastUpdate  = false;
    return true;
  }

  needRefresh = true;
  fastUpdate  = true;
  return false;
}

void drawConnect4() {
  if (!C4_hasOpponent) {
    drawCenteredText(SCREEN_W / 2, CONTENT_Y + 52, "Waiting for opponent...", &FreeMonoBold9pt7b);
    drawButtonHints("exit", "", "");
    return;
  }

  if (C4_CONFIRM_LEAVE) {
    drawConfirmPopup("Quit game?");
    return;
  }

  const uint8_t result = C4_RESULT();
  if (result != C4_NONE) {
    C4_drawBoard();

    if (C4_winStart >= 0) {
      int16_t x0, y0, x1, y1;
      C4_cellRect(C4_winStart / C4_COLS, C4_winStart % C4_COLS, x0, y0);
      C4_cellRect(C4_winEnd   / C4_COLS, C4_winEnd   % C4_COLS, x1, y1);
      display.drawLine(x0 + C4_CELL / 2, y0 + C4_CELL / 2,
                        x1 + C4_CELL / 2, y1 + C4_CELL / 2, GxEPD_BLACK);
    }

    display.fillRect(0, CONTENT_Y, SCREEN_W, C4_TOP_GAP, GxEPD_WHITE);
    const char* headline = (result == C4_WIN) ? "You Win!"
                          : (result == C4_LOSE) ? "You Lose" : "Draw!";
    drawCenteredText(SCREEN_W / 2, CONTENT_Y + 9, headline, &FreeMonoBold9pt7b);

    drawButtonHints("exit", "", C4_I_WANT_REMATCH ? "..." : "again");
    return;
  }

  C4_drawBoard();
  if (C4_myTurn) C4_drawSelector();

  drawButtonHints("exit", "drop", ">");
}

void startGameC4() {
  currentState = STATE_CONNECT4;
  inConnect4   = true;
  needRefresh  = true;
}

static void C4_resetBoard() {
  memset(C4_board, '\0', sizeof(C4_board));
  C4_selected  = C4_COLS / 2;
  C4_flags     = C4_NONE;
  C4_moveCount = 0;
  C4_winStart  = -1;
  C4_winEnd    = -1;
}

void C4_joinAsOpponent() {
  C4_isInitiator  = false;
  C4_SYMBOL       = 'Y';
  C4_myTurn       = false;
  C4_pendingStart = false;
  C4_hasOpponent  = true;
  mqttClient.publish(C4_topicGameOut, "C4_START_ACK");
  startGameC4();
}

void C4_becomeInitiator() {
  C4_isInitiator  = true;
  C4_SYMBOL       = 'R';
  C4_myTurn       = true;
  C4_pendingStart = false;
  C4_hasOpponent  = true;
  startGameC4();
}

void enterConnect4() {
  if (C4_topicOut[0] == '\0') {
    snprintf(C4_topicOut, sizeof(C4_topicOut), "%s/games/C4", RECIVER_ID);
    snprintf(C4_topicGameOut, sizeof(C4_topicGameOut), "%s/games", RECIVER_ID);
    snprintf(C4_topicIn, sizeof(C4_topicIn), "%s/games/C4", BEEPER_ID);
  }

  C4_resetBoard();
  mqttClient.subscribe(C4_topicIn);

  if (C4_hasOpponent) {
    C4_joinAsOpponent();
  } else {
    C4_pendingStart = true;
    needRefresh     = true;
    mqttClient.publish(C4_topicGameOut, "C4_START");
  }
}

void C4_startRematch(bool iGoFirst) {
  C4_resetBoard();
  C4_myTurn    = iGoFirst;
  inConnect4   = true;
  needRefresh  = true;
  fastUpdate   = false;
}

void C4_requestRematch() {
  if (C4_I_WANT_REMATCH) return;
  C4_flags |= 0x04;
  mqttClient.publish(C4_topicOut, "C4_REMATCH");
  needRefresh = true;
  fastUpdate  = true;
}

void checkConnect4Messages(char* payload) {
  if (STARTS_WITH(payload, "C4_REMATCH_ACK")) {
    C4_startRematch(true);
  } else if (STARTS_WITH(payload, "C4_REMATCH")) {
    if (C4_I_WANT_REMATCH) {
      C4_startRematch(strcmp(BEEPER_ID, RECIVER_ID) < 0);
    } else {
      mqttClient.publish(C4_topicOut, "C4_REMATCH_ACK");
      C4_startRematch(false);
    }
  } else if (inConnect4 && !C4_myTurn) {
    const int8_t col = payload[0] - '0';
    if (col >= 0 && col < C4_COLS) {
      const int8_t row = C4_dropRow(col);
      if (row >= 0) {
        C4_board[C4_idx(row, col)] = (C4_SYMBOL == 'R') ? 'Y' : 'R';
        C4_myTurn = true;
        C4_evaluateOutcome(row, col);
      }
    }
  }
}

void leaveConnect4() {
  mqttClient.publish(C4_topicGameOut, "C4_LEFT");
  mqttClient.unsubscribe(C4_topicIn);
  inConnect4      = false;
  C4_pendingStart = false;
  C4_hasOpponent  = false;
  C4_flags        = C4_NONE;
  currentState    = STATE_HOME;
  needRefresh     = true;
}

void handleConnect4Input(bool leftPressed, bool midPressed, bool rightPressed) {
  if (!C4_hasOpponent) {
    if (leftPressed) leaveConnect4();
    return;
  }

  if (C4_CONFIRM_LEAVE) {
    if (leftPressed) {
      C4_flags &= ~0x10;
      needRefresh = true;
      fastUpdate  = true;
    } else if (rightPressed) {
      leaveConnect4();
    }
    return;
  }

  if (C4_RESULT() != C4_NONE) {
    if (leftPressed)       leaveConnect4();
    else if (rightPressed) C4_requestRematch();
    return;
  }

  if (leftPressed) { 
    C4_flags |= 0x10;
    needRefresh = true;
    fastUpdate  = true;
  } else if (midPressed) { 
    if (!C4_myTurn) return;
    const int8_t row = C4_dropRow(C4_selected);
    if (row < 0) return;  
    C4_board[C4_idx(row, C4_selected)] = C4_SYMBOL;
    C4_myTurn = false;
    
    char colBuf[2];
    snprintf(colBuf, sizeof(colBuf), "%d", C4_selected);
    mqttClient.publish(C4_topicOut, colBuf);
    
    C4_evaluateOutcome(row, C4_selected);
  } else if (rightPressed) { 
    int8_t next = (C4_selected + 1) % C4_COLS;
    uint8_t checked = 0;
    while (checked < C4_COLS && C4_dropRow(next) < 0) {
      next = (next + 1) % C4_COLS;
      checked++;
    }
    if (checked < C4_COLS) {
      C4_selected = next;
      needRefresh = true;
      fastUpdate  = true;
    }
  }
}