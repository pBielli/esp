# ESP8266 DDNS Monitor — Frontend

A static web frontend for the ESP8266 DDNS Monitor firmware, deployable on **GitHub Pages** or any static host.

## Features

- 🖥️ Connect to any ESP8266 device by IP or mDNS hostname
- 📡 Real-time system info (MAC, SSID, RSSI, IPs, uptime, free heap)
- ⬡ DDNS status dashboard with sync indicator (domain IP vs public IP)
- 🔌 GPIO read/write control panel
- 💡 LED on/off/blink with visual feedback
- 📶 WiFi credential update (SSID and password saved to EEPROM)
- 🔧 HTTP proxy (CURL) and DNS resolver tools
- 📋 System log viewer
- 🌙 Industrial terminal aesthetic, mobile-friendly

## Usage

1. Deploy to GitHub Pages (the `index.html` is the entry point).
2. Open the page in your browser.
3. Enter your ESP8266's IP address (e.g. `192.168.1.42`) or mDNS hostname (`ddns-checker.local`).
4. Enter the admin password (default: `admin`).
5. Click **ESTABLISH LINK**.

## Auth

All API endpoints use HTTP Basic Auth with username `admin` and the password entered at connect time.  
Default credentials: `admin` / `admin`

## CORS Note

The ESP8266 firmware **must send CORS headers** for the browser to allow cross-origin requests from a GitHub Pages origin (`https://youruser.github.io`).

### Option A — Add CORS to firmware (recommended)
Add this to your ESP8266 firmware before any `server.send()` call:
```cpp
server.sendHeader("Access-Control-Allow-Origin", "https://*.github.io");
server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
server.sendHeader("Access-Control-Allow-Headers", "Authorization, Content-Type");
```

### Option B — Use the frontend locally
Download and open `index.html` directly from your local machine. Note that `file://` origins are also subject to CORS restrictions in most modern browsers.

### Option C — Host on the ESP itself
Add an endpoint in the firmware to serve this `index.html`.

## API Endpoints

All endpoints require HTTP Basic Auth. The frontend calls the following:

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/info` | System info (MAC, IPs, RSSI, uptime, heap, DDNS) |
| GET | `/api/time` | Device time (`YYYY-MM-DD HH:MM:SS`) |
| GET | `/api/ddns/update` | Force a DDNS update cycle |
| GET | `/api/resolve?host=<hostname>` | DNS resolution via device stack |
| GET | `/api/gpio/info` | List available GPIO pins |
| GET | `/api/gpio/read?pin=<n>` | Read a GPIO pin state |
| POST | `/api/gpio/set` | Set GPIO pin mode/value (`pin`, `mode`, `value`) |
| GET | `/api/led/on` | Turn LED on |
| GET | `/api/led/off` | Turn LED off |
| GET | `/api/led/blink?times=<n>` | Blink LED n times |
| POST | `/api/led/pin` | Change LED pin (`pin`) |
| POST | `/api/set/ssid` | Save new WiFi SSID (`ssid`) |
| POST | `/api/set/pswd` | Save new WiFi password (`pswd`) |
| GET | `/api/curl?url=<url>` | HTTP GET proxy through device |
| GET | `/api/help` | List all available endpoints |
| GET | `/api/log` | Fetch system log entries |

## Expected `/api/info` Response

```json
{
  "mac":      "AA:BB:CC:DD:EE:FF",
  "ssid":     "MyNetwork",
  "rssi":     -65,
  "local_ip": "192.168.1.42",
  "ddns_ip":  "1.2.3.4",
  "public_ip":"1.2.3.4",
  "ddns":     "myhome.duckdns.org",
  "mdns":     "ddns-checker.local",
  "uptime":   3600,
  "free_heap": 32768
}
```

**RSSI %** is calculated as: `((rssi + 100) / 60) * 100`, mapping the typical range −100 dBm (0 %) to −40 dBm (100 %).

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
