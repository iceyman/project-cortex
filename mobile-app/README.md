# Project Cortex — Mobile App

Talk to Rumi & Kira from your phone or tablet.

## Screens
- **Talk** — Your kid's screen. Big button, hold to talk, robot talks back
- **Dashboard** — Parent controls. Say things, change modes, volume, OTA update
- **Settings** — Set Cerebro IP address and kid's name

## Setup

### 1. Install Expo CLI
```bash
npm install -g expo-cli eas-cli
```

### 2. Install dependencies
```bash
npm install
```

### 3. Test on your phone instantly (no build needed)
```bash
npx expo start
```
Scan the QR code with the **Expo Go** app on your phone.
Works on any Android phone — great for testing.

### 4. Build a proper APK (for sideloading or Play Store)

First, create a free Expo account at https://expo.dev

```bash
# Login
eas login

# Build APK (for testing / sideloading)
eas build --platform android --profile preview

# Build AAB (for Play Store submission)
eas build --platform android --profile production
```

The build runs in the cloud — no Android Studio needed.
Takes about 10-15 minutes. You get a download link when done.

### 5. Play Store submission
- Go to https://play.google.com/console
- Pay the $25 one-time developer fee
- Create a new app → upload the AAB file
- Fill in store listing (name, description, screenshots)
- Submit for review (~3-7 days)

## Cerebro requirement
The app talks to Cerebro over your local WiFi.
Make sure Cerebro is running:
```bash
sudo systemctl status cerebro
curl http://YOUR_CEREBRO_IP:5005/health
```

## Away from home
To use the app outside your home network, set up Tailscale:
1. Install Tailscale on Cerebro: `curl -fsSL https://tailscale.com/install.sh | sh && sudo tailscale up`
2. Install Tailscale on your phone (free)
3. Use the Tailscale IP for Cerebro in Settings

## babel.config.js
```js
module.exports = function(api) {
  api.cache(true);
  return { presets: ['babel-preset-expo'] };
};
```
