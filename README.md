# C-EBOT ESP32

Standalone ESP32 port of c-ebot. The ESP32 hosts the setup and control webpage; a phone or PC connects over Wi-Fi using a browser.

## Current status

Initial hardware/web scaffold:

- PlatformIO project for `esp32dev`
- Wi-Fi provisioning with WiFiManager
- LittleFS-hosted dashboard
- REST status/config/sync/pause/resume endpoints
- NVS-backed configuration storage
- Dashboard polling and responsive UI

**Live Chaster/EmlaLock modifications are intentionally disabled in this first commit.** The API client implementation must be tested against real account permissions and response schemas before enabling automatic changes.

## Build and flash

1. Install VS Code and PlatformIO.
2. Open this repository.
3. Connect an ESP32 DevKit.
4. Build and upload firmware.
5. Upload the filesystem image (`PlatformIO: Upload Filesystem Image`).
6. On first boot, connect to `C-EBOT-SETUP` and follow the Wi-Fi portal.
7. Open the IP address shown in Serial Monitor.

## Security

Do not expose the ESP32 directly to the public internet. Credentials are stored in NVS and are never returned by the configuration GET endpoint. Add local authentication and encrypted storage before using this on an untrusted network.
