# 🤖 Project Cortex

A custom AI desktop robot crew built on M5Stack CoreS3 SE hardware.
Two robots — Kira and Rumi — powered by a local AI server (Cerebro) running
Ollama, Whisper STT, and gTTS. Fully offline capable on your home network.

Built with help from Claude — who also helped wrangle the Avatar and StackChan-BSP libraries into submission.

---

## The Crew

| Robot | Personality | Mode |
|-------|-------------|------|
| 💜 **Kira** | Warm, enthusiastic gaming nerd. Main host. | Adult |
| 🩷 **Rumi** | Gentle, playful kid companion. | Kid |

---

## Hardware

- **M5Stack CoreS3 SE** × 2 (ESP32-S3, 320×240 display, built-in mic/speaker)
- **StackChan BSP** — servo control for pan/tilt head movement
- **Cerebro** — Ubuntu server with GPU (RTX 2080 Ti recommended)

---

## Software Stack

| Component | Technology |
|-----------|-----------|
| LLM | Ollama + qwen3:8b (local, no API key) |
| STT | faster-whisper (medium, GPU) |
| TTS | gTTS (Australian English) |
| Server | Flask (cerebro.py) |
| Firmware | PlatformIO + Arduino + M5Stack Avatar |
| App | React Native + Expo |

---

## Features

- 🎤 Wake word detection (VAD-based, no cloud)
- 💬 Persistent conversation memory across reboots
- 🌤️ Daily weather briefings on boot
- 🕺 Dance party mode with LEDs and music
- 🦕 Dinosaur roar with descending tones + head shake
- 😴 Scheduled sleep/wake (configurable)
- 📱 Mobile app (Android) with PIN-locked parent dashboard
- ⬆️ OTA firmware updates over WiFi
- 🌏 Remote access via Tailscale
- 💾 SD card config editable from dashboard
- 🌸 Nintendo Talking Flower detection (Super Mario Bros. Wonder)
- 🧠 Kira uses Qwen3 thinking mode, Rumi uses fast mode

---

## Cerebro Setup

### Requirements
- Ubuntu 22.04+
- Python 3.10+
- Ollama installed (`curl -fsSL https://ollama.com/install.sh | sh`)
- ffmpeg (`sudo apt install ffmpeg`)
- GPU recommended (RTX 2080 Ti or similar)

### Install

```bash
git clone https://github.com/YOUR_USERNAME/project-cortex
cd project-cortex/pi-server
bash install.sh
```

### Configure

Edit `characters.json` with your robot names and personalities.

### Run

```bash
sudo systemctl enable cerebro
sudo systemctl start cerebro
curl http://YOUR_CEREBRO_IP:5005/health
```

### Dashboard

```
http://YOUR_CEREBRO_IP:5005/dashboard
```

---

## Firmware Setup

> **Note:** The firmware uses a fork of the M5Stack Avatar library (`iceyman/m5stack-avatar`)
> that includes fixes for the CoreS3 SE display. Using the original library will cause crashes.

### Requirements
- PlatformIO (VS Code extension)
- Windows PC recommended for deploy script

### Configure

Edit `deploy_firmware.bat` — set your Cerebro IP and OTA key.

Create SD card config files (`sd-cards/robot1/robot_config.json`):

```json
{
  "robot_id": "robot1",
  "robot_name": "Kira",
  "default_mode": "adult",
  "vad_threshold": 1100,
  "speaker_volume": 180,
  "led_color": "purple"
}
```

### Build and Deploy

```
deploy_firmware.bat
```

Ctrl+Alt+B in PlatformIO, then press any key to upload.
Robots OTA update on next boot.

---

## Mobile App

Located in `mobile-app/`. Built with React Native + Expo.

```bash
cd mobile-app
npm install
npx expo start    # Test with Expo Go
eas build --platform android --profile preview  # Build APK
```

### Features
- Choose your robot (Rumi default)
- Hold-to-talk voice interface
- Animated mic level bars
- PIN lock on parent controls
- Online/offline status
- Works anywhere via Tailscale

---

## Remote Access

Install Tailscale on Cerebro:
```bash
curl -fsSL https://tailscale.com/install.sh | sh
sudo tailscale up
tailscale ip   # Use this IP in app Settings
```

---

## Project Structure

```
project-cortex/
├── m5stack/          — Firmware (PlatformIO)
│   └── src/
│       ├── main.cpp        — Main firmware
│       ├── kira_faces.h    — Character face designs
│       └── dance_music.h   — Immortals chiptune
├── pi-server/        — Cerebro server
│   ├── cerebro.py          — Main server
│   ├── cerebro.service     — systemd service
│   ├── characters.json     — Robot personalities
│   └── install.sh          — Setup script
├── mobile-app/       — React Native app
├── web/              — Browser-based chat (kidchat.html)
├── deploy_firmware.bat — Windows deploy script
└── README.md
```

---

Built with help from Claude — who also helped wrangle the Avatar and StackChan-BSP libraries into submission.
