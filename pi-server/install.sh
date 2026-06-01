#!/bin/bash
# ═══════════════════════════════════════════════════════════
#  Cerebro Install Script
#  Kira & Rumi AI Robot Server
#  Ubuntu 24.04 LTS + RTX 2080 Ti (CUDA 12.2)
#  Run as normal user with sudo access
# ═══════════════════════════════════════════════════════════

set -e
CEREBRO_IP="YOUR_CEREBRO_IP"
CEREBRO_HOST="cerebro"
TIMEZONE="Australia/Brisbane"
USER_HOME="/home/YOUR_USERNAME"
SERVER_DIR="$USER_HOME/kira-server"

echo ""
echo "╔═══════════════════════════════════════════╗"
echo "║       Cerebro Install — Kira & Rumi       ║"
echo "╚═══════════════════════════════════════════╝"
echo ""

# ── 1. Hostname ──────────────────────────────────────────
echo "[1/9] Setting hostname to cerebro..."
sudo hostnamectl set-hostname cerebro
sudo sed -i "s/127.0.1.1.*/127.0.1.1\tcerebro/" /etc/hosts
echo "      Done ✓"

# ── 2. Timezone ──────────────────────────────────────────
echo "[2/9] Setting timezone to $TIMEZONE..."
sudo timedatectl set-timezone $TIMEZONE
echo "      Done ✓"

# ── 3. System packages ───────────────────────────────────
echo "[3/9] Installing system packages..."
sudo apt-get update -q
sudo apt-get install -y -q \
    python3 python3-pip python3-venv \
    ffmpeg curl git build-essential \
    net-tools
echo "      Done ✓"

# ── 4. Ollama with CUDA ───────────────────────────────────
echo "[4/9] Installing Ollama (GPU/CUDA support)..."
curl -fsSL https://ollama.ai/install.sh | sh

# Configure Ollama service for GPU + keep alive
sudo mkdir -p /etc/systemd/system/ollama.service.d
sudo tee /etc/systemd/system/ollama.service.d/override.conf > /dev/null << 'EOF'
[Service]
Environment="OLLAMA_HOST=0.0.0.0:11434"
Environment="OLLAMA_KEEP_ALIVE=-1"
EOF

sudo systemctl daemon-reload
sudo systemctl enable ollama
sudo systemctl restart ollama
sleep 5
echo "      Done ✓"

# ── 5. Pull AI model ─────────────────────────────────────
echo "[5/9] Pulling qwen2.5:14b (this takes a few minutes)..."
ollama pull qwen2.5:14b
echo "      Done ✓"

# ── 6. Python environment ─────────────────────────────────
echo "[6/9] Setting up Python environment..."
mkdir -p $SERVER_DIR
cd $SERVER_DIR
python3 -m venv venv
source venv/bin/activate

pip install --upgrade pip -q
pip install -q \
    flask \
    requests \
    faster-whisper \
    gtts \
    pydub \
    numpy

echo "      Done ✓"

# ── 7. Copy server files ──────────────────────────────────
echo "[7/9] Copying server files..."
# Copy from current directory if they exist, otherwise placeholder
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ -f "$SCRIPT_DIR/pi-server/cerebro.py" ]; then
    cp "$SCRIPT_DIR/pi-server/cerebro.py" "$SERVER_DIR/"
    cp "$SCRIPT_DIR/pi-server/characters.json" "$SERVER_DIR/"
    echo "      Copied cerebro.py and characters.json ✓"
else
    echo "      ⚠ Run this script from the kira-complete folder"
    echo "        so cerebro.py gets copied automatically."
    echo "        Or copy manually to $SERVER_DIR"
fi

mkdir -p "$SERVER_DIR/conversation_history"
echo "      Done ✓"

# ── 8. Systemd service ────────────────────────────────────
echo "[8/9] Creating kira systemd service..."
sudo tee /etc/systemd/system/cerebro.service > /dev/null << EOF
[Unit]
Description=Kira AI Robot Server
After=network-online.target ollama.service
Wants=network-online.target

[Service]
Type=simple
User=YOUR_USERNAME
WorkingDirectory=$SERVER_DIR
ExecStart=$SERVER_DIR/venv/bin/python $SERVER_DIR/cerebro.py
Restart=always
RestartSec=5
Environment="PYTHONUNBUFFERED=1"

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable kira
sudo systemctl start kira
sleep 3
echo "      Done ✓"

# ── 9. Warm up Ollama ─────────────────────────────────────
echo "[9/9] Warming up Ollama model..."
ollama run qwen2.5:14b << 'EOF'
hi
/bye
EOF
echo "      Done ✓"

# ── Summary ───────────────────────────────────────────────
echo ""
echo "╔═══════════════════════════════════════════════════════╗"
echo "║                  Install Complete! 🎉                  ║"
echo "╠═══════════════════════════════════════════════════════╣"
echo "║  Hostname:   cerebro                                   ║"
echo "║  Server:     http://$CEREBRO_IP:5005                  ║"
echo "║  Health:     http://$CEREBRO_IP:5005/health           ║"
echo "║  Ollama:     http://$CEREBRO_IP:11434                 ║"
echo "║  Model:      qwen2.5:14b (GPU accelerated)            ║"
echo "║  Timezone:   Australia/Brisbane (AEST)                 ║"
echo "╠═══════════════════════════════════════════════════════╣"
echo "║  SD CARD UPDATE NEEDED:                                ║"
echo "║  Change pi_ip to $CEREBRO_IP in robot_config.json    ║"
echo "╚═══════════════════════════════════════════════════════╝"
echo ""
echo "Test with: curl http://$CEREBRO_IP:5005/health"
echo ""
echo "NOTE: Reboot to apply hostname change: sudo reboot"
echo ""
