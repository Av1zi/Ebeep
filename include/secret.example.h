#pragma once

// ── Ebeep secrets template ─────────────────────────────────────
// 1. Copy this file to `secret.h` (same folder):
//      copy include\secret.example.h include\secret.h
//    `secret.h` is gitignored on purpose so your passwords never get committed.
// 2. Fill in your own values below.
//
// You can use any MQTT broker. Create two users or one shared
// password - the firmware uses BEEPER_ID ("Beeper_1" / "Beeper_2" from
// config.h DEVICE_NUM) as the MQTT username, so you only need to set the
// password here.

// WiFi config-portal hotspot password.
// Leave as "" for an open setup hotspot, or set something like "ebeep123".
#define HOTSPOT_PASSWORD ""

// MQTT broker address (just the hostname)
#define MQTT_SERVER     "YOUR_BROKER_HOSTNAME"
// MQTT broker port
#define MQTT_PORT       8883

// MQTT password shared by both devices (username is BEEPER_ID automatically).
#define MQTT_PASSWORD   "YOUR_MQTT_PASSWORD"
