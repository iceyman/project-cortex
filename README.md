# 🤖 Project Cortex

> Two fully local AI desktop robots. No cloud. No subscription. Just your hardware. — no cloud, no subscriptions, no ongoing costs.

Built on **M5Stack CoreS3 SE** robots with a local PC as the AI brain. Talk to them by name, get real AI responses, watch them react with animated faces and servo head movements.

---

## ✨ Features

- 🎤 **Always-on voice detection** — no buttons, just say the robot's name and talk
- 🧠 **Fully local AI** — Whisper STT + Ollama LLM + gTTS, everything on your hardware
- 🎭 **Anime-style animated faces** — blinking, expressions, lip sync, eye tracking
- 🦾 **Servo head movements** — reacts physically to what it hears
- 🌈 **RGB LED moods** — 12 LEDs breathe with expression colours
- 💬 **Conversation memory** — remembers the last 12 messages per session
- 😴 **Smart sleep schedule** — automatically sleeps at night, droops head, dims screen
- ☀️ **Weather morning briefing** — wakes up and tells you the weather
- 🕺 **Dance party mode** — say "dance party" and watch it go
- 🦕 **Dino roar** — talk about dinosaurs and it shakes its head
- 👦 **Kid mode** — fully separate personality for young children (Rumi)
- 📱 **Web dashboard** — control both robots from any device on your network
- 🔄 **Wireless OTA updates** — deploy new firmware without a USB cable
- 🔀 **Multi-WiFi** — robots remember multiple networks (home, grandparents, etc.)
- 📡 **Inter-robot messaging** — "Tell Rumi that dinner is ready"
- 🌸 **Nintendo Talking Flower detection** — special reactions to Super Mario Wonder phrases

---

## 🖥️ Hardware Requirements

| Component | Notes |
|-----------|-------|
| 2× M5Stack CoreS3 SE | The robots themselves |
| PC or server as "Cerebro" | Any machine that can run Ollama — GPU recommended |
| 2× micro SD cards | 8GB minimum each |

**Cerebro can be anything that runs Python and Ollama.** We used a spare gaming PC — that's the recommended path. Here's how the options stack up:

| Cerebro | Whisper model | Ollama model | Response time |
|---------|--------------|--------------|---------------|
| PC with GPU (RTX 2080 Ti etc) | `medium` | `qwen2.5:14b` | ~2–3s ⚡ |
| PC/mini PC, no GPU | `small` | `qwen2.5:7b` | ~5–8s ✅ |
| Raspberry Pi 5 + [AI HAT+](https://www.raspberrypi.com/products/ai-hat-plus/) | `small` | `gemma3:4b` | ~8–12s ✅ |
| Raspberry Pi 5, no HAT | `base` | `phi3:mini` | ~15–20s 🐢 |
| Raspberry Pi 4 | `tiny` | `tinyllama` | ~25s+ 🐢 |

**We used:** Ubuntu 24.04, RTX 2080 Ti, `qwen2.5:14b` — fast enough that responses feel natural.

**Pi users:** A Raspberry Pi 5 with the official AI HAT+ (Hailo-8L NPU, ~$70 AUD) is the sweet spot if you want a dedicated always-on device. Use `base` Whisper and `gemma3:4b` for the best balance of speed and quality. A plain Pi 4 works but responses will feel slow.

> **Note:** The robots themselves are M5Stack CoreS3 SE — not Raspberry Pi. "Cerebro" is just what we call the server that does the heavy AI lifting.

---

## 🏗️ Architecture

```
You speak
    ↓
M5Stack mic (VAD detects voice above threshold)
    ↓
Records until silence detected
    ↓
Sends raw PCM audio to Cerebro over WiFi (HTTP POST)
    ↓
Cerebro: Whisper STT → transcribed text
    ↓
Is it addressed to this robot? (name check)
    ↙ No                    ↘ Yes
Ignore silently         Send to Ollama LLM
                              ↓
                        AI response → gTTS audio
                              ↓
                        Back to M5Stack (HTTP response)
                              ↓
                   Speaker plays + face animates + head moves
```

**Cerebro** is a Flask server that handles:
- Speech-to-text (faster-whisper)
- LLM responses (Ollama)
- Text-to-speech (gTTS, cached)
- Sleep scheduling
- OTA firmware serving
- Web dashboard

**Robots** are ESP32-S3 devices running Arduino firmware that handles:
- Voice activity detection (VAD)
- WiFi + HTTP communication
- Avatar face rendering (M5Stack-Avatar BSP fork)
- Servo head control (StackChan BSP)
- RGB LED moods
- SD card config loading

---

## 📋 Prerequisites

### On Cerebro (your server/PC)

```bash
# Python 3.10+
python3 --version

# Ollama
curl -fsSL https://ollama.ai/install.sh | sh
ollama pull qwen2.5:14b   # or gemma3:27b for 32GB RAM machines

# Python dependencies
pip install flask faster-whisper gtts requests pytz
```

### On your Windows/Mac development machine

- [VS Code](https://code.visualstudio.com/)
- [PlatformIO extension](https://platformio.org/install/ide?install=vscode)
- Git

### Hardware prep

- Two M5Stack CoreS3 SE units
- Micro SD cards formatted as FAT32

---

## 🚀 Setup Guide

### Step 1 — Clone the repo

```bash
git clone https://github.com/YOUR_USERNAME/project-cortex.git
cd project-cortex
```

### Step 2 — Set up Cerebro

```bash
# Copy the server to your machine
scp -r pi-server/ YOU@YOUR_CEREBRO_IP:~/kira-server/

# SSH in and set up the service
ssh YOU@YOUR_CEREBRO_IP
cd ~/kira-server

# Install dependencies
pip3 install flask faster-whisper gtts requests pytz

# Edit the server config (change the Ollama model if needed)
nano cerebro.py
# OLLAMA_MODEL = "qwen2.5:14b"  ← change to match what you pulled
# WEATHER_LAT / WEATHER_LON     ← change to your city
# OTA_KEY = "change_this_key"   ← set your own OTA key

# Install as a service
sudo cp cerebro.service /etc/systemd/system/
sudo systemctl enable kira
sudo systemctl start kira

# Verify it's running
curl http://localhost:5005/health
```

### Step 3 — Configure SD cards

Edit both files in `sd-cards/robot1/robot_config.json` and `sd-cards/robot2/robot_config.json`:

```json
{
  "wifi_networks": [
    {
      "ssid":     "YOUR_WIFI_NETWORK",
      "password": "YOUR_WIFI_PASSWORD",
      "pi_ip":    "YOUR_CEREBRO_IP",
      "pi_port":  5005,
      "label":    "home"
    }
  ]
}
```

For Rumi (robot2), also set:
```json
{
  "kid_name": "YOUR_CHILDS_NAME"
}
```

Copy `robot_config.json` to the **root** of each micro SD card.

### Step 4 — Customise personalities (optional)

Edit `pi-server/characters.json` to change names, personalities, and voices. Each robot has an `adult_prompt` and (optionally) a `kid_prompt`.

### Step 5 — Build and flash firmware

Open the `m5stack/` folder in VS Code with PlatformIO.

**⚠️ Critical: do not change the build flags in `platformio.ini`** — they are required for the servo library to work.

```bash
# In VS Code, select the m5stack-cores3 environment and click Upload
# Or use the PlatformIO CLI:
pio run -e m5stack-cores3 -t upload
```

Flash both robots with the same firmware. The SD card config tells each robot who it is.

### Step 6 — Set up OTA updates (optional but recommended)

Edit `deploy_firmware.bat` (Windows) and set:
```bat
set CEREBRO=YOUR_CEREBRO_IP:5005
set KEY=YOUR_OTA_KEY
```

From now on, your deploy workflow is:
1. Make changes, run `deploy_firmware.bat` (patches version, prompts rebuild)
2. Rebuild in PlatformIO
3. Press any key — firmware uploads to Cerebro
4. Robots self-update on next boot or wake

### Step 7 — First boot

Power on the robots. You should see:
```
[WiFi] Connected: home → YOUR_IP:5005
[OTA] Ready — hostname: robot1
[Boot] Kira board=27 vad=1100
```

Say **"Hey Kira"** and start talking. 🎉

---

## 📱 Web Dashboard

Visit `http://YOUR_CEREBRO_IP:5005/dashboard` from any device on your network.

Features:
- 🟢 Online/offline status per robot
- 🔄 Kid mode toggle (iOS-style switch)
- 💡 LED on/off toggle
- 🎚️ Volume slider (50–255)
- ⚡ Quick phrase buttons (Hey, Dance, Fact, Weather, Joke, Goodnight)
- 💬 Type anything for a robot to say
- 📋 Recent conversation history
- 🌙 Sleep all / ☀️ Wake all / 🔄 Restore schedule
- 🗑️ Reset conversation memory per robot

---

## 🎮 Using the Robots

| Action | How |
|--------|-----|
| Talk to Kira | Say "Hey Kira" or just "Kira, ..." |
| Talk to Rumi | Say "Hey Rumi" or just "Rumi, ..." |
| Kid mode (Rumi) | Tap bottom of Rumi's screen, or use dashboard toggle |
| Reset memory | Dashboard → Reset memory button |
| Dance party | Say "dance party" to either robot |
| Dino roar | Talk about dinosaurs |
| Inter-robot | "Tell Rumi that dinner's ready" |

**VAD dot (top left corner of screen):**
- ⚫ Dark = listening quietly
- 🟡 Yellow = getting louder
- 🟢 Green = recording your voice

---

## 🛠️ Configuration Reference

### SD Card (`robot_config.json`)

No reflash needed — just edit the file on the SD card.

| Field | Robot 1 (Kira) | Robot 2 (Rumi) | Description |
|-------|---------------|---------------|-------------|
| `robot_id` | `"robot1"` | `"robot2"` | Unique identifier |
| `robot_name` | `"Kira"` | `"Rumi"` | Display name |
| `default_mode` | `"adult"` | `"kid"` | Starting mode |
| `led_color` | `"purple"` | `"teal"` | LED colour theme |
| `has_kid_mode` | `false` | `true` | Enable kid mode toggle |
| `kid_name` | — | `"Alex"` | Child's name for personalisation |
| `speaker_volume` | `180` | `200` | Volume 0–255 |
| `vad_threshold` | `1100` | `500` | Mic sensitivity (lower = more sensitive) |
| `vad_silence_ms` | `1400` | `1800` | Silence pause before sending (ms) |
| `vad_max_secs` | `8` | `10` | Maximum recording length |

### Server (`cerebro.py`)

Restart service after changes: `sudo systemctl restart kira`

| Setting | Default | Description |
|---------|---------|-------------|
| `WHISPER_MODEL` | `"medium"` | STT quality: tiny/base/small/medium/large |
| `OLLAMA_MODEL` | `"qwen2.5:14b"` | LLM model name |
| `SLEEP_HOUR/MINUTE` | `19, 30` | Bedtime (local time, no DST) |
| `WAKE_HOUR/MINUTE` | `7, 30` | Wake time |
| `WEATHER_LAT/LON` | Brisbane | Your city coordinates |
| `OTA_KEY` | `"change_me"` | Firmware upload authentication key |
| `SERVER_PORT` | `5005` | Port the Flask server runs on |

### Recommended Ollama Models

| Model | RAM needed | Quality | Notes |
|-------|------------|---------|-------|
| `gemma3:4b` | ~4GB | Good | Works on CPU, low RAM |
| `qwen2.5:7b` | ~6GB | Very good | Great balance |
| `qwen2.5:14b` | ~10GB | Excellent | Recommended with GPU |
| `gemma3:27b` | ~20GB | Excellent | Best for 32GB RAM |
| `llama3.3:70b` Q4 | ~45GB | Near-Claude | Needs 64GB RAM |

---

## 🚨 Troubleshooting

### Robot not responding to voice
- Watch the VAD dot — never turns green = lower `vad_threshold` in SD config
- Always green (false triggers) = raise `vad_threshold`
- Check Cerebro is reachable: `http://YOUR_CEREBRO_IP:5005/health`

### Boot loop after OTA update
- `FIRMWARE_VERSION` in `main.cpp` doesn't match what's on Cerebro
- Run `deploy_firmware.bat`, rebuild in PlatformIO, deploy again

### Servos not moving
- Check `platformio.ini` has exactly these build flags:
  ```
  build_unflags = -std=gnu++11
  build_flags = ... -std=gnu++17 -fpermissive -DUART_SCLK_DEFAULT=UART_SCLK_XTAL
  ```
- Delete any `src/M5StackChan.h` local file if it exists

### Only one robot responds when both speak
- This is normal — a threading lock serialises requests through Whisper+Ollama
- Second robot responds ~2–3 seconds later
- Both always respond, just staggered

### OTA upload fails with 413
- Check `app.config['MAX_CONTENT_LENGTH'] = 8 * 1024 * 1024` is in `cerebro.py`
- Restart service after updating

### SD card not loading
- File must be at the **root** of the SD card as `/robot_config.json`
- Format SD card as FAT32 if issues persist
- Serial output will show `[SD] No config` if not found

### "Pi offline!" on screen
- Check service: `sudo systemctl status kira`
- Check from robot's network: `http://YOUR_CEREBRO_IP:5005/health`
- Check firewall allows port 5005

---

## 📁 File Structure

```
project-cortex/
│
├── pi-server/
│   ├── cerebro.py        Flask AI server
│   ├── characters.json       Robot personalities (edit to customise)
│   └── install.sh            Cerebro setup script
│
├── m5stack/
│   ├── platformio.ini        Build config (do not modify flags)
│   └── src/
│       ├── main.cpp          Main firmware
│       ├── kira_faces.h      Avatar face drawing
│       ├── dance_music.h     Dance audio data
│       ├── wifi_networks.h   Multi-WiFi support
│       └── LTR5XX.h          Proximity sensor stub
│
├── sd-cards/
│   ├── robot1/
│   │   └── robot_config.json Copy to ROOT of Kira's SD card
│   └── robot2/
│       └── robot_config.json Copy to ROOT of Rumi's SD card
│
├── deploy_firmware.bat       Windows OTA deploy script
└── README.md                 This file
```

---

## 🔌 API Reference

The Flask server exposes these endpoints (default port 5005):

```
GET  /health              Server status + sleep state
POST /chat                Voice audio → AI response audio
GET  /greet               Boot greeting with weather
GET  /pending             Poll for queued messages
POST /reset               Clear conversation memory
GET  /history/<robot_id>  Conversation history
POST /sleep               Force sleep mode
POST /wake                Force wake mode
POST /schedule            Restore automatic schedule
POST /say                 Queue text for robot to speak
POST /mode                Switch adult/kid mode
POST /led                 LED on/off
POST /volume              Set volume
GET  /ota/version         Current firmware version
GET  /ota/firmware/<id>   Download firmware binary
POST /ota/upload          Upload new firmware
GET  /dashboard           Web dashboard
```

---

## 🧩 Extending

### Adding a new robot

1. Add a new entry to `characters.json`
2. Create a new `robot_config.json` in `sd-cards/robot3/`
3. Flash the same `main.cpp` firmware to a third CoreS3 SE
4. Copy the SD config to a new SD card

### Changing the AI personality

Edit `characters.json`. Each robot has:
- `adult_prompt` — personality for adult conversations
- `kid_prompt` — personality for kid mode (optional)
- `voice_tld` — gTTS accent (`com.au` = Australian, `co.uk` = British, `com` = American)
- `idle_phrases` — random things the robot says unprompted

### Using a different LLM

Change `OLLAMA_MODEL` in `cerebro.py` to any model you have pulled with Ollama. The server uses the standard Ollama chat API so any compatible model works.

### Remote access (away from home)

If you have a static home IP, forward port 5005 on your router to Cerebro. Add your static IP to the SD card `wifi_networks` array for a hotspot network entry. Robots on a phone hotspot will reach Cerebro via your static IP.

---

## 🙏 Credits & Dependencies

| Library | Purpose |
|---------|---------|
| [M5Stack-Avatar](https://github.com/m5stack/M5Stack-Avatar) | Face animation framework |
| [StackChan-BSP](https://github.com/m5stack/StackChan-BSP) | Servo + hardware abstraction |
| [faster-whisper](https://github.com/SYSTRAN/faster-whisper) | Speech-to-text |
| [Ollama](https://ollama.ai) | Local LLM inference |
| [gTTS](https://github.com/pndurette/gTTS) | Text-to-speech |
| [Flask](https://flask.palletsprojects.com) | Server framework |
| [Open-Meteo](https://open-meteo.com) | Free weather API |
| [ArduinoJson](https://arduinojson.org) | JSON on ESP32 |

---

## 📄 Licence

MIT — do whatever you want with it. If you build something cool, share it!

---

## 💬 About

This started as a "wouldn't it be fun" project and turned into something that genuinely gets used every day. The robots greet us in the morning, tell my son dinosaur facts, have dance parties, and go to sleep at bedtime without needing any apps or subscriptions.

Built with a lot of help from Claude. Helped With Avatar and StackChan-BSP 🤖

If you build your own version, open an issue or discussion — would love to see what people make.
