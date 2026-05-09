# Fix: Fallback HTTP → HTTPS per chiamate in uscita

## Problema
- Tutte le chiamate HTTPS falliscono con code=-1 (connection refused)
- NTP (UDP) funziona, ma TCP outbound è bloccato/instabile
- Il falso positivo in `updateDDNS()` segnala successo quando la richiesta fallisce

## Modifiche

### 1. `ino/DDNS.cpp` — `getPublicIP()`
**Prima**: tentava solo HTTPS
**Dopo**: tenta HTTP prima, poi HTTPS come fallback

```cpp
// Try HTTP first
WiFiClient c;
http.begin(c, "http://" + host);
code = http.GET();
if (code == HTTP_CODE_OK) { ip = http.getString(); ip.trim(); }
http.end();
if (ip != "") return ip;

// Fallback HTTPS
WiFiClientSecure cs;
cs.setInsecure();
http.begin(cs, "https://" + host);
code = http.GET();
if (code == HTTP_CODE_OK) { ip = http.getString(); ip.trim(); }
http.end();
if (ip != "") return ip;
```

### 2. `ino/DDNS.cpp` — `getPublicIP(int serverIdx)`
Stessa logica: HTTP prima, HTTPS fallback.

### 3. `ino/DDNS.cpp` — `updateDDNS()`
- Se URL è https://, tenta prima HTTPS, se fallisce prova http://
- Quando HTTP fallisce, restituisce `"KO_HTTP:codice"` invece di `""`
  → `checkAndUpdateDDNS()` fa `resp.indexOf("KO")` che trova "KO" → flag=false (corretto!)

### 4. `ino/WebServer.cpp` — `/api/curl`
- Errore descrittivo: `"Error (code): " + http.errorToString(code)`
