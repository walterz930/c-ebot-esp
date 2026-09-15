#pragma once
#include <Arduino.h>
#include <Preferences.h>

struct SyncConfig {
  String chasterToken;
  String chasterKeyholderToken;
  String chasterLockId;
  String emlaUserId;
  String emlaApiKey;
  String emlaKeyholderApiKey;
  uint32_t intervalSeconds = 30;
};

struct SyncState {
  int64_t chasterSeconds = -1;
  int64_t emlalockSeconds = -1;
  int64_t targetSeconds = -1;
  String status = "WAITING";
  String message = "Configure credentials to begin";
  String lastError;
  String lastAction;
  uint32_t lastCheckMillis = 0;
  bool paused = false;
};

class SyncEngine {
public:
  void begin(const SyncConfig &config);
  void updateConfig(const SyncConfig &config);
  void loop();
  bool syncNow();
  bool manualDelta(int32_t seconds, const String &actor);
  void pause(const String &reason);
  void resume();
  const SyncState &state() const { return _state; }
  String historyJson() const;
  String logsJson() const;

private:
  SyncConfig _cfg;
  SyncState _state;
  Preferences _prefs;
  bool _prefsOpen = false;
  uint32_t _lastAttempt = 0;
  String _history = "[]";
  String _logs = "[]";

  bool configured() const;
  bool readTimers(int64_t &chaster, int64_t &emla, String &error);
  bool chasterDelta(int32_t seconds, String &error);
  bool emlaDelta(int32_t seconds, String &error);
  bool parseChasterRemaining(const String &json, int64_t &seconds);
  bool parseEmlaRemaining(const String &json, int64_t &seconds);
  void loadHistory();
  void saveHistory();
  void addHistory(const String &action, const String &detail);
  void addLog(const String &level, const String &message);
};
