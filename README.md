# C-EBOT ESP32

ESP32 DevKit port of c-ebot. The ESP32 is the complete standalone controller: Wi-Fi, web dashboard, local storage, Chaster/EmlaLock sync engine, and Discord Gateway bot all run on the device. A PC or phone is only used as a UI.

## Current ESP32 build

### Included
- PlatformIO `esp32dev` project
- Wi-Fi provisioning with `C-EBOT-SETUP`
- LAN-hosted LittleFS dashboard
- NVS-backed credential/configuration storage
- Chaster and EmlaLock API reads
- Highest-time-wins synchronization
- Automatic extension of the lower timer only
- Post-change timer verification
- Automatic pause on failed synchronization/verification
- Manual add/subtract controls for both timers
- Local history and logs retained on the ESP32
- Real Discord Gateway client over WebSocket/TLS
- Discord Bot Token configuration
- Discord Application ID, Guild ID and Channel ID
- Guild-scoped slash-command registration
- Discord Administrator permission detection
- Optional configured Discord admin user ID
- `/status`, `/sync`, `/pause`, `/resume`, `/emergency`, `/health`, `/testdiscord`, `/testchaster`, `/testemlalock`, `/history`, `/logs`, `/nextsync`, `/version`, `/panel`, `/addtime`, `/subtracttime`
- Browser dashboard with live timer state, controls, manual adjustments and history
- Built-in diagnostic page when the LittleFS dashboard has not been uploaded

## Sync behavior

The normal synchronization rule is **highest time wins**. The ESP32 reads both timers, identifies the larger remaining value, extends the lower timer by the difference, reads both again, and verifies that they are within a few seconds. Normal automatic synchronization never intentionally shortens the higher timer.

A failed API operation or verification failure pauses automatic synchronization so the device does not repeatedly make changes against an uncertain state.

Manual subtraction requires the Chaster keyholder token and EmlaLock keyholder API key. Manual additions use the normal credentials.

## Discord setup

Create/use a Discord application and bot, then provide these values in the ESP32 dashboard:

- **Enable Discord bot**: on
- **Application ID**: Discord application ID
- **Bot Token**: the bot token itself, not a webhook URL
- **Guild / Server ID**: server where commands should be registered
- **Channel ID**: channel used by `/testdiscord`
- **Admin Discord User ID**: optional extra administrator identity

Timer-control commands are protected by Discord Administrator permission or the configured admin user ID. `/addtime seconds` and `/subtracttime seconds` apply the same change to both timers and verify the result.

**Never commit a real bot token, API key, Chaster token, or EmlaLock key to GitHub.** Enter credentials into the device dashboard only.

## Build and flash

1. Install VS Code + PlatformIO.
2. Open this repository.
3. Connect an ESP32 DevKit.
4. Build and upload firmware.
5. **Also upload the LittleFS dashboard:** run `pio run --target uploadfs`, or use **PlatformIO: Upload Filesystem Image**.
6. On first boot connect to `C-EBOT-SETUP` and complete Wi-Fi setup.
7. Open the IP shown in Serial Monitor.
8. Configure Chaster, EmlaLock and optional Discord settings.
9. Use **Sync now** to perform the first live API synchronization.

## Important security note

This build still uses `WiFiClientSecure::setInsecure()` for HTTPS compatibility during bring-up. Do **not** expose the device to an untrusted network or port-forward it to the public internet. Before treating this as production firmware, add certificate validation, dashboard authentication and ESP32 flash/NVS encryption where supported.
