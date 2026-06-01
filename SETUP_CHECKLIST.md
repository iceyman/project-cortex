# 🧠 Project Cortex — Setup Checklist


Work through this top to bottom. Check each item off as you go.

## Cerebro Setup

- [ ] Ollama installed and model pulled (`ollama pull qwen2.5:14b`)
- [ ] Python dependencies installed (`pip install flask faster-whisper gtts requests pytz`)
- [ ] `cerebro.py` — set `OTA_KEY` to your own secret key
- [ ] `cerebro.py` — set `WEATHER_LAT`, `WEATHER_LON`, `WEATHER_CITY` to your location
- [ ] `cerebro.py` — set `OLLAMA_MODEL` to match what you pulled
- [ ] `characters.json` — set kid's name if using kid mode (replace `YOUR_SONS_NAME`)
- [ ] `cerebro.service` — replace `YOUR_USERNAME` with your Linux username
- [ ] Service installed: `sudo cp cerebro.service /etc/systemd/system/ && sudo systemctl enable kira && sudo systemctl start kira`
- [ ] Service running: `curl http://localhost:5005/health` returns JSON

## SD Cards

- [ ] Both SD cards formatted as FAT32
- [ ] `robot_config.json` copied to ROOT of each SD card (not in a subfolder)
- [ ] Both configs have real WiFi SSID and password
- [ ] Both configs have Cerebro's real IP address
- [ ] Rumi's config has child's real name in `kid_name`

## Firmware

- [ ] PlatformIO + VS Code installed
- [ ] `platformio.ini` — set OTA password in `upload_flags`
- [ ] `main.cpp` — `FIRMWARE_VERSION` set (any string, e.g. `"1.0.0"`)
- [ ] Built and flashed to both robots via USB
- [ ] Both robots boot, connect to WiFi, greet themselves

## Deploy Script (Windows)

- [ ] `deploy_firmware.bat` — set `CEREBRO` to your IP:port
- [ ] `deploy_firmware.bat` — set `KEY` to match `OTA_KEY` in server
- [ ] Test: run bat, rebuild, press key → see `OK — robot1 firmware ... ready`

## First Boot Verification

- [ ] Robot shows "Connecting..." then "Online!"
- [ ] Robot says a greeting (with weather!)
- [ ] Say the robot's name + a question → gets a response
- [ ] Dashboard loads at `http://YOUR_CEREBRO_IP:5005/dashboard`
- [ ] Both robots appear on dashboard as online
