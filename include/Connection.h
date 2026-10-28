#pragma once
#include <esp_wifi.h>
#include <Preferences.h>

// ═══════════════════════════════════════════════════════════════
//  Connection.h
//  WiFi (remembered networks + setup hotspot) and the MQTT connection.
//
//  connectWifi() tries, in order:
//    1. the last network that worked, on its cached channel (no scan)
//    2. every remembered network in range, strongest first
//    3. the setup hotspot, with instructions on screen
// ═══════════════════════════════════════════════════════════════

// ── Remembered networks ───────────────────────────────────────
// Every network that ever worked is kept in flash, most recent first,
// so moving between home / work / a phone hotspot needs no setup.
struct KnownNetwork {
  char ssid[33];
  char pass[65];
};

KnownNetwork knownNets[MAX_KNOWN_NETWORKS];
uint8_t      knownCount = 0;
Preferences  wifiPrefs;

// Last network that worked — survives deep sleep so the usual wake
// (same place as last time) can connect without scanning.
RTC_DATA_ATTR char    cachedSsid[33] = "";
RTC_DATA_ATTR int32_t cachedChannel  = 0;   // 0 = nothing cached
RTC_DATA_ATTR uint8_t cachedBssid[6];

void loadKnownNetworks() {
  wifiPrefs.begin("wifi", true);
  knownCount = min<uint8_t>(wifiPrefs.getUChar("n", 0), MAX_KNOWN_NETWORKS);
  for (uint8_t i = 0; i < knownCount; i++) {
    char key[4];
    snprintf(key, sizeof(key), "s%u", i);
    wifiPrefs.getString(key, knownNets[i].ssid, sizeof(knownNets[i].ssid));
    snprintf(key, sizeof(key), "p%u", i);
    wifiPrefs.getString(key, knownNets[i].pass, sizeof(knownNets[i].pass));
  }
  wifiPrefs.end();

  Serial.printf("Known WiFi: %u\n", knownCount);
  for (uint8_t i = 0; i < knownCount; i++) {
    Serial.printf("  %u: '%s' (pass len %u)\n", i, knownNets[i].ssid, (unsigned)strlen(knownNets[i].pass));
  }
}

void storeKnownNetworks() {
  wifiPrefs.begin("wifi", false);
  wifiPrefs.putUChar("n", knownCount);
  for (uint8_t i = 0; i < knownCount; i++) {
    char key[4];
    snprintf(key, sizeof(key), "s%u", i);
    wifiPrefs.putString(key, knownNets[i].ssid);
    snprintf(key, sizeof(key), "p%u", i);
    wifiPrefs.putString(key, knownNets[i].pass);
  }
  wifiPrefs.end();
}

const KnownNetwork* findKnown(const char* ssid) {
  for (uint8_t i = 0; i < knownCount; i++) {
    if (strcmp(knownNets[i].ssid, ssid) == 0) return &knownNets[i];
  }
  return nullptr;
}

// Moves (or adds) a network to the front of the list. Once the list is
// full the oldest one falls off the end.
void rememberNetwork(const char* ssid, const char* pass) {
  if (ssid[0] == '\0') return;
  int8_t found = -1;
  for (uint8_t i = 0; i < knownCount; i++) {
    if (strcmp(knownNets[i].ssid, ssid) == 0) { found = i; break; }
  }
  if (found == 0 && strcmp(knownNets[0].pass, pass) == 0) return;   // already first, nothing to write

  const uint8_t shiftFrom = (found >= 0) ? found : min<uint8_t>(knownCount, MAX_KNOWN_NETWORKS - 1);
  for (int8_t i = shiftFrom; i > 0; i--) knownNets[i] = knownNets[i - 1];
  if (found < 0 && knownCount < MAX_KNOWN_NETWORKS) knownCount++;

  strlcpy(knownNets[0].ssid, ssid, sizeof(knownNets[0].ssid));
  strlcpy(knownNets[0].pass, pass, sizeof(knownNets[0].pass));
  storeKnownNetworks();
  Serial.printf("Remembered '%s' (pass len %u, %u known)\n",
                knownNets[0].ssid, (unsigned)strlen(knownNets[0].pass), knownCount);
}

// The firmware before the network list relied on WiFiManager's single saved
// network — pick that up so nobody has to go through setup again. Also acts as
// a safety net: whatever the setup hotspot saved into the driver is imported if
// our own list never got it (e.g. a reboot between connect and save).
void importLegacyNetwork() {
  wifi_config_t conf;
  if (esp_wifi_get_config(WIFI_IF_STA, &conf) != ESP_OK || conf.sta.ssid[0] == '\0') return;
  char ssid[33] = {0}, pass[65] = {0};
  memcpy(ssid, conf.sta.ssid,     sizeof(conf.sta.ssid));
  memcpy(pass, conf.sta.password, sizeof(conf.sta.password));
  if (findKnown(ssid)) return;   // our list already has it — never overwrite a known password
  rememberNetwork(ssid, pass);
}


// ── Connecting ────────────────────────────────────────────────
void cacheCurrentAp() {
  // Right after the setup hotspot closes, or after a drop, the station can be
  // in-between: SSID() is then empty and BSSID() is NULL. Never cache that.
  if (WiFi.status() != WL_CONNECTED) { cachedChannel = 0; return; }
  strlcpy(cachedSsid, WiFi.SSID().c_str(), sizeof(cachedSsid));
  if (cachedSsid[0] == '\0') { cachedChannel = 0; return; }
  const uint8_t* bssid = WiFi.BSSID();
  if (bssid) memcpy(cachedBssid, bssid, sizeof(cachedBssid));
  cachedChannel = WiFi.channel();
}

bool waitForWifi(unsigned long timeoutMs) {
  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) delay(20);
  return WiFi.status() == WL_CONNECTED;
}

// Starts connecting to the last network that worked without waiting, so
// the radio can work while the "Connecting..." screen draws.
bool beginLastNetwork() {
  const KnownNetwork* last = (cachedChannel > 0) ? findKnown(cachedSsid) : nullptr;
  if (!last) return false;
  WiFi.begin(last->ssid, last->pass, cachedChannel, cachedBssid);
  return true;
}

// Scans and tries every remembered network in range, strongest first.
bool connectToAnyKnown() {
  if (knownCount == 0) return false;
  WiFi.disconnect();
  const int16_t found = WiFi.scanNetworks();   // the driver returns them strongest first
  for (int16_t i = 0; i < found; i++) {
    const KnownNetwork* net = findKnown(WiFi.SSID(i).c_str());
    if (!net) continue;
    Serial.printf("Trying %s\n", net->ssid);
    WiFi.begin(net->ssid, net->pass, WiFi.channel(i), WiFi.BSSID(i));
    if (waitForWifi(WIFI_CONNECT_TIMEOUT_MS)) {
      WiFi.scanDelete();
      rememberNetwork(net->ssid, net->pass);   // most recently used goes to the front
      return true;
    }
    WiFi.disconnect();
  }
  WiFi.scanDelete();
  return false;
}

// Nothing known in range: open the setup hotspot and say so on screen.
// Any button press cancels it; it also gives up after WIFI_PORTAL_TIMEOUT_S.
// Returns true once the user has picked a network and we're on it.
WiFiManager wm;

bool runWifiPortal() {
  currentState = STATE_PORTAL;
  fastUpdate   = false;
  refreshDisplay();

  wm.setConfigPortalBlocking(false);
  wm.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_S);

  // WiFiManager saves the new credentials through the WiFi driver, so let the
  // driver keep them in flash. With RAM storage the only copy lived in the
  // driver's RAM config, which the AP -> STA switch below can throw away.
  esp_wifi_set_storage(WIFI_STORAGE_FLASH);

  if (sizeof(HOTSPOT_PASSWORD) > 1) wm.startConfigPortal(AP_NAME, AP_password);
  else                              wm.startConfigPortal(AP_NAME);

  const unsigned long start = millis();
  bool armed = !anyButtonDown();   // ignore the press that woke us until it's released
  while (!wm.process()) {
    if (!wm.getConfigPortalActive() || millis() - start > WIFI_PORTAL_TIMEOUT_S * 1000UL) return false;
    const bool down = anyButtonDown();
    if (!armed && !down) armed = true;
    else if (armed && down) { wm.stopConfigPortal(); return false; }
    delay(10);
  }

  // Read the credentials back from the driver's saved config while the portal
  // is still up. WiFi.SSID() only works while we are actually associated and
  // is often already empty during the AP -> STA switch below, which used to
  // make rememberNetwork() return without saving anything.
  String newSsid = wm.getWiFiSSID(true);
  String newPass = wm.getWiFiPass();
  if (newSsid.isEmpty()) newSsid = WiFi.SSID();
  if (newPass.isEmpty()) newPass = WiFi.psk();

  if (!newSsid.isEmpty()) rememberNetwork(newSsid.c_str(), newPass.c_str());
  else Serial.println("Portal connected but the SSID could not be read - nothing saved");

  wm.stopConfigPortal();
  WiFi.mode(WIFI_STA);              // hotspot off
  esp_wifi_set_storage(WIFI_STORAGE_RAM);

  // The mode switch can drop the link for a moment — give the driver a chance
  // to come back before caching an AP. connectWifi() keeps reporting success
  // either way, since the credentials are saved for the next attempt.
  if (waitForWifi(WIFI_CONNECT_TIMEOUT_MS)) cacheCurrentAp();
  else                                      cachedChannel = 0;
  return true;
}

// Only touches the e-ink if the quick reconnect doesn't land in ~1 s.
bool connectWifi() {
  WiFi.mode(WIFI_STA);
  importLegacyNetwork();
  // Our list is the source of truth — stop the driver rewriting its own
  // flash copy (channel/BSSID pin included) on every connect.
  esp_wifi_set_storage(WIFI_STORAGE_RAM);

  const bool quickStarted = beginLastNetwork();
  bool ok = quickStarted && waitForWifi(1200);

  if (!ok) {
    currentState = STATE_WIFI;
    fastUpdate   = false;
    refreshDisplay();   // "Connecting..." — the radio keeps trying meanwhile

    ok = (quickStarted && waitForWifi(WIFI_FAST_CONNECT_MS)) || connectToAnyKnown() || runWifiPortal();
  }

  if (ok) cacheCurrentAp();
  else    cachedChannel = 0;
  return ok;
}


// ── MQTT ──────────────────────────────────────────────────────
void setupMqtt() {
  wifiClient.setInsecure();
  wifiClient.setTimeout(WIFI_CONNECT_TIMEOUT_MS);
  // The library default is 120 s, and loop() is stuck for all of it —
  // no buttons, no redraws. A healthy handshake takes 1-3 s.
  wifiClient.setHandshakeTimeout(MQTT_HANDSHAKE_TIMEOUT_S);
  mqttClient.setServer(MQTT_SERVER_ADDR, MQTT_SERVER_PORT);
  mqttClient.setSocketTimeout(WIFI_CONNECT_TIMEOUT_MS / 1000);
  mqttClient.setCallback(onMessageReceived);
}

bool connectMQTT() {
  static char inboxTopic[24] = "";
  static char gamesTopic[24] = "";
  if (inboxTopic[0] == '\0') {
    snprintf(inboxTopic, sizeof(inboxTopic), "%s/inbox", BEEPER_ID);
    snprintf(gamesTopic, sizeof(gamesTopic), "%s/games", BEEPER_ID);
  }

  // Radio fully awake for the handshake — in max modem sleep replies can
  // arrive late or get missed, and the handshake stalls.
  WiFi.setSleep(WIFI_PS_NONE);

  Serial.printf("MQTT connect (RSSI %d dBm)... ", WiFi.RSSI());
  const unsigned long start = millis();
  const bool ok = mqttClient.connect(BEEPER_ID, MQTT_USERNAME_STR, MQTT_PASSWORD_STR);
  const unsigned long took = millis() - start;

  WiFi.setSleep(WIFI_PS_MAX_MODEM);

  if (!ok) {
    char err[64];
    wifiClient.lastError(err, sizeof(err));
    Serial.printf("failed rc=%d after %lu ms (tls: %s)\n", mqttClient.state(), took, err);
    return false;
  }
  Serial.printf("OK in %lu ms\n", took);
  mqttClient.subscribe(inboxTopic);   // the retained message (if any) arrives right after this
  mqttClient.subscribe(gamesTopic);
  return true;
}
