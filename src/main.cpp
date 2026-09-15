#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>

WebServer server(80);
Preferences prefs;

struct Config {
  String chasterToken;
  String chasterKeyholderToken;
  String chasterLockId;
  String emlaUserId;
  String emlaApiKey;
  String emlaKeyholderApiKey;
  bool autoSync = true;
  uint32_t intervalSeconds = 30;
};

Config cfg;
String state = "WAITING";
String message = "Configure credentials to begin";
String lastError;
int64_t chasterSeconds = -1;
int64_t emlaSeconds = -1;
uint32_t lastSync = 0;
uint32_t lastAttempt = 0;

void loadConfig() {
  prefs.begin("cebot", true);
  cfg.chasterToken = prefs.getString("ct", "");
  cfg.chasterKeyholderToken = prefs.getString("ckt", "");
  cfg.chasterLockId = prefs.getString("clid", "");
  cfg.emlaUserId = prefs.getString("euid", "");
  cfg.emlaApiKey = prefs.getString("eak", "");
  cfg.emlaKeyholderApiKey = prefs.getString("ekak", "");
  cfg.autoSync = prefs.getBool("auto", true);
  cfg.intervalSeconds = prefs.getUInt("int", 30);
  prefs.end();
}

void saveConfig(JsonDocument &doc) {
  prefs.begin("cebot", false);
  if (doc["chasterToken"].is<const char*>()) prefs.putString("ct", doc["chasterToken"].as<String>());
  if (doc["chasterKeyholderToken"].is<const char*>()) prefs.putString("ckt", doc["chasterKeyholderToken"].as<String>());
  if (doc["chasterLockId"].is<const char*>()) prefs.putString("clid", doc["chasterLockId"].as<String>());
  if (doc["emlaUserId"].is<const char*>()) prefs.putString("euid", doc["emlaUserId"].as<String>());
  if (doc["emlaApiKey"].is<const char*>()) prefs.putString("eak", doc["emlaApiKey"].as<String>());
  if (doc["emlaKeyholderApiKey"].is<const char*>()) prefs.putString("ekak", doc["emlaKeyholderApiKey"].as<String>());
  if (doc["autoSync"].is<bool>()) prefs.putBool("auto", doc["autoSync"].as<bool>());
  if (doc["intervalSeconds"].is<uint32_t>()) prefs.putUInt("int", max(10U, doc["intervalSeconds"].as<uint32_t>()));
  prefs.end();
  loadConfig();
}

void sendJson(JsonDocument &doc, int code = 200) {
  String out; serializeJson(doc, out); server.send(code, "application/json", out);
}

void handleStatus() {
  JsonDocument doc;
  doc["state"] = state;
  doc["message"] = message;
  doc["autoSync"] = cfg.autoSync;
  doc["configured"] = cfg.chasterToken.length() && cfg.chasterLockId.length() && cfg.emlaApiKey.length();
  doc["chasterSeconds"] = chasterSeconds;
  doc["emlalockSeconds"] = emlaSeconds;
  doc["lastSyncMillis"] = lastSync;
  doc["lastError"] = lastError;
  doc["rssi"] = WiFi.RSSI();
  sendJson(doc);
}

void handleConfigGet() {
  JsonDocument doc;
  doc["chasterLockId"] = cfg.chasterLockId;
  doc["emlaUserId"] = cfg.emlaUserId;
  doc["autoSync"] = cfg.autoSync;
  doc["intervalSeconds"] = cfg.intervalSeconds;
  doc["hasChasterToken"] = cfg.chasterToken.length() > 0;
  doc["hasEmlaApiKey"] = cfg.emlaApiKey.length() > 0;
  sendJson(doc);
}

void handleConfigPost() {
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) { sendJson(doc, 400); return; }
  saveConfig(doc);
  message = "Configuration saved";
  sendJson(doc);
}

void handlePause() { cfg.autoSync = false; prefs.begin("cebot", false); prefs.putBool("auto", false); prefs.end(); state="PAUSED"; message="Automatic synchronization paused"; handleStatus(); }
void handleResume() { cfg.autoSync = true; prefs.begin("cebot", false); prefs.putBool("auto", true); prefs.end(); state="WAITING"; message="Automatic synchronization enabled"; handleStatus(); }

// API porting point: implement the exact Chaster and EmlaLock requests here.
// The original project uses:
// Chaster GET https://api.chaster.app/locks/{id}
// Chaster POST https://api.chaster.app/locks/{id}/update-time {"duration": delta}
// EmlaLock GET https://api.emlalock.com/info?userid=...&apikey=...
// EmlaLock GET /add or /sub with value and authentication parameters.
// These calls remain disabled until response schemas and permissions are tested on-device.
void syncOnce() {
  if (!cfg.chasterToken.length() || !cfg.chasterLockId.length() || !cfg.emlaApiKey.length()) {
    state = "WAITING"; message = "Configure credentials to begin"; return;
  }
  state = "NOT_IMPLEMENTED";
  message = "API client validation is required before live changes are enabled";
  lastError = "Live synchronization is intentionally disabled in this initial firmware build";
}

void handleSync() { syncOnce(); handleStatus(); }

void setupRoutes() {
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/config", HTTP_GET, handleConfigGet);
  server.on("/api/config", HTTP_POST, handleConfigPost);
  server.on("/api/sync", HTTP_POST, handleSync);
  server.on("/api/pause", HTTP_POST, handlePause);
  server.on("/api/resume", HTTP_POST, handleResume);
  server.onNotFound([](){
    if (server.uri() == "/" || server.uri() == "/index.html") { File f=LittleFS.open("/index.html", "r"); server.streamFile(f, "text/html"); f.close(); return; }
    if (server.uri() == "/app.js") { File f=LittleFS.open("/app.js", "r"); server.streamFile(f, "application/javascript"); f.close(); return; }
    if (server.uri() == "/style.css") { File f=LittleFS.open("/style.css", "r"); server.streamFile(f, "text/css"); f.close(); return; }
    server.send(404, "text/plain", "Not found");
  });
}

void setup() {
  Serial.begin(115200);
  loadConfig();
  if (!LittleFS.begin(true)) Serial.println("LittleFS mount failed");

  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  if (!wm.autoConnect("C-EBOT-SETUP")) {
    ESP.restart();
  }
  Serial.print("C-EBOT IP: "); Serial.println(WiFi.localIP());
  setupRoutes();
  server.begin();
  message = "Connected; dashboard ready";
}

void loop() {
  server.handleClient();
  if (cfg.autoSync && cfg.intervalSeconds > 0 && millis() - lastAttempt >= cfg.intervalSeconds * 1000UL) {
    lastAttempt = millis();
    syncOnce();
  }
  delay(2);
}
