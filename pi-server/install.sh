#!/bin/bash
set -e
echo "Installing Cerebro..."
python3 -m venv venv
source venv/bin/activate
pip install flask faster-whisper gtts pydub requests
echo "Done! Edit cerebro.py with your settings, then:"
echo "  sudo cp cerebro.service /etc/systemd/system/"
echo "  sudo systemctl enable cerebro && sudo systemctl start cerebro"
