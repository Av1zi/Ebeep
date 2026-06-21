#define STARTS_WITH(str, prefix) (strncmp(str, prefix, strlen(prefix)) == 0)

bool    inTicTacToe     = false;
bool    TTT_hasOpponent = false;
bool    TTT_pendingStart= false;   // we've sent TTT_START, waiting for ACK / race resolution
bool    TTT_isInitiator = false;   // true = we went first in the original game
char    TTT_board[9];              // flat 3x3 — avoids /3, %3 divisions on every access
bool    TTT_myTurn      = false;
int8_t  TTT_selected    = 0;
int8_t  TTT_winLine     = -1;      // index into TTT_LINES for the winning line, -1 = none/draw
char    TTT_SYMBOL      = 'X';     // assigned per-game: initiator='X', joiner='O'

// ── Game-over / rematch / popup state, packed into one byte ─────
//   bits: 0-1 = result (0 none, 1 win, 2 lose, 3 draw)
//         2   = I requested rematch (waiting for opponent's ACK)
//         4   = leave-confirm popup showing
enum : uint8_t { TTT_NONE, TTT_WIN, TTT_LOSE, TTT_DRAW };
uint8_t TTT_flags = TTT_NONE;
#define TTT_RESULT()           (TTT_flags & 0x03)
#define TTT_SET_RESULT(r)      (TTT_flags = (TTT_flags & ~0x03) | (r))
#define TTT_I_WANT_REMATCH     (TTT_flags & 0x04)
#define TTT_CONFIRM_LEAVE      (TTT_flags & 0x10)

// Cached MQTT topics — built once instead of allocating a String every publish.
static String TTT_topicOut;      // "<RECIVER_ID>/games/TTT"
static String TTT_topicGameOut;  // "<RECIVER_ID>/games"
static String TTT_topicIn;       // "<BEEPER_ID>/games/TTT"

// Square cells: size is the smaller of the two possible dimensions
// divided by 3, with a fixed offset to center the board
static const int16_t TTT_CELL  = min(SCREEN_W, CONTENT_H) / 3;
static const int16_t TTT_LEFT  = (SCREEN_W  - TTT_CELL * 3) / 2;
static const int16_t TTT_TOP   = CONTENT_Y + (CONTENT_H - TTT_CELL * 3) / 2;

inline void TTT_cellRect(int8_t idx, int16_t &x, int16_t &y, int16_t &w, int16_t &h) {
  w = TTT_CELL;
  h = TTT_CELL;
  x = TTT_LEFT + (idx % 3) * TTT_CELL;
  y = TTT_TOP  + (idx / 3) * TTT_CELL;
}

// All 8 win lines as flat indices into TTT_board[9]. PROGMEM keeps it out of RAM.
static const int8_t TTT_LINES[8][3] PROGMEM = {
  {0,1,2}, {3,4,5}, {6,7,8},   // rows
  {0,3,6}, {1,4,7}, {2,5,8},   // cols
  {0,4,8}, {2,4,6}             // diagonals
};

// Returns 'X', 'O' (winner symbol), 'D' (draw), or '\0' (game still going).
char TTT_checkOutcome() {
  for (uint8_t i = 0; i < 8; i++) {
    const char a = TTT_board[pgm_read_byte(&TTT_LINES[i][0])];
    if (a == '\0') continue;  // empty cell can't start a winning line — cheap skip
    const char b = TTT_board[pgm_read_byte(&TTT_LINES[i][1])];
    const char c = TTT_board[pgm_read_byte(&TTT_LINES[i][2])];
    if (a == b && b == c) { TTT_winLine = i; return a; }
  }
  for (uint8_t i = 0; i < 9; i++)
    if (TTT_board[i] == '\0') return '\0';  // still open cells → game ongoing
  return 'D';
}

// Draws the 3x3 grid + symbols only (no button hints). Shared by the live
// board view and the game-over screen so the final position stays visible.
void TTT_drawBoard() {
  const int16_t boardRight  = TTT_LEFT + TTT_CELL * 3;
  const int16_t boardBottom = TTT_TOP  + TTT_CELL * 3;
  display.drawLine(TTT_LEFT + TTT_CELL,     TTT_TOP, TTT_LEFT + TTT_CELL,     boardBottom, GxEPD_BLACK);
  display.drawLine(TTT_LEFT + TTT_CELL * 2, TTT_TOP, TTT_LEFT + TTT_CELL * 2, boardBottom, GxEPD_BLACK);
  display.drawLine(TTT_LEFT, TTT_TOP + TTT_CELL,     boardRight, TTT_TOP + TTT_CELL,     GxEPD_BLACK);
  display.drawLine(TTT_LEFT, TTT_TOP + TTT_CELL * 2, boardRight, TTT_TOP + TTT_CELL * 2, GxEPD_BLACK);

  for (int8_t i = 0; i < 9; i++) {
    const char cell = TTT_board[i];
    if (cell == 'X' || cell == 'O') {
      int16_t rx, ry, rw, rh;
      TTT_cellRect(i, rx, ry, rw, rh);
      const char text[2] = { cell, '\0' };
      drawCenteredText(rx + rw / 2, ry + rh / 2 + 5, text, &FreeMonoBold12pt7b);
    }
  }
}

// Evaluates TTT_board after a move. If the game just ended, updates TTT_flags,
// flips inTicTacToe off, and requests the appropriate redraw. Returns true if ended.
bool TTT_evaluateOutcome() {
  const char outcome = TTT_checkOutcome();
  if (outcome == '\0') {
    needRefresh = true;
    fastUpdate  = true;
    return false;
  }

  TTT_SET_RESULT(outcome == TTT_SYMBOL ? TTT_WIN : (outcome == 'D' ? TTT_DRAW : TTT_LOSE));
  inTicTacToe = false;
  needRefresh = true;
  fastUpdate  = false;  // full refresh into game-over screen
  return true;
}

void drawTicTacToe() {
  if (!TTT_hasOpponent) {
    drawCenteredText(SCREEN_W / 2, CONTENT_Y + 52, "Waiting for opponent...", &FreeMonoBold12pt7b);
    drawButtonHints("exit", "", "");
    return;
  }

  // ── Leave-confirm popup (drawn over the live board) ───────
  if (TTT_CONFIRM_LEAVE) {
    TTT_drawBoard();
    constexpr int16_t pw = 210, ph = 48;
    constexpr int16_t px = (SCREEN_W - pw) / 2;
    const     int16_t py = CONTENT_Y + (CONTENT_H - ph) / 2;
    display.fillRect(px,     py,     pw,     ph,     GxEPD_WHITE);
    display.drawRect(px,     py,     pw,     ph,     GxEPD_BLACK);
    display.drawRect(px + 2, py + 2, pw - 4, ph - 4, GxEPD_BLACK);
    drawCenteredText(SCREEN_W / 2, py + 30, "Quit game?", &FreeMonoBold9pt7b);
    drawButtonHints("No", "", "Yes");
    return;
  }

  // ── Game-over screen ──────────────────────────────────────
  const uint8_t result = TTT_RESULT();
  if (result != TTT_NONE) {
    TTT_drawBoard();

    // Strike a line through the winning three cells so it's clear how the game ended.
    if (TTT_winLine >= 0) {
      int16_t x0, y0, x1, y1, w, h;
      TTT_cellRect(pgm_read_byte(&TTT_LINES[TTT_winLine][0]), x0, y0, w, h);
      TTT_cellRect(pgm_read_byte(&TTT_LINES[TTT_winLine][2]), x1, y1, w, h);
      display.drawLine(x0 + w / 2, y0 + h / 2, x1 + w / 2, y1 + h / 2, GxEPD_BLACK);
    }

    // Headline banner over the board, white-filled behind it for legibility.
    constexpr int16_t bannerH = 22;
    const int16_t bannerY = TTT_TOP - bannerH - 2;
    display.fillRect(0, bannerY, SCREEN_W, bannerH, GxEPD_WHITE);
    const char* headline = (result == TTT_WIN) ? "You Win!"
                          : (result == TTT_LOSE) ? "You Lose" : "Draw!";
    drawCenteredText(SCREEN_W / 2, bannerY + 16, headline, &FreeMonoBold12pt7b);

    const char* sub = TTT_I_WANT_REMATCH ? "Waiting for opponent..." : "";
    if (sub[0] != '\0') {
      constexpr int16_t subH = 14;
      const int16_t subY = TTT_TOP + TTT_CELL * 3 + 2;
      display.fillRect(0, subY, SCREEN_W, subH, GxEPD_WHITE);
      drawCenteredText(SCREEN_W / 2, subY + 11, sub, &FreeMonoBold9pt7b);
    }

    drawButtonHints("exit", "", TTT_I_WANT_REMATCH ? "..." : "again");
    return;
  }

  // ── Live board ─────────────────────────────────────────────
  TTT_drawBoard();

  // Selection outline
  if (TTT_myTurn) {
    int16_t rx, ry, rw, rh;
    TTT_cellRect(TTT_selected, rx, ry, rw, rh);
    constexpr int PAD = 3;
    display.drawRect(rx + PAD, ry + PAD, rw - PAD * 2, rh - PAD * 2, GxEPD_BLACK);
  }

  drawButtonHints("exit", "select", ">");
}

void startGameTTT() {
  inTicTacToe = true;
  needRefresh = true;
}

// Shared reset used by both a fresh game entry and a rematch.
static void TTT_resetBoard() {
  memset(TTT_board, '\0', sizeof(TTT_board));
  TTT_selected = 0;
  TTT_flags    = TTT_NONE;
  TTT_winLine  = -1;
}

// We've been confirmed as the joiner — opponent invited us, or lost the
// simultaneous-press tiebreak below. ACK them so they stop waiting.
void TTT_joinAsOpponent() {
  TTT_isInitiator  = false;
  TTT_SYMBOL       = 'O';
  TTT_myTurn       = false;
  TTT_pendingStart = false;
  TTT_hasOpponent  = true;
  mqttClient.publish(TTT_topicGameOut.c_str(), "TTT_START_ACK");
  startGameTTT();
}

// We've been confirmed as the initiator — opponent ack'd our invite, or we
// won the simultaneous-press tiebreak below.
void TTT_becomeInitiator() {
  TTT_isInitiator  = true;
  TTT_SYMBOL       = 'X';
  TTT_myTurn       = true;
  TTT_pendingStart = false;
  TTT_hasOpponent  = true;
  startGameTTT();
}

void enterTicTacToe() {
  // Build topic strings once per game session rather than per publish.
  TTT_topicOut     = String(RECIVER_ID) + "/games/TTT";
  TTT_topicGameOut = String(RECIVER_ID) + "/games";
  TTT_topicIn      = String(BEEPER_ID)  + "/games/TTT";

  TTT_resetBoard();
  mqttClient.subscribe(TTT_topicIn.c_str());

  if (TTT_hasOpponent) {
    // They invited us first (gameReqHandler already saw their TTT_START) — join now.
    TTT_joinAsOpponent();
  } else {
    // Nobody's invited us — declare ourselves and wait. If they pressed Play
    // at the same moment, gameReqHandler resolves the race deterministically
    // (see TTT_START handling in Callbacks.h) instead of both sides assuming X.
    TTT_pendingStart = true;
    needRefresh      = true;
    mqttClient.publish(TTT_topicGameOut.c_str(), "TTT_START");
  }
}

// Resets the board for a rematch. iGoFirst is computed explicitly by the
// caller from the message protocol below — no timing inference involved.
void TTT_startRematch(bool iGoFirst) {
  TTT_resetBoard();
  TTT_myTurn  = iGoFirst;
  inTicTacToe = true;
  needRefresh = true;
  fastUpdate  = false;
}

void TTT_requestRematch() {
  if (TTT_I_WANT_REMATCH) return;  // already requested
  TTT_flags |= 0x04;
  mqttClient.publish(TTT_topicOut.c_str(), "TTT_REMATCH");
  needRefresh = true;
  fastUpdate  = true;
}

void checkTicTacToeMessages(char* payload) {
  Serial.println(payload);
  if (STARTS_WITH(payload, "TTT_LEFT")) {
    TTT_hasOpponent = false;
    needRefresh = true;
  } else if (STARTS_WITH(payload, "TTT_REMATCH_ACK")) {
    // Our proposal was accepted — we go first.
    TTT_startRematch(true);
  } else if (STARTS_WITH(payload, "TTT_REMATCH")) {
    if (TTT_I_WANT_REMATCH) {
      // Simultaneous request race — neither side ACK'd the other.
      // Same deterministic tiebreak used for game start (both sides agree on this).
      TTT_startRematch(strcmp(BEEPER_ID, RECIVER_ID) < 0);
    } else {
      // Clean accept: they proposed, we hadn't asked yet. ACK and let them go first.
      mqttClient.publish(TTT_topicOut.c_str(), "TTT_REMATCH_ACK");
      TTT_startRematch(false);
    }
  } else if (payload[1] == ',' && inTicTacToe && !TTT_myTurn) {  // "R,C..." — just check the comma
    const int8_t row = payload[0] - '0';
    const int8_t col = payload[2] - '0';
    if (row >= 0 && row < 3 && col >= 0 && col < 3) {
      const int8_t idx = row * 3 + col;
      const char oppSymbol = (TTT_SYMBOL == 'X') ? 'O' : 'X';
      if (TTT_board[idx] == '\0') {
        TTT_board[idx] = oppSymbol;
        TTT_myTurn = true;
        TTT_evaluateOutcome();
      }
    }
  }
}

void leaveTicTacToe() {
  // Sent on the broad "/games" channel (not "/games/TTT") so it reaches the
  // opponent even if they haven't joined yet and never subscribed to TTT's
  // own topic — e.g. cancelling an invite nobody's accepted.
  mqttClient.publish(TTT_topicGameOut.c_str(), "TTT_LEFT");
  mqttClient.unsubscribe(TTT_topicIn.c_str());
  inTicTacToe      = false;
  TTT_pendingStart = false;
  TTT_hasOpponent  = false;
  TTT_flags        = TTT_NONE;
  currentState     = STATE_HOME;
  needRefresh      = true;
}

void handleTicTacToeInput(bool leftPressed, bool midPressed, bool rightPressed) {
  // ── Waiting for opponent: nothing to lose yet, skip the confirm popup ──
  if (!TTT_hasOpponent) {
    if (leftPressed) leaveTicTacToe();
    return;
  }

  // ── Leave-confirm popup ────────────────────────────────────
  if (TTT_CONFIRM_LEAVE) {
    if (leftPressed) {
      TTT_flags &= ~0x10;
      needRefresh = true;
      fastUpdate  = true;
    } else if (rightPressed) {
      leaveTicTacToe();
    }
    return;
  }

  // ── Game-over screen: LEFT = exit, RIGHT = request rematch ──
  if (TTT_RESULT() != TTT_NONE) {
    if (leftPressed)       leaveTicTacToe();
    else if (rightPressed) TTT_requestRematch();
    return;
  }

  if (leftPressed) { // ask before leaving mid-game
    TTT_flags |= 0x10;
    needRefresh = true;
    fastUpdate  = true;
  } else if (midPressed) { // confirm
    if (!TTT_myTurn || TTT_board[TTT_selected] != '\0') return;
    TTT_board[TTT_selected] = TTT_SYMBOL;
    TTT_myTurn = false;
    const int8_t row = TTT_selected / 3, col = TTT_selected % 3;
    mqttClient.publish(TTT_topicOut.c_str(), (String(row) + "," + String(col)).c_str());
    TTT_evaluateOutcome();
  } else if (rightPressed) { // Advance to next empty cell, skipping occupied ones
    int8_t next = (TTT_selected + 1) % 9;
    uint8_t checked = 0;
    while (checked < 9 && TTT_board[next] != '\0') {
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