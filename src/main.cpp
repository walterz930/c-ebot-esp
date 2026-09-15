#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include "discord_bot.h"

WebServer server(80);
Preferences prefs;
DiscordBot discord;

struct Config {
  String chasterToken, chasterKeyholderToken, chasterLockId;
  String emlaUserId, emlaApiKey, emlaKeyholderApiKey;
  String discordToken, discordApplicationId, discordGuildId, discordChannelId, discordAdminUserId;
  bool discordEnabled=false, autoSync=true;
  uint32_t intervalSeconds=30;
};
Config cfg;
String state="WAITING", message="Configure credentials to begin", lastError;
int64_t chasterSeconds=-1, emlaSeconds=-1;
uint32_t lastSync=0, lastAttempt=0;

void syncOnce();
void discordCommand(const String&,const String&,const String&,const String&,bool);

DiscordConfig makeDiscordConfig(){
  DiscordConfig dc;
  dc.token = cfg.discordToken;
  dc.applicationId = cfg.discordApplicationId;
  dc.guildId = cfg.discordGuildId;
  dc.channelId = cfg.discordChannelId;
  dc.enabled = true;
  return dc;
}

void loadConfig(){
  prefs.begin("cebot",true);
  cfg.chasterToken=prefs.getString("ct","");cfg.chasterKeyholderToken=prefs.getString("ckt","");cfg.chasterLockId=prefs.getString("clid","");
  cfg.emlaUserId=prefs.getString("euid","");cfg.emlaApiKey=prefs.getString("eak","");cfg.emlaKeyholderApiKey=prefs.getString("ekak","");
  cfg.discordToken=prefs.getString("dt","");cfg.discordApplicationId=prefs.getString("daid","");cfg.discordGuildId=prefs.getString("dgid","");
  cfg.discordChannelId=prefs.getString("dcid","");cfg.discordAdminUserId=prefs.getString("duid","");cfg.discordEnabled=prefs.getBool("den",false);
  cfg.autoSync=prefs.getBool("auto",true);cfg.intervalSeconds=prefs.getUInt("int",30);prefs.end();
}
void saveConfig(JsonDocument &doc){
  prefs.begin("cebot",false);
  if(doc["chasterToken"].is<const char*>())prefs.putString("ct",doc["chasterToken"].as<String>());
  if(doc["chasterKeyholderToken"].is<const char*>())prefs.putString("ckt",doc["chasterKeyholderToken"].as<String>());
  if(doc["chasterLockId"].is<const char*>())prefs.putString("clid",doc["chasterLockId"].as<String>());
  if(doc["emlaUserId"].is<const char*>())prefs.putString("euid",doc["emlaUserId"].as<String>());
  if(doc["emlaApiKey"].is<const char*>())prefs.putString("eak",doc["emlaApiKey"].as<String>());
  if(doc["emlaKeyholderApiKey"].is<const char*>())prefs.putString("ekak",doc["emlaKeyholderApiKey"].as<String>());
  if(doc["discordToken"].is<const char*>())prefs.putString("dt",doc["discordToken"].as<String>());
  if(doc["discordApplicationId"].is<const char*>())prefs.putString("daid",doc["discordApplicationId"].as<String>());
  if(doc["discordGuildId"].is<const char*>())prefs.putString("dgid",doc["discordGuildId"].as<String>());
  if(doc["discordChannelId"].is<const char*>())prefs.putString("dcid",doc["discordChannelId"].as<String>());
  if(doc["discordAdminUserId"].is<const char*>())prefs.putString("duid",doc["discordAdminUserId"].as<String>());
  if(doc["discordEnabled"].is<bool>())prefs.putBool("den",doc["discordEnabled"].as<bool>());
  if(doc["autoSync"].is<bool>())prefs.putBool("auto",doc["autoSync"].as<bool>());
  if(doc["intervalSeconds"].is<uint32_t>())prefs.putUInt("int",max(10U,doc["intervalSeconds"].as<uint32_t>()));prefs.end();loadConfig();
}
void sendJson(JsonDocument &doc,int code=200){String out;serializeJson(doc,out);server.send(code,"application/json",out);}
bool configured(){return cfg.chasterLockId.length()&&cfg.chasterToken.length()&&cfg.emlaUserId.length()&&cfg.emlaApiKey.length();}
void handleStatus(){JsonDocument d;d["state"]=state;d["message"]=message;d["autoSync"]=cfg.autoSync;d["configured"]=configured();d["chasterSeconds"]=chasterSeconds;d["emlalockSeconds"]=emlaSeconds;d["lastSyncMillis"]=lastSync;d["lastError"]=lastError;d["rssi"]=WiFi.RSSI();d["discordConfigured"]=cfg.discordEnabled&&cfg.discordToken.length()&&cfg.discordApplicationId.length();d["discordConnected"]=discord.connected();sendJson(d);}
void handleConfigGet(){JsonDocument d;d["chasterLockId"]=cfg.chasterLockId;d["emlaUserId"]=cfg.emlaUserId;d["autoSync"]=cfg.autoSync;d["intervalSeconds"]=cfg.intervalSeconds;d["hasChasterToken"]=cfg.chasterToken.length()>0;d["hasChasterKeyholderToken"]=cfg.chasterKeyholderToken.length()>0;d["hasEmlaApiKey"]=cfg.emlaApiKey.length()>0;d["hasEmlaKeyholderApiKey"]=cfg.emlaKeyholderApiKey.length()>0;d["discordEnabled"]=cfg.discordEnabled;d["discordApplicationId"]=cfg.discordApplicationId;d["discordGuildId"]=cfg.discordGuildId;d["discordChannelId"]=cfg.discordChannelId;d["discordAdminUserId"]=cfg.discordAdminUserId;d["hasDiscordToken"]=cfg.discordToken.length()>0;d["discordConnected"]=discord.connected();sendJson(d);}
void handleConfigPost(){JsonDocument d;if(deserializeJson(d,server.arg("plain"))){JsonDocument e;e["error"]="Invalid JSON";sendJson(e,400);return;}saveConfig(d);message="Configuration saved";JsonDocument o;o["ok"]=true;o["message"]="Configuration saved";sendJson(o);if(cfg.discordEnabled&&cfg.discordToken.length()&&cfg.discordApplicationId.length()){DiscordConfig dc=makeDiscordConfig();discord.begin(dc,discordCommand);discord.registerCommands();}}
void setPaused(const String &why){cfg.autoSync=false;prefs.begin("cebot",false);prefs.putBool("auto",false);prefs.end();state="PAUSED";message=why;}
void handlePause(){setPaused("Automatic synchronization paused");handleStatus();}
void handleResume(){cfg.autoSync=true;prefs.begin("cebot",false);prefs.putBool("auto",true);prefs.end();state="WAITING";message="Automatic synchronization enabled";handleStatus();}
String formatTime(int64_t s){if(s<0)return "unknown";uint64_t v=(uint64_t)s,d=v/86400;v%=86400;uint64_t h=v/3600;v%=3600;uint64_t m=v/60,sec=v%60;char b[64];snprintf(b,sizeof(b),"%llud %02llu:%02llu:%02llu",d,h,m,sec);return String(b);}
void discordCommand(const String &command,const String &iid,const String &token,const String &userId,bool gatewayAdmin){
  bool admin=gatewayAdmin||(cfg.discordAdminUserId.length()&&userId==cfg.discordAdminUserId);
  if((command=="pause"||command=="resume"||command=="sync"||command=="emergency")&&!admin){discord.respond(iid,token,"You are not authorized to control c-ebot.",true);return;}
  String reply;
  if(command=="status")reply="c-ebot: "+state+"\nChaster: "+formatTime(chasterSeconds)+"\nEmlaLock: "+formatTime(emlaSeconds)+"\nAuto Sync: "+String(cfg.autoSync?"enabled":"paused")+"\nDiscord: "+String(discord.connected()?"connected":"disconnected");
  else if(command=="pause"||command=="emergency"){setPaused("Paused from Discord");reply="Automatic synchronization is now paused.";}
  else if(command=="resume"){handleResume();reply="Automatic synchronization has been resumed.";}
  else if(command=="sync"){syncOnce();reply="Synchronization check requested. Live timer writes remain disabled in this test firmware.";}
  else if(command=="health")reply="ESP32 Wi-Fi: OK\nDiscord Gateway: "+String(discord.connected()?"connected":"disconnected")+"\nTimer write engine: test-only/disabled";
  else if(command=="testdiscord"){discord.respond(iid,token,"Discord bot test received.",true);discord.sendMessage("🧪 c-ebot ESP32 Discord test message.");return;}
  else if(command=="testchaster")reply="Chaster test is not yet enabled; API read client is the next integration stage.";
  else if(command=="testemlalock")reply="EmlaLock test is not yet enabled; API read client is the next integration stage.";
  else if(command=="history"||command=="logs")reply="ESP32 history endpoint is reserved for the next firmware stage.";
  else if(command=="nextsync")reply="Next automatic check: "+String(cfg.intervalSeconds)+" seconds after the previous check.";
  else if(command=="version")reply="c-ebot-esp test firmware with Discord Gateway support.";
  else if(command=="panel")reply="Use the ESP32 web dashboard for configuration and timer controls.";
  else reply="Unknown command.";
  discord.respond(iid,token,reply,true);
}
void syncOnce(){if(!configured()){state="WAITING";message="Configure Chaster and EmlaLock credentials to begin";return;}state="TEST_ONLY";message="Live Chaster/EmlaLock timer writes are disabled until their on-device API responses are validated";lastError="No timer mutation performed";}
void handleSync(){syncOnce();handleStatus();}
void setupRoutes(){
  server.on("/api/status",HTTP_GET,handleStatus);server.on("/api/config",HTTP_GET,handleConfigGet);server.on("/api/config",HTTP_POST,handleConfigPost);server.on("/api/sync",HTTP_POST,handleSync);server.on("/api/pause",HTTP_POST,handlePause);server.on("/api/resume",HTTP_POST,handleResume);
  server.on("/api/discord/test",HTTP_POST,[](){JsonDocument d;bool ok=discord.sendMessage("🧪 c-ebot ESP32 Discord test message.");d["ok"]=ok;d["connected"]=discord.connected();sendJson(d,ok?200:503);});
  server.onNotFound([](){String p=server.uri(),t="text/plain";if(p=="/"||p=="/index.html"){p="/index.html";t="text/html";}else if(p=="/app.js")t="application/javascript";else if(p=="/style.css")t="text/css";else{server.send(404,"text/plain","Not found");return;}File f=LittleFS.open(p,"r");if(!f){server.send(404,"text/plain","File not found");return;}server.streamFile(f,t);f.close();});
}
void setup(){Serial.begin(115200);loadConfig();if(!LittleFS.begin(true))Serial.println("LittleFS mount failed");WiFiManager wm;wm.setConfigPortalTimeout(180);if(!wm.autoConnect("C-EBOT-SETUP"))ESP.restart();Serial.print("C-EBOT IP: ");Serial.println(WiFi.localIP());setupRoutes();server.begin();message="Connected; dashboard ready";if(cfg.discordEnabled&&cfg.discordToken.length()&&cfg.discordApplicationId.length()){DiscordConfig dc=makeDiscordConfig();discord.begin(dc,discordCommand);delay(500);discord.registerCommands();}}
void loop(){server.handleClient();discord.loop();if(cfg.autoSync&&cfg.intervalSeconds>0&&millis()-lastAttempt>=cfg.intervalSeconds*1000UL){lastAttempt=millis();syncOnce();}delay(2);}