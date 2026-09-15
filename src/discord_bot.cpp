#include "discord_bot.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

DiscordBot *DiscordBot::_instance = nullptr;

static String jsonEscape(const String &s) {
  String out; serializeJson(s, out); return out.substring(1, out.length() - 1);
}

void DiscordBot::begin(const DiscordConfig &config, DiscordCommandHandler handler) {
  _cfg = config; _handler = handler; _instance = this;
  _connected = false; _identified = false;
  if (!configured()) return;
  _ws.onEvent(DiscordBot::wsEvent);
  _ws.setReconnectInterval(5000);
  _ws.beginSSL("gateway.discord.gg", 443, "/?v=10&encoding=json");
}

void DiscordBot::loop() {
  if (!configured()) return;
  _ws.loop();
  if (_identified && _heartbeatMs && millis() - _lastHeartbeat >= _heartbeatMs) heartbeat();
}

void DiscordBot::wsEvent(WStype_t type, uint8_t *payload, size_t length) {
  if (!_instance) return;
  if (type == WStype_CONNECTED) { _instance->_connected = true; return; }
  if (type == WStype_DISCONNECTED) { _instance->_connected = false; _instance->_identified = false; return; }
  if (type == WStype_TEXT) _instance->onText(String((char *)payload).substring(0, length));
}

void DiscordBot::onText(const String &text) {
  JsonDocument doc;
  if (deserializeJson(doc, text)) return;
  int op = doc["op"] | -1;
  if (op == 10) {
    _heartbeatMs = (doc["d"]["heartbeat_interval"] | 45000U);
    _lastHeartbeat = millis(); identify(); return;
  }
  if (op == 11) { _lastHeartbeat = millis(); return; }
  if (op != 0) return;
  String event = doc["t"] | "";
  if (event == "READY") {
    _identified = true; _sessionId = doc["d"]["session_id"] | "";
    _resumeGateway = doc["d"]["resume_gateway_url"] | "";
  } else if (event == "INTERACTION_CREATE") {
    handleInteraction(doc);
  }
}

void DiscordBot::identify() {
  JsonDocument d;
  d["op"] = 2;
  JsonObject data = d["d"].to<JsonObject>();
  data["token"] = _cfg.token;
  data["intents"] = 1; // GUILDS
  JsonObject props = data["properties"].to<JsonObject>();
  props["os"] = "linux"; props["browser"] = "c-ebot-esp"; props["device"] = "c-ebot-esp";
  String out; serializeJson(d, out); _ws.sendTXT(out);
}

void DiscordBot::heartbeat() {
  JsonDocument d; d["op"] = 1; d["d"] = nullptr;
  String out; serializeJson(d, out); _ws.sendTXT(out); _lastHeartbeat = millis();
}

void DiscordBot::handleInteraction(JsonDocument &doc) {
  String command = doc["d"]["data"]["name"] | "";
  String iid = doc["d"]["id"] | "";
  String token = doc["d"]["token"] | "";
  String userId = doc["d"]["member"]["user"]["id"] | doc["d"]["user"]["id"] | "";
  bool admin = false; // Permission checks are enforced by the server configuration/UI in this test build.
  if (_handler) _handler(command, iid, token, userId, admin);
}

bool DiscordBot::respond(const String &interactionId, const String &interactionToken, const String &content, bool ephemeral) {
  if (!configured() || !interactionId.length() || !interactionToken.length()) return false;
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http;
  String url = "https://discord.com/api/v10/interactions/" + interactionId + "/" + interactionToken + "/callback";
  if (!http.begin(client, url)) return false;
  http.addHeader("Content-Type", "application/json");
  JsonDocument d; d["type"] = 4; JsonObject data = d["data"].to<JsonObject>(); data["content"] = content;
  if (ephemeral) data["flags"] = 64;
  String body; serializeJson(d, body); int code = http.POST(body); http.end(); return code >= 200 && code < 300;
}

bool DiscordBot::sendMessage(const String &content) {
  if (!configured() || !_cfg.channelId.length()) return false;
  WiFiClientSecure client; client.setInsecure(); HTTPClient http;
  String url = "https://discord.com/api/v10/channels/" + _cfg.channelId + "/messages";
  if (!http.begin(client, url)) return false;
  http.addHeader("Authorization", "Bot " + _cfg.token); http.addHeader("Content-Type", "application/json");
  JsonDocument d; d["content"] = content; String body; serializeJson(d, body);
  int code = http.POST(body); http.end(); return code >= 200 && code < 300;
}

bool DiscordBot::registerCommands() {
  if (!configured() || !_cfg.guildId.length()) return false;
  WiFiClientSecure client; client.setInsecure(); HTTPClient http;
  String url = "https://discord.com/api/v10/applications/" + _cfg.applicationId + "/guilds/" + _cfg.guildId + "/commands";
  if (!http.begin(client, url)) return false;
  http.addHeader("Authorization", "Bot " + _cfg.token); http.addHeader("Content-Type", "application/json");
  JsonDocument d; JsonArray a = d.to<JsonArray>();
  const char *names[] = {"status","sync","pause","resume","emergency","health","testdiscord","testchaster","testemlalock","history","logs","nextsync","version","panel"};
  const char *desc[] = {"Show c-ebot status","Run a synchronization check","Pause automatic synchronization","Resume automatic synchronization","Emergency pause","Check service health","Send a Discord test message","Test Chaster connection","Test EmlaLock connection","Show recent activity","Show recent logs","Show next sync time","Show firmware version","Open the control panel"};
  for (uint8_t i=0;i<sizeof(names)/sizeof(names[0]);++i) { JsonObject c=a.add<JsonObject>(); c["name"]=names[i]; c["description"]=desc[i]; }
  String body; serializeJson(d, body); int code=http.PUT(body); http.end(); return code>=200 && code<300;
}
