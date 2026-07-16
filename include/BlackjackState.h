#pragma once

// ═══════════════════════════════════════════════════════════════
//  BlackjackState.h
// ═══════════════════════════════════════════════════════════════

// ── Phase ────────────────────────────────────────────────────
enum BjPhase : uint8_t { BJ_BETTING, BJ_PLAYER, BJ_RESULT };
static BjPhase bjPhase = BJ_BETTING;

// ── Balance & bet ────────────────────────────────────────────
static int16_t bjBalance  = 100;
static int16_t bjBet      = 25;    // default = 1/4 of starting balance
static uint8_t bjBetFrac  = 0;     // 0=1/4  1=1/2  2=3/4  3=all-in

// ── Deck ─────────────────────────────────────────────────────
static uint8_t bjDeck[52];
static uint8_t bjDeckTop = 52;

static void bjShuffle() {
  for (uint8_t i = 0; i < 52; i++) bjDeck[i] = i;
  for (uint8_t i = 51; i > 0; i--) {
    uint8_t j = random(i + 1);
    uint8_t t = bjDeck[i]; bjDeck[i] = bjDeck[j]; bjDeck[j] = t;
  }
  bjDeckTop = 0;
}

static uint8_t bjDeal() {
  if (bjDeckTop >= 52) bjShuffle();
  return bjDeck[bjDeckTop++];
}

// ── Hands ────────────────────────────────────────────────────
static uint8_t bjPlayer[2][11];
static uint8_t bjPlayerLen[2];
static uint8_t bjDealer[11];
static uint8_t bjDealerLen;
static bool    bjSplit      = false;
static uint8_t bjActiveHand = 0;

// ── Card math ────────────────────────────────────────────────
static uint8_t bjRank(uint8_t card) { return card % 13; }

static uint8_t bjPoints(uint8_t rank) {
  if (rank == 0)  return 11;
  if (rank >= 10) return 10;
  return rank + 1;
}

static uint8_t bjHandValue(const uint8_t* hand, uint8_t len) {
  uint8_t total = 0, aces = 0;
  for (uint8_t i = 0; i < len; i++) {
    uint8_t p = bjPoints(bjRank(hand[i]));
    total += p;
    if (p == 11) aces++;
  }
  while (total > 21 && aces > 0) { total -= 10; aces--; }
  return total;
}

static bool bjIsSoft(const uint8_t* hand, uint8_t len) {
  uint8_t total = 0, aces = 0;
  for (uint8_t i = 0; i < len; i++) {
    uint8_t p = bjPoints(bjRank(hand[i]));
    total += p;
    if (p == 11) aces++;
  }
  return (aces > 0 && total <= 21);
}

// ── Bet fraction helpers ──────────────────────────────────────
static const char* BJ_FRAC_LABELS[] = { "1/4", "1/2", "3/4", "All" };

static int16_t bjBetFromFrac(uint8_t frac) {
  int16_t b;
  switch (frac) {
    case 0: b = bjBalance / 4;   break;
    case 1: b = bjBalance / 2;   break;
    case 2: b = (bjBalance * 3) / 4; break;
    default: b = bjBalance;      break;
  }
  return max((int16_t)1, b);
}

// ── Action menu ──────────────────────────────────────────────
enum BjAction : uint8_t { BJ_HIT, BJ_STAND, BJ_DOUBLE, BJ_SPLIT, BJ_EXIT, BJ_TEST_MASSIVE };
static uint8_t bjActions[6];
static uint8_t bjActionCount;
static uint8_t bjActionIdx;

static void bjBuildActions() {
  bjActionCount = 0;
  bjActions[bjActionCount++] = BJ_HIT;
  bjActions[bjActionCount++] = BJ_STAND;
  const uint8_t* hand = bjPlayer[bjActiveHand];
  const uint8_t  len  = bjPlayerLen[bjActiveHand];
  if (len == 2 && bjBalance >= bjBet)
    bjActions[bjActionCount++] = BJ_DOUBLE;
  if (!bjSplit && len == 2 && bjRank(hand[0]) == bjRank(hand[1]) && bjBalance >= bjBet)
    bjActions[bjActionCount++] = BJ_SPLIT;
  bjActions[bjActionCount++] = BJ_EXIT;
  bjActionIdx = 0;
}

static const char* bjActionLabel(BjAction a) {
  switch (a) {
    case BJ_HIT:          return "Hit";
    case BJ_STAND:        return "Stand";
    case BJ_DOUBLE:       return "Dbl";
    case BJ_SPLIT:        return "Split";
    case BJ_EXIT:         return "Exit";
    case BJ_TEST_MASSIVE: return "Test11";
    default:              return "?";
  }
}

// ── Result ───────────────────────────────────────────────────
enum BjResult : uint8_t { BJ_R_NONE, BJ_R_WIN, BJ_R_LOSE, BJ_R_PUSH, BJ_R_BLACKJACK };
static BjResult bjResult[2];

static const char* bjResultStr(BjResult r) {
  switch (r) {
    case BJ_R_BLACKJACK: return "Blackjack!";
    case BJ_R_WIN:       return "Win!";
    case BJ_R_LOSE:      return "Lose";
    case BJ_R_PUSH:      return "Push";
    default:             return "";
  }
}

// ── Card drawing ─────────────────────────────────────────────
// Each card: 26px wide, 34px tall
static constexpr int16_t BJ_CW   = 26;
static constexpr int16_t BJ_CH   = 34;
static constexpr int16_t BJ_STEP = 28;  // default step size (guarantees 2px gap)

// Leftmost limits on the screen to prevent cards overlapping text labels
static constexpr int16_t BJ_DEALER_MIN_X = 95;  // Space for "Dealer: XX"
static constexpr int16_t BJ_PLAYER_MIN_X = 75;  // Space for "You: XX" or "H1:XX •"

// Dynamic step logic to contract gaps as hand sizes grow
static int16_t bjGetStep(uint8_t len, int16_t minX) {
  if (len <= 1) return BJ_STEP;
  int16_t maxWidth = (SCREEN_W - 2) - minX;
  int16_t maxStep = (maxWidth - BJ_CW) / (len - 1);
  if (maxStep > BJ_STEP) return BJ_STEP;
  if (maxStep < 4) return 4; // Absolute minimum floor so cards are readable
  return maxStep;
}

static void bjDrawCard(int16_t x, int16_t y, uint8_t card, bool hidden) {
  display.fillRect(x, y, BJ_CW, BJ_CH, GxEPD_WHITE);
  display.drawRect(x, y, BJ_CW, BJ_CH, GxEPD_BLACK);
  
  if (hidden) {
    for (int16_t dy = 3; dy < BJ_CH - 2; dy += 3)
      display.drawFastHLine(x + 2, y + dy, BJ_CW - 4, GxEPD_BLACK);
    return;
  }
  
  char rb[3];
  uint8_t rank = bjRank(card);
  if (rank == 9) {
    snprintf(rb, sizeof(rb), "10");
  } else if (rank < 9) {
    snprintf(rb, sizeof(rb), "%c", "A23456789"[rank]);
  } else {
    snprintf(rb, sizeof(rb), "%c", "JQK"[rank - 10]);
  }

  static const char SUITS[] = "SHDC";
  char sb[2] = { SUITS[card / 13], '\0' };
  
  int16_t rankX = (rank == 9) ? x + 1 : x + 4;
  
  drawText(rankX, y + 14, rb, &FreeMonoBold9pt7b);
  drawText(x + 5, y + 30, sb, &FreeMonoBold9pt7b);
}

static void bjDrawHand(int16_t x, int16_t y,
                       const uint8_t* hand, uint8_t len, bool hideSecond, int16_t step) {
  for (uint8_t i = 0; i < len; i++)
    bjDrawCard(x + i * step, y, hand[i], hideSecond && i == 1);
}

// ── Layout constants (absolute y) ────────────────────────────
static constexpr int16_t BJ_CARDS_X        = 96;               
static constexpr int16_t BJ_DEALER_LABEL_Y = CONTENT_Y + 14;   
static constexpr int16_t BJ_DEALER_CARD_Y  = CONTENT_Y +  0;   
static constexpr int16_t BJ_DIVIDER_Y      = CONTENT_Y + 37;   
static constexpr int16_t BJ_PLAYER_LABEL_Y = CONTENT_Y + 52;   
static constexpr int16_t BJ_PLAYER_CARD_Y  = CONTENT_Y + 40;   
static constexpr int16_t BJ_RESULT_Y       = CONTENT_Y + 77;

// ── Resolve ──────────────────────────────────────────────────
static BjResult bjResolveHand(const uint8_t* hand, uint8_t len) {
  const uint8_t pv  = bjHandValue(hand, len);
  const uint8_t dv  = bjHandValue(bjDealer, bjDealerLen);
  const bool    pBJ = (len == 2 && pv == 21);
  const bool    dBJ = (bjDealerLen == 2 && dv == 21);
  if (pBJ && !dBJ) return BJ_R_BLACKJACK;
  if (pv > 21)     return BJ_R_LOSE;
  if (dv > 21)     return BJ_R_WIN;
  if (pBJ && dBJ)  return BJ_R_PUSH;
  if (pv > dv)     return BJ_R_WIN;
  if (pv < dv)     return BJ_R_LOSE;
  return BJ_R_PUSH;
}

static void bjApplyResult(BjResult r, int16_t bet) {
  switch (r) {
    case BJ_R_BLACKJACK: bjBalance += bet + bet / 2; break;
    case BJ_R_WIN:       bjBalance += bet;           break;
    case BJ_R_LOSE:      bjBalance -= bet;           break;
    default:                                         break;
  }
}

static void bjPlayDealer() {
  while (true) {
    uint8_t v = bjHandValue(bjDealer, bjDealerLen);
    if (v > 17) break;
    if (v == 17 && !bjIsSoft(bjDealer, bjDealerLen)) break;
    bjDealer[bjDealerLen++] = bjDeal();
  }
}

static void bjFinishRound() {
  bool playerHasActiveHand = (bjHandValue(bjPlayer[0], bjPlayerLen[0]) <= 21);
  if (bjSplit && (bjHandValue(bjPlayer[1], bjPlayerLen[1]) <= 21)) {
    playerHasActiveHand = true;
  }

  if (playerHasActiveHand) {
    bjPlayDealer();
  }

  bjResult[0] = bjResolveHand(bjPlayer[0], bjPlayerLen[0]);
  bjApplyResult(bjResult[0], bjBet);
  if (bjSplit) {
    bjResult[1] = bjResolveHand(bjPlayer[1], bjPlayerLen[1]);
    bjApplyResult(bjResult[1], bjBet);
  }
  if (bjBalance <= 0) bjBalance = 50;
  bjPhase     = BJ_RESULT;
  needRefresh = true;
  fastUpdate  = false;
}

static void bjStartHand() {
  bjPlayerLen[0] = bjPlayerLen[1] = 0;
  bjDealerLen    = 0;
  bjSplit        = false;
  bjActiveHand   = 0;
  bjResult[0]    = bjResult[1] = BJ_R_NONE;

  bjPlayer[0][bjPlayerLen[0]++] = bjDeal();
  bjDealer[bjDealerLen++]       = bjDeal();
  bjPlayer[0][bjPlayerLen[0]++] = bjDeal();
  bjDealer[bjDealerLen++]       = bjDeal();

  if (bjHandValue(bjPlayer[0], 2) == 21) {
    bjPlayDealer();
    bjResult[0] = bjResolveHand(bjPlayer[0], bjPlayerLen[0]);
    bjApplyResult(bjResult[0], bjBet);
    if (bjBalance <= 0) bjBalance = 50;
    bjPhase     = BJ_RESULT;
    needRefresh = true;
    fastUpdate  = false;
    return;
  }

  bjBuildActions();
  bjPhase     = BJ_PLAYER;
  needRefresh = true;
  fastUpdate  = false;
}

// ── Enter ─────────────────────────────────────────────────────
void enterBlackjack() {
  bjPhase   = BJ_BETTING;
  bjBetFrac = 0;
  bjBet     = bjBetFromFrac(0);
  needRefresh = true;
  fastUpdate  = false;
  bjShuffle();
}

// ── Draw ─────────────────────────────────────────────────────
void drawBlackjack() {

  // ── Betting screen ────────────────────────────────────────
  if (bjPhase == BJ_BETTING) {
    drawCenteredText(SCREEN_W / 2, CONTENT_Y + 14, "Place your bet", &FreeMonoBold9pt7b);

    char buf[20];
    snprintf(buf, sizeof(buf), "Bal: $%d", bjBalance);
    drawCenteredText(SCREEN_W / 2, CONTENT_Y + 29, buf, &FreeMonoBold9pt7b);

    const char* fracLabel = BJ_FRAC_LABELS[bjBetFrac];
    constexpr int16_t fx = SCREEN_W / 2 - 18, fy = CONTENT_Y + 34, fw = 36, fh = 20;
    display.drawRect(fx, fy, fw, fh, GxEPD_BLACK);
    drawCenteredText(SCREEN_W / 2, fy + 14, fracLabel, &FreeMonoBold9pt7b);
    drawCenteredText(SCREEN_W / 2 - 36, fy + 14, "<", &FreeMonoBold9pt7b);
    drawCenteredText(SCREEN_W / 2 + 36, fy + 14, ">", &FreeMonoBold9pt7b);

    snprintf(buf, sizeof(buf), "Bet: $%d", bjBet);
    drawCenteredText(SCREEN_W / 2, CONTENT_Y + 67, buf, &FreeMonoBold9pt7b);

    constexpr int16_t barX = 8, barY = CONTENT_Y + 73, barW = SCREEN_W - 16, barH = 5;
    display.drawRect(barX, barY, barW, barH, GxEPD_BLACK);
    const int16_t fillW = (int16_t)((bjBetFrac + 1) * barW / 4);
    display.fillRect(barX, barY, fillW, barH, GxEPD_BLACK);

    drawButtonHints("<", "Deal", ">");
    return;
  }

  // ── Game screen (BJ_PLAYER / BJ_RESULT) ──────────────────
  const bool hideHole = (bjPhase == BJ_PLAYER);

  // Dealer row
  {
    char dbuf[16];
    uint8_t shown = hideHole
      ? bjHandValue(bjDealer, 1)
      : bjHandValue(bjDealer, bjDealerLen);
    if (hideHole) snprintf(dbuf, sizeof(dbuf), "Dealer: %d+?", shown);
    else          snprintf(dbuf, sizeof(dbuf), "Dealer: %d",   shown);
    drawText(4, BJ_DEALER_LABEL_Y, dbuf, &FreeMonoBold9pt7b);

    // Compute dynamic step and perfect starting coordinate
    int16_t step = bjGetStep(bjDealerLen, BJ_DEALER_MIN_X);
    int16_t dStartX = (bjDealerLen > 0) ? (SCREEN_W - 2) - ((bjDealerLen - 1) * step + BJ_CW) : (SCREEN_W - 2);
    bjDrawHand(dStartX, BJ_DEALER_CARD_Y, bjDealer, bjDealerLen, hideHole, step);
  }

  // Divider
  display.drawFastHLine(0, BJ_DIVIDER_Y, SCREEN_W, GxEPD_BLACK);

  // Player row
  if (!bjSplit) {
    char pbuf[12];
    snprintf(pbuf, sizeof(pbuf), "You: %d", bjHandValue(bjPlayer[0], bjPlayerLen[0]));
    drawText(4, BJ_PLAYER_LABEL_Y, pbuf, &FreeMonoBold9pt7b);

    // Compute dynamic step and perfect starting coordinate
    int16_t step = bjGetStep(bjPlayerLen[0], BJ_PLAYER_MIN_X);
    int16_t pStartX = (bjPlayerLen[0] > 0) ? (SCREEN_W - 2) - ((bjPlayerLen[0] - 1) * step + BJ_CW) : (SCREEN_W - 2);
    bjDrawHand(pStartX, BJ_PLAYER_CARD_Y, bjPlayer[0], bjPlayerLen[0], false, step);
  } else {
    // Split: Active indicator dot next to H1/H2 labels
    char pb0[12], pb1[12];
    snprintf(pb0, sizeof(pb0), "H1:%d", bjHandValue(bjPlayer[0], bjPlayerLen[0]));
    snprintf(pb1, sizeof(pb1), "H2:%d", bjHandValue(bjPlayer[1], bjPlayerLen[1]));
    
    if (bjActiveHand == 0) {
      display.fillCircle(6, BJ_PLAYER_LABEL_Y - 4, 3, GxEPD_BLACK);
    }
    drawText(16, BJ_PLAYER_LABEL_Y, pb0, &FreeMonoBold9pt7b);
    
    if (bjActiveHand == 1) {
      display.fillCircle(6, BJ_PLAYER_LABEL_Y + 13 - 4, 3, GxEPD_BLACK);
    }
    drawText(16, BJ_PLAYER_LABEL_Y + 13, pb1, &FreeMonoBold9pt7b);
    
    // Compute dynamic step and draw ONLY the active hand
    int16_t step = bjGetStep(bjPlayerLen[bjActiveHand], BJ_PLAYER_MIN_X);
    int16_t pStartX = (bjPlayerLen[bjActiveHand] > 0) ? (SCREEN_W - 2) - ((bjPlayerLen[bjActiveHand] - 1) * step + BJ_CW) : (SCREEN_W - 2);
    bjDrawHand(pStartX, BJ_PLAYER_CARD_Y, bjPlayer[bjActiveHand], bjPlayerLen[bjActiveHand], false, step);
  }

  // ── Result banner ─────────────────────────────────────────
  if (bjPhase == BJ_RESULT) {
    const int16_t banY = BJ_PLAYER_CARD_Y + BJ_CH + 2;         // y=96
    const int16_t banH = DIVIDER_BOT - banY - 1;                // ~11px
    display.fillRect(0, banY, SCREEN_W, banH, GxEPD_WHITE);

    char rbuf[28];
    if (bjSplit) {
      snprintf(rbuf, sizeof(rbuf), "H1:%s H2:%s $%d",
               bjResultStr(bjResult[0]), bjResultStr(bjResult[1]), bjBalance);
    } else {
      snprintf(rbuf, sizeof(rbuf), "%s   Bal:$%d", bjResultStr(bjResult[0]), bjBalance);
    }
    drawText(4, banY + banH - 2, rbuf, &FreeMonoBold9pt7b);
    drawButtonHints("exit", "again", bjSplit ? "H1/H2" : "");
    return;
  }

  // ── Action selector (BJ_PLAYER) ──────────────────────────
  {
    char abuf[20];
    const BjAction cur = (BjAction)bjActions[bjActionIdx];
    snprintf(abuf, sizeof(abuf), "%s $%d", bjActionLabel(cur), bjBet);
    drawButtonHints("<", abuf, ">");
  }
}

// Helper to advance to the next hand or finish the round if appropriate
static void bjAdvanceHand() {
  if (bjSplit && bjActiveHand == 0) {
    bjActiveHand = 1;
    if (bjHandValue(bjPlayer[1], bjPlayerLen[1]) >= 21) {
      bjFinishRound();
    } else {
      bjBuildActions();
      needRefresh = true;
      fastUpdate  = true;
    }
  } else {
    bjFinishRound();
  }
}

// ── Input ─────────────────────────────────────────────────────
void handleBlackjackInput(bool leftPressed, bool midPressed, bool rightPressed) {

  // ── Betting ──────────────────────────────────────────────
  if (bjPhase == BJ_BETTING) {
    if (leftPressed) {
      bjBetFrac = (bjBetFrac == 0) ? 3 : bjBetFrac - 1;
      bjBet     = bjBetFromFrac(bjBetFrac);
      needRefresh = true; fastUpdate = true;
    } else if (rightPressed) {
      bjBetFrac = (bjBetFrac + 1) % 4;
      bjBet     = bjBetFromFrac(bjBetFrac);
      needRefresh = true; fastUpdate = true;
    } else if (midPressed) {
      bjStartHand();
    }
    return;
  }

  // ── Player action ─────────────────────────────────────────
  if (bjPhase == BJ_PLAYER) {
    if (leftPressed) {
      bjActionIdx = (bjActionIdx == 0) ? bjActionCount - 1 : bjActionIdx - 1;
      needRefresh = true; fastUpdate = true;
    } else if (rightPressed) {
      bjActionIdx = (bjActionIdx + 1) % bjActionCount;
      needRefresh = true; fastUpdate = true;
    } else if (midPressed) {
      const BjAction act = (BjAction)bjActions[bjActionIdx];
      uint8_t* hand = bjPlayer[bjActiveHand];
      uint8_t& len  = bjPlayerLen[bjActiveHand];

      switch (act) {
        case BJ_HIT:
          hand[len++] = bjDeal();
          if (bjHandValue(hand, len) >= 21) {
            bjAdvanceHand();
          } else {
            bjBuildActions();
            needRefresh = true; fastUpdate = true;
          }
          break;

        case BJ_STAND:
          bjAdvanceHand();
          break;

        case BJ_DOUBLE:
          bjBalance -= bjBet;
          bjBet     *= 2;
          hand[len++] = bjDeal();
          bjAdvanceHand();
          break;

        case BJ_SPLIT: {
          bjBalance      -= bjBet;
          bjSplit         = true;
          bjPlayer[1][0]  = bjPlayer[0][1];
          bjPlayerLen[1]  = 1;
          bjPlayerLen[0]  = 1;
          bjPlayer[0][bjPlayerLen[0]++] = bjDeal();
          bjPlayer[1][bjPlayerLen[1]++] = bjDeal();
          bjActiveHand = 0;
          
          if (bjHandValue(bjPlayer[0], bjPlayerLen[0]) >= 21) {
            bjAdvanceHand();
          } else {
            bjBuildActions();
            needRefresh = true; fastUpdate = false;
          }
          break;
        }
        
        case BJ_EXIT:
          bjBalance -= bjBet;
          if (bjBalance <= 0) bjBalance = 50;
          bjPhase      = BJ_BETTING;
          bjBetFrac    = 0;
          bjBet        = bjBetFromFrac(0);
          currentState = STATE_GAMES;
          needRefresh  = true; fastUpdate = false;
          break;
      }
    }
    return;
  }

  // ── Result ────────────────────────────────────────────────
  if (bjPhase == BJ_RESULT) {
    if (midPressed) {
      bjPhase   = BJ_BETTING;
      bjBetFrac = 0;
      bjBet     = bjBetFromFrac(0);
      needRefresh = true; fastUpdate = false;
    } else if (leftPressed) {
      currentState = STATE_GAMES;
      needRefresh  = true; fastUpdate = false;
    } else if (rightPressed && bjSplit) {
      bjActiveHand = 1 - bjActiveHand;
      needRefresh  = true; fastUpdate = true;
    }
    return;
  }
}