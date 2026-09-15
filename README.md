# C-EBOT ESP32

ESP32 DevKit port of c-ebot. The ESP32 hosts the web dashboard and can run a real Discord application bot using the Discord Bot Token/Gateway. A PC or phone is only used for configuration/control.

## Test-build status

### Included
- PlatformIO `esp32dev` project
- Wi-Fi provisioning with `C-EBOT-SETUP`
- LittleFS dashboard
- NVS-backed credential/configuration storage
- Chaster/EmlaLock credential fields, including keyholder credentials
- Real Discord Gateway client over WebSocket/TLS
- Discord Bot Token configuration
- Discord Application ID, Guild ID and Channel ID
- Discord slash-command registration
- Discord Administrator permission detection
- Optional configured Discord admin user ID
- Discord test-message endpoint
- `/status`, `/sync`, `/pause`, `/resume`, `/emergency`, `/health`, `/testdiscord`, `/testchaster`, `/testemlalock`, `/history`, `/logs`, `/nextsync`, `/version`, `/panel`
- Browser dashboard showing Discord connection state
- Clear fallback page and Serial Monitor diagnostic when the LittleFS dashboard has not been uploaded

### Intentionally disabled
The Chaster/EmlaLock timer read/write engine is still **test-only**. No live timer mutation is performed by this firmware. This is deliberate: the API response formats and account permissions must be verified on-device before enabling automatic additions/subtractions.

## Discord setup

Create/use a Discord application and bot, then provide these values in the ESP32 dashboard:

- **Enable Discord bot**: on
- **Application ID**: Discord application ID
- **Bot Token**: the bot token itself, not a webhook URL
- **Guild / Server ID**: server where commands should be registered
- **Channel ID**: channel used by `/testdiscord`
- **Admin Discord User ID**: optional extra administrator identity

The firmware registers guild-scoped slash commands so changes appear quickly during testing. Server members with the Discord Administrator permission are accepted for timer-control commands; the configured admin user ID can also be used.

**Never commit a real bot token, API key, Chaster token, or EmlaLock key to GitHub.** Enter credentials into the device dashboard only.

## Build and flash

1. Install VS Code + PlatformIO.
2. Open this repository.
3. Connect an ESP32 DevKit.
4. Build and upload firmware.
5. **Also upload the LittleFS dashboard:** run `pio run --target uploadfs`, or use **PlatformIO: Upload Filesystem Image**.
6. On first boot connect to `C-EBOT-SETUP` and complete Wi-Fi setup.
7. Open the IP shown in Serial Monitor.
8. Configure Discord and press **Test Discord**.
9. Verify the bot appears online and the slash commands register in the configured guild.

If firmware was uploaded without the filesystem image, the ESP32 will now show a built-in page explaining that the dashboard filesystem is missing instead of returning repeated `index.html does not exist` errors. Uploading the filesystem image once fixes that state.

## Important security note

This is a testing firmware. The current Discord HTTPS REST calls use an insecure TLS client for compatibility during bring-up. Do **not** expose the device to an untrusted network or treat this build as production-ready. Before production, replace that with certificate validation, add dashboard authentication, and use ESP32 flash/NVS encryption where supported.

Do not port-forward the ESP32 dashboard to the public internet.
