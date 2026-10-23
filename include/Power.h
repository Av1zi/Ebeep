#pragma once
#include <esp_sleep.h>

// ═══════════════════════════════════════════════════════════════
//  Power.h
//  Battery reading and deep sleep.
//
//  The device is deep asleep unless someone is using it:
//    - A press on BTN_LEFT / BTN_SELECT wakes it, and it connects.
//    - It stays awake while you're typing or in a game, and otherwise
//      for ACTIVE_TIMEOUT_MS after the last press or message.
//    - Then it shows "Sleeping" and powers down. There's no timer
//      wake, so asleep it draws only the chip's deep-sleep current.
// ═══════════════════════════════════════════════════════════════

// ── Battery ───────────────────────────────────────────────────
// Smoothed pack voltage in mV. Kept across deep sleep so the % doesn't
// jump around from one wake to the next.
RTC_DATA_ATTR static float battVoltageEma = 0;

void updateBattery() {
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogReadMilliVolts(A0);
  const float sample = (sum / 8.0f) * BATTERY_DIVIDER_RATIO;

  battVoltageEma = (battVoltageEma == 0) ? sample
                                         : battVoltageEma + BATTERY_EMA_ALPHA * (sample - battVoltageEma);

  batteryPct = (int)((battVoltageEma - BATTERY_MIN_MV) * 100.0f / (BATTERY_MAX_MV - BATTERY_MIN_MV));
  batteryPct = constrain(batteryPct, 0, 100);
}


// ── When not to sleep ─────────────────────────────────────────
// Sleeping here would throw away a draft or a game in progress.
bool userIsBusy() {
  return currentState == STATE_COMPOSE   || messageLen > 0 ||
         currentState == STATE_TICTACTOE || inTicTacToe || TTT_pendingStart ||
         currentState == STATE_CONNECT4  || inConnect4  || C4_pendingStart  ||
         currentState == STATE_BLACKJACK;
}


// ── Deep sleep ────────────────────────────────────────────────
// Never returns. Shows the "Sleeping" screen (unless drawScreen is false),
// then sleeps until BTN_LEFT / BTN_SELECT is pressed.
void enterDeepSleep(bool drawScreen = true) {
  if (mqttClient.connected()) mqttClient.disconnect();   // flushes anything just published
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  if (drawScreen) {
    netWifiOk = netMqttOk = false;
    currentState = STATE_SLEEP;
    fastUpdate   = false;
    initDisplay();
    refreshDisplay();
  }

  // Put the panel's controller into its own deep sleep too. Without this its
  // booster can stay powered after a partial refresh, all night long.
  if (displayAwake) display.hibernate();

  // Don't go down while a button is still held — it would wake us instantly.
  const unsigned long heldSince = millis();
  while (anyButtonDown() && millis() - heldSince < 3000) delay(10);

  // A button still reading pressed (stuck / shorted switch) would wake us
  // on the spot, forever — leave it out and rely on the other one.
  uint64_t wakeMask = 0;
  if (digitalRead(BTN_LEFT)   == HIGH) wakeMask |= 1ULL << BTN_LEFT;
  if (digitalRead(BTN_SELECT) == HIGH) wakeMask |= 1ULL << BTN_SELECT;

  if (wakeMask) {
    // GPIO wakeup rather than ext1: on this board ext1 pulled the chip back
    // out of deep sleep ~70 ms later, with no wake cause, every single time.
    // GPIO wakeup also sets up the pins' pull-ups for sleep by itself.
    esp_deep_sleep_enable_gpio_wakeup(wakeMask, ESP_GPIO_WAKEUP_GPIO_LOW);
  } else {
    // Both read pressed — check again later rather than never waking.
    // (The sleep timer runs slow on this board, so this is a rough interval.)
    esp_sleep_enable_timer_wakeup(STUCK_BUTTON_RETRY_S * 1000000ULL);
  }

  Serial.printf("Sleeping (wake mask 0x%llx)\n", wakeMask);
  Serial.flush();
  esp_deep_sleep_start();
}

// Called every loop() tick: sleeps once the device has been left alone.
void handlePowerState() {
  const unsigned long idle = millis() - lastActivityMillis;
  if (idle < ACTIVE_TIMEOUT_MS) return;

  if (userIsBusy()) {
    if (idle < BUSY_IDLE_TIMEOUT_MS) return;
    // Mid-game or mid-draft but untouched for ages (an opponent's move counts
    // as activity too) — give it up rather than keep WiFi on all night.
    if (inTicTacToe || TTT_pendingStart) leaveTicTacToe();
    if (inConnect4  || C4_pendingStart)  leaveConnect4();
  }
  enterDeepSleep();
}
