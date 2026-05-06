# ESP8266 DDNS Monitor — Frontend

A static web frontend for the ESP8266 DDNS Monitor firmware, deployable on **GitHub Pages** or any static host.

## Features

- 🖥️ Connect to any ESP8266 device by IP or mDNS hostname
- 📡 Real-time system info (MAC, SSID, RSSI, IPs, uptime)
- ⬡ DDNS status dashboard with sync indicator
- 🔌 GPIO read/write control panel
- 💡 LED on/off/blink with visual feedback
- 📶 WiFi credential update
- 🔧 HTTP proxy (CURL) and DNS resolver tools
- 📋 System log viewer
- 🌙 Industrial terminal aesthetic, mobile-friendly

## Usage

1. Deploy to GitHub Pages (the `index.html` is the entry point).
2. Open the page in your browser.
3. Enter your ESP8266's IP address (e.g. `192.168.1.42`) or mDNS hostname (`ddns-checker.local`).
4. Enter the admin password (default: `admin`).
5. Click **ESTABLISH LINK**.

## CORS Note

The ESP8266 firmware **does not send CORS headers** by default. This means requests from a GitHub Pages origin (`https://youruser.github.io`) will be blocked by the browser.

**Solutions:**

### Option A — Add CORS to firmware (recommended)
Add this to your ESP8266 firmware before any `server.send()` call:
```cpp
server.sendHeader("Access-Control-Allow-Origin", "*");
server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
server.sendHeader("Access-Control-Allow-Headers", "Authorization, Content-Type");
```

### Option B — Use the frontend locally
Download and open `index.html` directly from your local machine. Modern browsers still apply CORS for `file://` origins.

### Option C — Host on the ESP itself
Add an endpoint in the firmware to serve this `index.html`.

## Structure

```
ddns-monitor/
├── index.html       # Main app shell
├── css/
│   └── style.css    # Industrial terminal theme
├── js/
│   └── app.js       # Application logic & API calls
└── README.md
```

## Auth

All sensitive endpoints use HTTP Basic Auth with username `admin` and the password you enter at connect time.

Default credentials: `admin` / `admin`
