#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>

struct DiscordConfig {
  String token;
  String applicationId;
  String guildId;
  String channelId;
  bool enabled = false;
};

using DiscordCommandHandler = void (*)(const String &command, const String &interactionId, const String &interactionToken, const String &userId, bool admin);

class DiscordBot {
public:
  void begin(const DiscordConfig &config, DiscordCommandHandler handler);
  void loop();
  bool connected() const { return _connected; }
  bool configured() const { return _cfg.enabled && _cfg.token.length() && _cfg.applicationId.length(); }
  bool sendMessage(const String &content);
  bool respond(const String &interactionId, const String &interactionToken, const String &content, bool ephemeral = false);
  bool registerCommands();

private:
  WebSocketsClient _ws;
  DiscordConfig _cfg;
  DiscordCommandHandler _handler = nullptr;
  bool _connected = false;
  bool _identified = false;
  uint32_t _heartbeatMs = 0;
  uint32_t _lastHeartbeat = 0;
  String _sessionId;
  String _resumeGateway;
  static DiscordBot *_instance;
  static void wsEvent(WStype_t type, uint8_t *payload, size_t length);
  void onText(const String &text);
  void identify();
  void heartbeat();
  void handleInteraction(JsonDocument &doc);
};
