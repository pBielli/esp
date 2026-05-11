#pragma once
// ============================================================
//  user_config.h  —  Configurazione personalizzabile
//  Modifica questi valori prima di flashare il firmware.
//  NON committare questo file con credenziali reali!
// ============================================================

// ── WiFi default ─────────────────────────────────────────────
#define DEFAULT_WIFI_SSID      "CasaBielli"
#define DEFAULT_WIFI_PASSWORD  "CasaBielli01"

// ── Access Point fallback ────────────────────────────────────
#define DEFAULT_AP_SSID        ""        // vuoto = "ESP-<MAC>"
#define DEFAULT_AP_PASSWORD    ""        // vuoto = AP aperto

// ── Admin web ────────────────────────────────────────────────
#define DEFAULT_ADMIN_PASSWORD "admin"

// ── mDNS ─────────────────────────────────────────────────────
#define DEFAULT_MDNS_NAME      "esp-device"

// ── DDNS (DuckDNS) ───────────────────────────────────────────
#define DEFAULT_DDNS_HOSTNAME  "patbee.duckdns.org"
#define DEFAULT_DDNS_TOKEN     "3894523a-d53d-433d-b661-83ac156319fa"
#define DEFAULT_DDNS_DOMAIN    "patbee"

// ── Hardware ─────────────────────────────────────────────────
#define DEFAULT_LED_PIN        2         // GPIO2 = LED blu su NodeMCU/D1 Mini
#define DEFAULT_GPIO_INVERT    1         // 1 = logica invertita (LED attivo LOW)
#define DEFAULT_PWM_PIN        5         // GPIO5 = D1 su NodeMCU

// ── NTP ──────────────────────────────────────────────────────
#define DEFAULT_NTP_SERVER     "pool.ntp.org"
#define DEFAULT_TZ_OFFSET      7200      // UTC+2 (Italia estiva / CEST)
//                             3200      // UTC+1 (Italia invernale / CET)

// ── DDNS intervallo (secondi) ────────────────────────────────
#define DEFAULT_DDNS_INTERVAL  300       // 5 minuti

// ── OTA ──────────────────────────────────────────────────────
#define DEFAULT_OTA_URL        ""        // URL JSON o .bin diretto
#define DEFAULT_OTA_INTERVAL   0         // 0 = disabilitato

// ── Tentativi WiFi ───────────────────────────────────────────
#define DEFAULT_WIFI_RETRY     2
