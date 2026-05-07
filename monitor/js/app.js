/* ============================================================
   DDNS Monitor — Application Logic
   ============================================================ */

// ── State ──────────────────────────────────────────────────
const state = {
  host: '',
  pass: '',
  authHeader: '',
  refreshInterval: null,
  timeInterval: null,
  blinkCount: 5,
};

// ── Utils ──────────────────────────────────────────────────
function encodeAuth(user, pass) {
  return 'Basic ' + btoa(`${user}:${pass}`);
}

function buildUrl(path) {
  const host = state.host.trim();
  const proto = host.startsWith('http') ? '' : 'http://';
  return `${proto}${host}${path}`;
}

async function apiFetch(path, opts = {}) {
  const url = buildUrl(path);
  const headers = { ...opts.headers };
  if (opts.auth) headers['Authorization'] = state.authHeader;

  const fetchOpts = { method: opts.method || 'GET', headers };
  if (opts.body) {
    fetchOpts.body = opts.body;
    headers['Content-Type'] = 'application/x-www-form-urlencoded';
  }

  const res = await fetch(url, fetchOpts);
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  const ct = res.headers.get('content-type') || '';
  if (ct.includes('json')) return res.json();
  return res.text();
}

function $(id) { return document.getElementById(id); }

function setHtml(id, html) {
  const el = $(id);
  if (el) el.innerHTML = html;
}

function setText(id, text) {
  const el = $(id);
  if (el) el.textContent = text;
}

function showResult(id, msg, type = 'success') {
  const el = $(id);
  if (!el) return;
  el.className = `action-result ${type}`;
  el.textContent = typeof msg === 'object' ? JSON.stringify(msg, null, 2) : msg;
  el.classList.remove('hidden');
}

function hideResult(id) {
  const el = $(id);
  if (el) el.classList.add('hidden');
}

// ── Toast ──────────────────────────────────────────────────
function toast(msg, type = 'success', duration = 3000) {
  const icons = { success: '✓', error: '✕', warning: '⚠' };
  const container = $('toast-container');
  const t = document.createElement('div');
  t.className = `toast ${type}`;
  t.innerHTML = `<span class="toast-icon">${icons[type] || '·'}</span><span>${msg}</span>`;
  container.appendChild(t);
  setTimeout(() => {
    t.classList.add('out');
    setTimeout(() => t.remove(), 220);
  }, duration);
}

// ── Noise Canvas ───────────────────────────────────────────
(function initNoise() {
  const canvas = $('noise-canvas');
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  let animId;

  function resize() {
    canvas.width = window.innerWidth;
    canvas.height = window.innerHeight;
  }

  function drawNoise() {
    const w = canvas.width, h = canvas.height;
    const imageData = ctx.createImageData(w, h);
    const data = imageData.data;
    for (let i = 0; i < data.length; i += 4) {
      const v = Math.random() * 255 | 0;
      data[i] = data[i+1] = data[i+2] = v;
      data[i+3] = 255;
    }
    ctx.putImageData(imageData, 0, 0);
    animId = requestAnimationFrame(drawNoise);
  }

  resize();
  window.addEventListener('resize', resize);
  drawNoise();
})();

// ── Screen Switch ──────────────────────────────────────────
function showScreen(id) {
  document.querySelectorAll('.screen').forEach(s => s.classList.remove('active'));
  const s = $(id);
  if (s) s.classList.add('active');
}

// ── Connect ────────────────────────────────────────────────
$('connect-form').addEventListener('submit', async (e) => {
  e.preventDefault();
  const ip   = $('input-ip').value.trim();
  const pass = $('input-pass').value;

  if (!ip) { showConnectError('Enter a device IP or hostname.'); return; }

  state.host = ip;
  state.pass = pass || 'admin';
  state.authHeader = encodeAuth('admin', state.pass);

  const btn = $('btn-connect');
  btn.classList.add('loading');
  btn.disabled = true;
  hideConnectError();

  try {
    const info = await apiFetch('/api/info');
    // Connected!
    state.host = ip;
    initDashboard(info);
    showScreen('dashboard-screen');
    toast('Connection established', 'success');
  } catch (err) {
    showConnectError(`Cannot reach ${ip} — ${err.message}. Check IP and CORS.`);
  } finally {
    btn.classList.remove('loading');
    btn.disabled = false;
  }
});

function showConnectError(msg) {
  $('connect-error-msg').textContent = msg;
  $('connect-error').classList.remove('hidden');
}

function hideConnectError() {
  $('connect-error').classList.add('hidden');
}

// ── Disconnect ─────────────────────────────────────────────
$('btn-disconnect').addEventListener('click', () => {
  clearInterval(state.refreshInterval);
  clearInterval(state.timeInterval);
  state.refreshInterval = null;
  state.timeInterval = null;
  showScreen('connect-screen');
  toast('Disconnected', 'warning');
});

// ── Dashboard Init ─────────────────────────────────────────
function initDashboard(info) {
  setText('topbar-host', state.host);
  const pill = $('status-pill');
  pill.classList.add('online');
  setText('status-text', 'ONLINE');

  updateInfoCard(info);
  loadTime();
  loadGpioInfo();

  // Auto-refresh every 30s
  clearInterval(state.refreshInterval);
  state.refreshInterval = setInterval(refreshAll, 30000);

  // Sync clock on load
  loadTime();
  // Refresh clock every 10 minutes
  clearInterval(state.timeInterval);
  state.timeInterval = setInterval(loadTime, 600000);
}

async function refreshAll() {
  try {
    const info = await apiFetch('/api/info');
    updateInfoCard(info);
    toast('Refreshed', 'success', 1500);
  } catch {
    toast('Refresh failed', 'error');
    $('status-pill').classList.remove('online');
    setText('status-text', 'OFFLINE');
  }
}

$('btn-refresh').addEventListener('click', refreshAll);

// ── Info ───────────────────────────────────────────────────
function updateInfoCard(info) {
  setText('info-mac',     info.mac     || '—');
  setText('info-ssid',    info.ssid    || '—');
  setText('info-rssi',    info.rssi    ? `${info.rssi} dBm` : '—');
  if (info.rssi != null) {
    // 0=weak(0%), -100=strong(100%) per user note
    const rssiPct = Math.max(0, Math.min(100, Math.round(-info.rssi)));
    setText('info-rssi-pct', rssiPct + '%');
  } else {
    setText('info-rssi-pct', '—');
  }
  setText('info-ddns-ip', info.ddns_ip || '—');
  setText('info-local-ip', info.local_ip || '—');
  setText('info-mdns', info.mdns || '—');
  setText('info-ddns-host', info.ddns || '—');
  setText('info-uptime',  info.uptime  ? formatUptime(info.uptime) : '—');
  if (info.free_heap != null) {
    const kb = Math.round(info.free_heap / 1024);
    setText('info-heap-pct', kb + ' KB');
  } else {
    setText('info-heap-pct', '—');
  }
  setText('rssi-badge',   info.rssi    ? `${info.rssi} dBm` : '—');

  // DDNS status comparison
  if (info.ddns_ip && info.public_ip) {
    updateDdnsStatus(info.ddns_ip, info.public_ip);
  } else if (info.ddns_ip) {
    setText('ddns-domain-ip', info.ddns_ip);
    setText('ddns-public-ip', '—');
    setText('ddns-state', 'UNKNOWN');
  }
}

function updateDdnsStatus(domainIp, publicIp) {
  setText('ddns-domain-ip', domainIp);
  setText('ddns-public-ip', publicIp);
  const ind = $('ddns-indicator');
  const icon = ind.querySelector('.ddns-icon');
  if (domainIp === publicIp) {
    ind.className = 'ddns-indicator ok';
    icon.textContent = '✓';
    setText('ddns-state', 'IN SYNC');
  } else {
    ind.className = 'ddns-indicator mismatch';
    icon.textContent = '!';
    setText('ddns-state', 'MISMATCH');
  }
}

function formatUptime(seconds) {
  const s = parseInt(seconds);
  if (isNaN(s)) return seconds;
  const h = Math.floor(s / 3600);
  const m = Math.floor((s % 3600) / 60);
  const sec = s % 60;
  return `${h}h ${m}m ${sec}s`;
}

// ── Time ───────────────────────────────────────────────────
async function loadTime() {
  try {
    const data = await apiFetch('/api/time');
    const timeStr = data.time || data;
    if (timeStr && timeStr !== '--') {
      const parts = timeStr.toString().split(' ');
      const date = parts[0] || '';
      const time = parts[1] || timeStr;
      setText('device-time', time);
      setText('device-date', date);
      setText('topbar-time', time);
    }
  } catch {
    // Silently fail for time
  }
}

// ── Tabs ───────────────────────────────────────────────────
document.querySelectorAll('.tab-btn').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
    document.querySelectorAll('.tab-pane').forEach(p => p.classList.remove('active'));
    btn.classList.add('active');
    $(`tab-${btn.dataset.tab}`).classList.add('active');

    // Lazy-load tab data
    if (btn.dataset.tab === 'gpio') loadGpioInfo();
    if (btn.dataset.tab === 'log') loadLog();
  });
});

// ── DDNS Tab ───────────────────────────────────────────────
$('btn-ddns-update').addEventListener('click', async () => {
  const btn = $('btn-ddns-update');
  btn.disabled = true;
  btn.textContent = '⟳ UPDATING…';
  try {
    const data = await apiFetch('/api/ddns/update', { auth: true });
    showResult('ddns-update-result', `Status: ${data.status} · Response: ${data.response}`, 'success');
    toast('DDNS update sent', 'success');
  } catch (err) {
    showResult('ddns-update-result', `Error: ${err.message}`, 'error');
    toast('DDNS update failed', 'error');
  } finally {
    btn.disabled = false;
    btn.innerHTML = '<span>⟳</span> FORCE UPDATE';
  }
});

$('btn-resolve').addEventListener('click', async () => {
  const host = $('resolve-host').value.trim();
  if (!host) { toast('Enter a hostname', 'warning'); return; }
  hideResult('resolve-result');
  try {
    const data = await apiFetch(`/api/resolve?host=${encodeURIComponent(host)}`);
    if (data.error) {
      showResult('resolve-result', `DNS failed for: ${host}`, 'error');
    } else {
      showResult('resolve-result', `${data.host} → ${data.ip}`, 'success');
    }
  } catch (err) {
    showResult('resolve-result', `Error: ${err.message}`, 'error');
  }
});

// ── GPIO Tab ───────────────────────────────────────────────
async function loadGpioInfo() {
  const grid = $('gpio-grid');
  grid.innerHTML = '<div class="gpio-loading">Loading pins…</div>';
  try {
    const data = await apiFetch('/api/gpio/info');
    const pins = data.pins || [];
    grid.innerHTML = '';
    for (const p of pins) {
      const div = document.createElement('div');
      div.className = `gpio-pin ${p.available ? 'available' : 'unavailable'}`;
      div.innerHTML = `<span class="gpio-pin-num">${p.pin}</span><span class="gpio-pin-state">${p.available ? 'AVAIL' : 'N/A'}</span>`;
      if (p.available) {
        div.title = `GPIO ${p.pin} — Click to read`;
        div.addEventListener('click', () => {
          $('gpio-pin-sel').value = p.pin;
          readGpioPin(p.pin);
        });
      }
      grid.appendChild(div);
    }
  } catch (err) {
    grid.innerHTML = `<div class="gpio-loading" style="color:var(--red)">Failed: ${err.message}</div>`;
  }
}

$('btn-gpio-refresh').addEventListener('click', loadGpioInfo);

async function readGpioPin(pin) {
  try {
    const p = pin !== undefined ? pin : parseInt($('gpio-pin-sel').value);
    const data = await apiFetch(`/api/gpio/read?pin=${p}`);
    showResult('gpio-result', `GPIO ${data.pin}: ${data.value === 1 ? 'HIGH (1)' : 'LOW (0)'}`, 'success');
  } catch (err) {
    showResult('gpio-result', `Error: ${err.message}`, 'error');
  }
}

$('btn-gpio-read').addEventListener('click', () => readGpioPin());

$('gpio-mode-sel').addEventListener('change', () => {
  const valField = $('gpio-value-field');
  valField.style.display = $('gpio-mode-sel').value === 'output' ? '' : 'none';
});

$('btn-gpio-set').addEventListener('click', async () => {
  const pin   = $('gpio-pin-sel').value;
  const mode  = $('gpio-mode-sel').value;
  const value = $('gpio-value-sel').value;
  const body = new URLSearchParams({ pin, mode });
  if (mode === 'output') body.append('value', value);

  try {
    const data = await apiFetch('/api/gpio/set', { method: 'POST', auth: true, body: body.toString() });
    showResult('gpio-result', `GPIO ${data.pin}: mode=${data.mode}${data.value !== undefined ? ', value=' + data.value : ''}`, 'success');
    toast(`GPIO ${pin} configured`, 'success');
  } catch (err) {
    showResult('gpio-result', `Error: ${err.message}`, 'error');
    toast('GPIO set failed', 'error');
  }
});

// ── LED Tab ────────────────────────────────────────────────
async function setLedState(state_) {
  try {
    const data = await apiFetch(`/api/led/${state_}`, { auth: true });
    const bulb = $('led-bulb');
    const label = $('led-state-label');
    const isOn = data.state === 'on';
    bulb.className = `led-bulb ${isOn ? 'on' : 'off'}`;
    label.className = `led-label ${isOn ? 'on' : 'off'}`;
    setText('led-state-label', isOn ? 'ON' : 'OFF');
    setText('led-pin-display', data.led_pin !== undefined ? data.led_pin : '—');
    toast(`LED turned ${data.state}`, 'success');
  } catch (err) {
    toast(`LED error: ${err.message}`, 'error');
  }
}

$('btn-led-on').addEventListener('click',  () => setLedState('on'));
$('btn-led-off').addEventListener('click', () => setLedState('off'));

// Blink counter
$('blink-dec').addEventListener('click', () => {
  if (state.blinkCount > 1) setText('blink-count', --state.blinkCount);
});
$('blink-inc').addEventListener('click', () => {
  if (state.blinkCount < 100) setText('blink-count', ++state.blinkCount);
});

$('btn-blink').addEventListener('click', async () => {
  try {
    const data = await apiFetch(`/api/led/blink?times=${state.blinkCount}`, { auth: true });
    showResult('blink-result', `Blinked ${data.blinked} times`, 'success');
    toast(`LED blinked ×${data.blinked}`, 'success');
  } catch (err) {
    showResult('blink-result', `Error: ${err.message}`, 'error');
    toast('Blink failed', 'error');
  }
});

$('btn-led-pin').addEventListener('click', async () => {
  const pin = $('led-pin-sel').value;
  const body = new URLSearchParams({ pin }).toString();
  try {
    const data = await apiFetch('/api/led/pin', { method: 'POST', auth: true, body });
    showResult('led-pin-result', `LED pin saved: GPIO ${data.led_pin}`, 'success');
    setText('led-pin-display', data.led_pin);
    toast(`LED pin set to GPIO ${data.led_pin}`, 'success');
  } catch (err) {
    showResult('led-pin-result', `Error: ${err.message}`, 'error');
    toast('Pin change failed', 'error');
  }
});

// ── WiFi Tab ───────────────────────────────────────────────
$('btn-save-ssid').addEventListener('click', async () => {
  const ssid = $('wifi-ssid').value.trim();
  if (!ssid) { toast('Enter an SSID', 'warning'); return; }
  const body = new URLSearchParams({ ssid }).toString();
  try {
    const data = await apiFetch('/api/set/ssid', { method: 'POST', auth: true, body });
    showResult('wifi-result', `SSID saved: "${ssid}". Device will reconnect.`, 'success');
    toast('SSID saved', 'success');
  } catch (err) {
    showResult('wifi-result', `Error: ${err.message}`, 'error');
    toast('SSID save failed', 'error');
  }
});

$('btn-save-pswd').addEventListener('click', async () => {
  const pswd = $('wifi-pswd').value;
  if (!pswd) { toast('Enter a password', 'warning'); return; }
  const body = new URLSearchParams({ pswd }).toString();
  try {
    const data = await apiFetch('/api/set/pswd', { method: 'POST', auth: true, body });
    showResult('wifi-result', 'WiFi password saved.', 'success');
    toast('Password saved', 'success');
  } catch (err) {
    showResult('wifi-result', `Error: ${err.message}`, 'error');
    toast('Password save failed', 'error');
  }
});

// ── Tools Tab ──────────────────────────────────────────────
$('btn-curl').addEventListener('click', async () => {
  const url = $('curl-url').value.trim();
  if (!url) { toast('Enter a URL', 'warning'); return; }
  const result = $('curl-result');
  result.textContent = 'Fetching…';
  result.classList.remove('hidden');

  try {
    const data = await apiFetch(`/api/curl?url=${encodeURIComponent(url)}`, { auth: true });
    result.textContent = data;
    toast('Fetch complete', 'success');
  } catch (err) {
    result.textContent = `Error: ${err.message}`;
    toast('Fetch failed', 'error');
  }
});

$('btn-api-help').addEventListener('click', async () => {
  const list = $('api-list');
  list.innerHTML = '<div style="color:var(--text-dim);font-size:.75rem">Loading…</div>';
  list.classList.remove('hidden');

  try {
    const data = await apiFetch('/api/help');
    const endpoints = data.endpoints || [];
    list.innerHTML = '';
    for (const ep of endpoints) {
      const div = document.createElement('div');
      div.className = 'api-endpoint';
      const isPost = ep.toString().includes('POST') || ep.method === 'POST';
      const path = typeof ep === 'string' ? ep : ep.path || ep;
      div.innerHTML = `<span class="api-method">${isPost ? 'POST' : 'GET'}</span><span>${path}</span>`;
      list.appendChild(div);
    }
    if (!endpoints.length) list.innerHTML = '<div style="color:var(--text-dim);font-size:.75rem">No endpoints returned.</div>';
    toast(`${endpoints.length} endpoints loaded`, 'success');
  } catch (err) {
    list.innerHTML = `<div style="color:var(--red);font-size:.75rem">Error: ${err.message}</div>`;
    toast('API help failed', 'error');
  }
});

// ── Log Tab ────────────────────────────────────────────────
async function loadLog() {
  const container = $('log-container');
  container.innerHTML = '<div class="log-empty">Loading…</div>';
  try {
    const data = await apiFetch('/api/log', { auth: true });
    const entries = Array.isArray(data) ? data : [];
    setText('log-count', `${entries.length} entries`);
    if (!entries.length) {
      container.innerHTML = '<div class="log-empty">Log is empty.</div>';
      return;
    }
    container.innerHTML = '';
    for (const entry of entries.slice().reverse()) {
      const div = document.createElement('div');
      div.className = 'log-entry';
      const timeStr = entry.time ? new Date(entry.time * 1000).toLocaleTimeString() : '—';
      div.innerHTML = `<span class="log-time">${timeStr}</span><span class="log-msg">${escapeHtml(entry.msg || '')}</span>`;
      container.appendChild(div);
    }
  } catch (err) {
    container.innerHTML = `<div class="log-empty" style="color:var(--red)">Error: ${err.message}</div>`;
    toast('Log fetch failed', 'error');
  }
}

$('btn-log-refresh').addEventListener('click', loadLog);

function escapeHtml(str) {
  return str.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}
