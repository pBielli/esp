/* ============================================================
   DDNS Monitor — Application Logic
   ============================================================ */

// ── State ──────────────────────────────────────────────────
const state = {
  host: '',
  pass: '',
  authHeader: '',
  refreshInterval: null,
  clockInterval: null,
  blinkCount: 5,
  deviceEpoch: 0,
  tzOffset: 0,
  lastTimeSync: 0,
  bootEpoch: null,
};

let savedSsidList = [];

const ESP8266_PIN_LABELS = {
  0: 'D3', 1: 'TX', 2: 'D4', 3: 'RX', 4: 'D2', 5: 'D1',
  9: 'SD2', 10: 'SD3', 12: 'D6', 13: 'D7', 14: 'D5', 15: 'D8', 16: 'D0'
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
    const info = await apiFetch('/api/status');
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
  clearInterval(state.clockInterval);
  state.refreshInterval = null;
  state.clockInterval = null;
  showScreen('connect-screen');
  toast('Disconnected', 'warning');
});

// ── Dashboard Init ─────────────────────────────────────────
function initDashboard(info) {
  setText('topbar-host', state.host);
  const pill = $('status-pill');
  pill.classList.add('online');
  setText('status-text', 'ONLINE');
  $('device-time').classList.remove('dimmed');
  $('info-uptime').classList.remove('dimmed');

  updateInfoCard(info);
  loadGpioInfo();

  // Auto-refresh every 30s
  clearInterval(state.refreshInterval);
  state.refreshInterval = setInterval(refreshAll, 30000);

  // Initial time sync
  loadTime();
  // Start local clock tick
  clearInterval(state.clockInterval);
  state.clockInterval = setInterval(tickClock, 1000);
}

async function refreshAll() {
  try {
    const info = await apiFetch('/api/info');
    updateInfoCard(info);
    const timeData = await apiFetch('/api/ntp/time');
    if (timeData.epoch && timeData.tz_offset != null) {
      state.deviceEpoch = timeData.epoch;
      state.tzOffset = timeData.tz_offset;
      state.lastTimeSync = Date.now();
      if (state.lastUptime != null) state.bootEpoch = timeData.epoch - state.lastUptime;
    }
    $('status-pill').classList.add('online');
    setText('status-text', 'ONLINE');
    $('device-time').classList.remove('dimmed');
    $('info-uptime').classList.remove('dimmed');
    toast('Refreshed', 'success', 1500);
  } catch {
    toast('Refresh failed', 'error');
    $('status-pill').classList.remove('online');
    setText('status-text', 'OFFLINE');
    $('device-time').classList.add('dimmed');
    $('info-uptime').classList.add('dimmed');
  }
}

$('btn-refresh').addEventListener('click', refreshAll);

// ── Info ───────────────────────────────────────────────────
function updateInfoCard(info) {
  // SYSTEM card
    setText('info-fw-version', info.firmware || info.version || '—');
  setText('info-mac',       info.mac         || '—');
  if (info.uptime) {
    state.lastUptime = info.uptime;
    setText('info-uptime', formatUptime(info.uptime));
  } else {
    setText('info-uptime', '—');
  }
  if (info.free_heap != null) {
    const kb = Math.round(info.free_heap / 1024);
    setText('info-heap-pct', kb + ' KB');
  } else {
    setText('info-heap-pct', '—');
  }
  setText('info-local-ip',  info.local_ip    || '—');
  setText('info-ip-mode',   info.use_static_ip ? 'STATIC' : 'DHCP');

  // NETWORK card
  setText('info-ssid',      info.ssid        || '—');
  setText('info-bssid',     info.bssid       || '—');
  setText('info-channel',   info.channel     != null ? String(info.channel) : '—');
  setText('info-phy',       info.phy_mode    || '—');
  setText('info-rssi',      info.rssi        ? `${info.rssi} dBm` : '—');
  if (info.rssi != null) {
    const rssiPct = Math.max(0, Math.min(100, Math.round(((info.rssi + 100) / 60) * 100)));
    setText('info-rssi-pct', rssiPct + '%');
  } else {
    setText('info-rssi-pct', '—');
  }
  setText('info-gateway',   info.gateway     || '—');
  setText('info-subnet',    info.subnet      || '—');
  setText('info-dns1',      info.dns1        || '—');
  setText('info-dns2',      info.dns2        || '—');
  setText('info-mdns',     info.mdns        || '—');

  // CONFIG card
  setText('info-ntp-server', info.ntp_server || '—');
  if (info.tz_offset != null) {
    const hours = info.tz_offset / 3600;
    setText('info-tz', `UTC${hours >= 0 ? '+' : ''}${hours}h`);
  } else {
    setText('info-tz', '—');
  }
  setText('info-fw-build',  info.build_date  || '—');
  setText('info-gpio-invert', info.gpio_invert ? 'YES' : 'NO');
  setText('info-led-pin',   info.led_pin != null ? `GPIO ${info.led_pin}` : '—');
  setText('info-pwm-pin',   info.pwm_pin != null ? `GPIO ${info.pwm_pin}` : '—');

  // LED bulb
  const bulb = $('led-bulb');
  const stateLabel = $('led-state-label');
  if (bulb && stateLabel) {
    const isMatch = info.ddns_ip && info.public_ip && info.ddns_ip === info.public_ip;
    bulb.className = 'led-bulb ' + (isMatch ? 'on' : 'off');
    stateLabel.textContent = isMatch ? 'IN SYNC' : (info.ddns_ip ? 'MISMATCH' : 'UNKNOWN');
  }

  // DDNS status comparison
  setText('ddns-domain-ip', info.ddns_ip     || '—');
  setText('info-ddns-host', info.ddns        || '—');
  if (info.ddns_ip && info.public_ip) {
    updateDdnsStatus(info.ddns_ip, info.public_ip);
  } else if (info.ddns_ip) {
    setText('ddns-domain-ip', info.ddns_ip);
    setText('ddns-public-ip', '—');
    setText('ddns-state', 'UNKNOWN');
  }
  // Last DDNS check info
  if (info.last_check_elapsed_ms != null && info.last_check_elapsed_ms >= 0) {
    const secs = Math.floor(info.last_check_elapsed_ms / 1000);
    setText('ddns-last-check', formatUptime(secs) + ' ago');
  } else {
    setText('ddns-last-check', '—');
  }
  updateLedInvertUI(info.gpio_invert === 1);
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

// ── DDNS Check (new behavior) ─────────────────────────────
$('btn-ddns-check').addEventListener('click', async () => {
  const block = $('ddns-check-result-block');
  const result = $('ddns-check-result');
  const forceBtn = $('btn-ddns-force-update');
  block.classList.remove('hidden');
  forceBtn.classList.add('hidden');
  result.className = 'action-result';
  result.textContent = '⟳ Checking...';
  setText('ddns-domain-ip', '⟳');
  setText('ddns-public-ip', '⟳');
  try {
    const data = await apiFetch('/api/ddns/check', { method: 'POST', auth: true });
    setText('ddns-domain-ip', data.ddns_ip || '—');
    setText('ddns-public-ip', data.public_ip || '—');
    const ind = $('ddns-indicator');
    const icon = ind.querySelector('.ddns-icon');
    if (data.match) {
      ind.className = 'ddns-indicator ok';
      icon.textContent = '✓';
      setText('ddns-state', 'IN SYNC');
      result.className = 'action-result success';
      result.textContent = `IPs match: ${data.public_ip}`;
    } else {
      ind.className = 'ddns-indicator mismatch';
      icon.textContent = '!';
      setText('ddns-state', 'MISMATCH');
      result.className = 'action-result error';
      result.textContent = `DDNS: ${data.ddns_ip || '—'} ≠ Public: ${data.public_ip || '—'}`;
      forceBtn.classList.remove('hidden');
    }
    toast('DDNS check done', 'success');
    setTimeout(refreshAll, 2000);
  } catch (err) {
    result.className = 'action-result error';
    result.textContent = `Check failed: ${err.message}`;
    toast('DDNS check failed', 'error');
  }
});

$('btn-ddns-force-update').addEventListener('click', async () => {
  const result = $('ddns-check-result');
  result.textContent = '⟳ Updating...';
  try {
    const data = await apiFetch('/api/ddns/update', { auth: true });
    result.className = 'action-result success';
    result.textContent = `Update sent: ${data.status}`;
    toast('DDNS update sent', 'success');
    setTimeout(refreshAll, 2000);
  } catch (err) {
    result.className = 'action-result error';
    result.textContent = `Update failed: ${err.message}`;
    toast('DDNS update failed', 'error');
  }
});

// ── Connectivity Check ─────────────────────────────────────
$('btn-conn-check').addEventListener('click', async () => {
  const gwEl = $('conn-gateway');
  const inetEl = $('conn-internet');
  gwEl.textContent = '⟳';
  inetEl.textContent = '⟳';
  try {
    const netStatus = await apiFetch('/api/network/status');
    const gateway = netStatus.gateway || '8.8.8.8';
    const [gwRes, inetRes] = await Promise.all([
      apiFetch('/api/ping', { method: 'POST', auth: true, body: 'host=' + encodeURIComponent(gateway) }),
      apiFetch('/api/ping', { method: 'POST', auth: true, body: 'host=8.8.8.8' })
    ]);
    gwEl.textContent = gwRes.success ? `✓ ${gwRes.rtt_ms}ms` : '✕ FAIL';
    inetEl.textContent = inetRes.success ? `✓ ${inetRes.rtt_ms}ms` : '✕ FAIL';
    toast('Connectivity check done', 'success');
  } catch (err) {
    gwEl.textContent = 'ERR';
    inetEl.textContent = 'ERR';
    toast('Connectivity check failed', 'error');
  }
});

// ── Time ───────────────────────────────────────────────────
function formatTimeFromEpoch(epoch) {
  const d = new Date((epoch + state.tzOffset) * 1000);
  const pad = n => String(n).padStart(2, '0');
  const time = `${pad(d.getUTCHours())}:${pad(d.getUTCMinutes())}:${pad(d.getUTCSeconds())}`;
  const date = `${d.getUTCFullYear()}-${pad(d.getUTCMonth() + 1)}-${pad(d.getUTCDate())}`;
  return { time, date };
}

async function loadTime() {
  try {
    const data = await apiFetch('/api/ntp/time');
    if (data.epoch && data.tz_offset != null) {
      state.deviceEpoch = data.epoch;
      state.tzOffset = data.tz_offset;
      state.lastTimeSync = Date.now();
      if (state.lastUptime != null) {
        state.bootEpoch = data.epoch - state.lastUptime;
      }
      const { time, date } = formatTimeFromEpoch(data.epoch);
      setText('device-time', time);
      setText('device-date', date);
      setText('topbar-time', time);
    }
  } catch {
    // Silently fail for time
  }
}

function tickClock() {
  if (!state.lastTimeSync || !state.deviceEpoch) return;
  const pill = $('status-pill');
  if (!pill.classList.contains('online')) return;
  const elapsed = Math.floor((Date.now() - state.lastTimeSync) / 1000);
  const current = state.deviceEpoch + elapsed;
  const { time, date } = formatTimeFromEpoch(current);
  setText('device-time', time);
  setText('device-date', date);
  setText('topbar-time', time);
  if (state.bootEpoch != null) {
    const uptime = current - state.bootEpoch;
    setText('info-uptime', formatUptime(uptime));
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
    if (btn.dataset.tab === 'gpio') { loadGpioInfo(); loadGpioInvert(); }
    if (btn.dataset.tab === 'log') loadLog();
    if (btn.dataset.tab === 'ddns') { loadDdnsConfig(); }
    if (btn.dataset.tab === 'network') loadNetworkTab();
    if (btn.dataset.tab === 'system') loadOtaConfig();
    if (btn.dataset.tab === 'api') loadApiExplorer();
  });
});

// ── Sub-Tab Navigation ────────────────────────────────────
document.addEventListener('click', (e) => {
  const btn = e.target.closest('.sub-tab-btn');
  if (btn) {
    document.querySelectorAll('.sub-tab-btn').forEach(b => b.classList.remove('active'));
    document.querySelectorAll('.sub-tab-pane').forEach(p => p.classList.remove('active'));
    btn.classList.add('active');
    const pane = document.getElementById(`subtab-${btn.dataset.subtab}`);
    if (pane) pane.classList.add('active');
  }
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

// ── DDNS Config ────────────────────────────────────────────
async function loadDdnsConfig() {
  try {
    const info = await apiFetch('/api/info');
    if (info.ddns) $('ddns-hostname').value = info.ddns;
    if (info.ddns) $('ddns-hostname').placeholder = info.ddns;
    if (info.ddns_domain) $('ddns-domain').value = info.ddns_domain;
    if (info.ddns_upd_url) $('ddns-upd-url').value = info.ddns_upd_url;
    if (info.ddns_check_interval != null) $('ddns-interval').value = info.ddns_check_interval;
    if (info.public_ip_urls) {
      $('ddns-ipurls').value = info.public_ip_urls;
      const sel = $('ipcheck-server');
      sel.innerHTML = '';
      info.public_ip_urls.split(',').forEach((url, i) => {
        sel.options[i] = new Option(url || `Server ${i+1}`, i);
      });
    }
  } catch {}
}

$('btn-ddns-config-save').addEventListener('click', async () => {
  const hostname = $('ddns-hostname').value.trim();
  const domain   = $('ddns-domain').value.trim();
  const token    = $('ddns-token').value.trim();
  const upd_url  = $('ddns-upd-url').value.trim();
  if (!hostname && !domain && !token && !upd_url) { toast('Fill at least one field', 'warning'); return; }
  const body = new URLSearchParams();
  if (hostname) body.append('hostname', hostname);
  if (domain)   body.append('domain', domain);
  if (token)    body.append('token', token);
  if (upd_url)  body.append('upd_url', upd_url);
  try {
    const data = await apiFetch('/api/ddns/config', { method: 'POST', auth: true, body: body.toString() });
    showResult('ddns-config-result', `DDNS saved: ${data.ddns_hostname}`, 'success');
    toast('DDNS config saved', 'success');
  } catch (err) {
    showResult('ddns-config-result', `Error: ${err.message}`, 'error');
    toast('DDNS config failed', 'error');
  }
});

// ── DDNS IP URLs ──────────────────────────────────────────
$('btn-ddns-ipurls-save').addEventListener('click', async () => {
  const urls = $('ddns-ipurls').value.trim();
  if (!urls) { toast('Enter at least one server URL', 'warning'); return; }
  const body = new URLSearchParams({ urls }).toString();
  try {
    const data = await apiFetch('/api/ddns/ip-urls', { method: 'POST', auth: true, body });
    const urls = data.urls || data.public_ip_urls || '';
    $('ddns-ipurls').value = urls;
    const sel = $('ipcheck-server');
    sel.innerHTML = '';
    urls.split(',').forEach((url, i) => {
      sel.options[i] = new Option(url || `Server ${i+1}`, i);
    });
    showResult('ddns-ipurls-result', `IP URLs saved: ${urls}`, 'success');
    toast('IP URLs saved', 'success');
  } catch (err) {
    showResult('ddns-ipurls-result', `Error: ${err.message}`, 'error');
    toast('IP URLs save failed', 'error');
  }
});

// ── Public IP Check ──────────────────────────────────────
$('btn-ipcheck').addEventListener('click', async () => {
  const idx = $('ipcheck-server').value;
  const btn = $('btn-ipcheck');
  btn.disabled = true;
  btn.textContent = '⟳ CHECKING…';
  try {
    const data = await apiFetch('/api/network/public-ip/check', { method: 'POST', auth: true, body: 'idx=' + idx });
    const cls = data.success ? 'success' : 'error';
    const serverName = data.server || '—';
    const ipText = data.ip || 'FAILED';
    showResult('ipcheck-result',
      `Server: ${serverName} → ${ipText} [${data.success ? 'OK' : 'FAIL'}]`,
      cls);
    if (data.success) toast('Public IP: ' + data.ip, 'success');
    else toast('Public IP check failed', 'error');
  } catch (err) {
    showResult('ipcheck-result', `Error: ${err.message}`, 'error');
    toast('IP check failed', 'error');
  } finally {
    btn.disabled = false;
    btn.innerHTML = 'CHECK IP';
  }
});

// ── DDNS Interval ──────────────────────────────────────────
$('btn-ddns-interval-save').addEventListener('click', async () => {
  const interval = parseInt($('ddns-interval').value);
  if (!interval || interval < 30) { toast('Minimum interval is 30 seconds', 'warning'); return; }
  const body = new URLSearchParams({ interval }).toString();
  try {
    const data = await apiFetch('/api/ddns/interval', { method: 'POST', auth: true, body });
    showResult('ddns-interval-result', `Check interval saved: ${data.ddns_check_interval}s`, 'success');
    toast('DDNS interval saved', 'success');
  } catch (err) {
    showResult('ddns-interval-result', `Error: ${err.message}`, 'error');
    toast('DDNS interval failed', 'error');
  }
});

$('btn-resolve').addEventListener('click', async () => {
  const host = $('resolve-host').value.trim();
  if (!host) { toast('Enter a hostname', 'warning'); return; }
  hideResult('resolve-result');
  try {
    const data = await apiFetch('/api/dns/resolve', { method: 'POST', auth: true, body: 'host=' + encodeURIComponent(host) });
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
    const data = await apiFetch('/api/gpio', { auth: true });
    const pins = data.pins || [];
    grid.innerHTML = '';
    for (const p of pins) {
      const div = document.createElement('div');
      const valueClass = p.available && p.value !== undefined ? (p.value === 1 ? 'high' : 'low') : '';
      div.className = `gpio-pin ${p.available ? 'available' : 'unavailable'} ${valueClass}`;
      const label = ESP8266_PIN_LABELS[p.pin] || '';
      const stateText = p.available ? (p.value !== undefined ? `V: ${p.value}` : 'AVAIL') : 'N/A';
      div.innerHTML = `<span class="gpio-pin-num">GPIO ${p.pin}</span>${label ? `<span class="gpio-pin-label">${label}</span>` : ''}<span class="gpio-pin-state">${stateText}</span>`;
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

// ── GPIO Live Refresh ──────────────────────────────────────
let gpioLiveInterval = null;

$('gpio-live-toggle').addEventListener('click', () => {
  const toggle = $('gpio-live-toggle');
  toggle.classList.toggle('active');
  if (toggle.classList.contains('active')) {
    const interval = parseInt($('gpio-live-interval').value) || 5;
    gpioLiveInterval = setInterval(loadGpioInfo, Math.max(1, interval) * 1000);
    toast(`GPIO live refresh ON (${interval}s)`, 'success');
  } else {
    clearInterval(gpioLiveInterval);
    gpioLiveInterval = null;
    toast('GPIO live refresh OFF', 'warning');
  }
});

async function readGpioPin(pin) {
  try {
    const p = pin !== undefined ? pin : parseInt($('gpio-pin-sel').value);
    const data = await apiFetch(`/api/gpio/read?pin=${p}`, { auth: true });
    showResult('gpio-result', `GPIO ${data.pin}: ${data.value === 1 ? 'HIGH (1)' : 'LOW (0)'}`, 'success');
  } catch (err) {
    showResult('gpio-result', `Error: ${err.message}`, 'error');
  }
}

$('btn-gpio-read').addEventListener('click', () => readGpioPin());

$('gpio-mode-sel').addEventListener('change', () => {
  const isOutput = $('gpio-mode-sel').value === 'output';
  $('gpio-value-field').style.display = isOutput ? '' : 'none';
  $('gpio-pwm-row').classList.toggle('hidden', !isOutput);
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

// ── GPIO Pulse/Toggle ──────────────────────────────────────
$('btn-gpio-pulse').addEventListener('click', async () => {
  const pin = $('gpio-pin-sel').value;
  const body = new URLSearchParams({ pin }).toString();
  try {
    const data = await apiFetch('/api/gpio/pulse', { method: 'POST', auth: true, body });
    showResult('gpio-result', `GPIO ${data.pin} pulsed (${data.pulse_ms || 500}ms)`, 'success');
    toast(`GPIO ${pin} pulsed`, 'success');
  } catch (err) {
    showResult('gpio-result', `Error: ${err.message}`, 'error');
    toast('Pulse failed', 'error');
  }
});

$('btn-gpio-toggle').addEventListener('click', async () => {
  const pin = $('gpio-pin-sel').value;
  const body = new URLSearchParams({ pin }).toString();
  try {
    const data = await apiFetch('/api/gpio/toggle', { method: 'POST', auth: true, body });
    showResult('gpio-result', `GPIO ${data.pin} toggled → ${(data.value ?? data.current) === 1 ? 'HIGH' : 'LOW'}`, 'success');
    toast(`GPIO ${pin} toggled`, 'success');
  } catch (err) {
    showResult('gpio-result', `Error: ${err.message}`, 'error');
    toast('Toggle failed', 'error');
  }
});

// ── Blink counter ──────────────────────────────────────────
$('blink-dec').addEventListener('click', () => {
  if (state.blinkCount > 1) setText('blink-count', --state.blinkCount);
});
$('blink-inc').addEventListener('click', () => {
  if (state.blinkCount < 100) setText('blink-count', ++state.blinkCount);
});

$('btn-blink').addEventListener('click', async () => {
  const pin = $('gpio-pin-sel').value;
  const times = state.blinkCount;
  const ms = parseInt($('blink-ms').value) || 200;
  const body = new URLSearchParams({ pin, times, ms }).toString();
  try {
    const data = await apiFetch('/api/gpio/blink', { method: 'POST', auth: true, body });
    showResult('gpio-result', `GPIO ${data.pin} blinked ${data.blinked}×${data.ms ? ' ('+data.ms+'ms)' : ''}`, 'success');
    toast(`GPIO ${pin} blinked ×${data.blinked}`, 'success');
  } catch (err) {
    showResult('gpio-result', `Error: ${err.message}`, 'error');
    toast('Blink failed', 'error');
  }
});

$('btn-led-pin').addEventListener('click', async () => {
  const pin = $('led-pin-sel').value;
  const body = new URLSearchParams({ pin }).toString();
  try {
    const data = await apiFetch('/api/ddns/led/pin', { method: 'POST', auth: true, body });
    showResult('led-pin-result', `LED pin saved: GPIO ${data.led_pin}`, 'success');
    setText('led-pin-display', data.led_pin);
    toast(`LED pin set to GPIO ${data.led_pin}`, 'success');
  } catch (err) {
    showResult('led-pin-result', `Error: ${err.message}`, 'error');
    toast('Pin change failed', 'error');
  }
});

// ── GPIO Invert ─────────────────────────────────────────────
async function loadGpioInvert() {
  try {
    const info = await apiFetch('/api/gpio/config/invert');
    updateLedInvertUI(info.gpio_invert === 1);
  } catch {}
}

function updateLedInvertUI(enabled) {
  const toggle = $('gpio-invert-toggle');
  if (toggle) toggle.classList.toggle('active', enabled);
}

$('gpio-invert-toggle')?.addEventListener('click', async () => {
  const enabled = $('gpio-invert-toggle').classList.contains('active') ? 0 : 1;
  const body = new URLSearchParams({ enabled }).toString();
  try {
    await apiFetch('/api/gpio/config/invert', { method: 'POST', auth: true, body });
    updateLedInvertUI(enabled === 1);
    toast(`GPIO invert ${enabled ? 'ON' : 'OFF'}`, 'success');
  } catch (err) {
    toast(`Invert failed: ${err.message}`, 'error');
  }
});

// ── Analog (A0 / PWM) ──────────────────────────────────────
$('btn-analog-read').addEventListener('click', async () => {
  try {
    const data = await apiFetch('/api/gpio/analog', { auth: true });
    setText('analog-read-value', data.value != null ? data.value : data.raw);
    toast(`A0: ${data.value} / 100`, 'success');
  } catch (err) {
    toast(`Analog read failed: ${err.message}`, 'error');
  }
});

// ── PWM Slider in GPIO Control ────────────────────────────
$('gpio-pwm-slider').addEventListener('input', () => {
  setText('gpio-pwm-display', $('gpio-pwm-slider').value);
});

$('btn-gpio-pwm-write').addEventListener('click', async () => {
  const pin = $('gpio-pin-sel').value;
  const value = $('gpio-pwm-slider').value;
  const body = new URLSearchParams({ pin, value }).toString();
  try {
    const data = await apiFetch('/api/gpio/analog/write', { method: 'POST', auth: true, body });
    showResult('gpio-result', `PWM GPIO ${data.pin}: ${data.value}`, 'success');
    toast(`PWM GPIO ${pin} = ${data.value}`, 'success');
  } catch (err) {
    showResult('gpio-result', `Error: ${err.message}`, 'error');
    toast('PWM write failed', 'error');
  }
});

// ── NTP Tab ──────────────────────────────────────────────────
$('btn-ntp-save').addEventListener('click', async () => {
  const server = $('ntp-server').value.trim();
  const tz_offset = $('ntp-tz').value.trim();
  if (!server && !tz_offset) { toast('Enter at least one field', 'warning'); return; }
  const body = new URLSearchParams();
  if (server) body.append('server', server);
  if (tz_offset) body.append('tz_offset', tz_offset);
  try {
    const data = await apiFetch('/api/ntp/config', { method: 'POST', auth: true, body: body.toString() });
    showResult('ntp-result', `NTP: ${data.ntp_server}, TZ: ${data.tz_offset}s (UTC${data.tz_offset / 3600 >= 0 ? '+' : ''}${data.tz_offset / 3600}h)`, 'success');
    toast('NTP config saved', 'success');
  } catch (err) {
    showResult('ntp-result', `Error: ${err.message}`, 'error');
    toast('NTP save failed', 'error');
  }
});

// ── WiFi Scan ───────────────────────────────────────────────
let scanPollInterval = null;

$('btn-wifi-scan').addEventListener('click', async () => {
  const statusEl = $('scan-status');
  const resultsEl = $('scan-results');
  statusEl.textContent = 'Scanning...';
  statusEl.className = 'scan-status';
  resultsEl.classList.add('hidden');
  resultsEl.innerHTML = '';

  try {
    const data = await apiFetch('/api/network/wifi/scan', { auth: true });
    if (data.status === 'scanning') {
      statusEl.textContent = 'Scan in progress, polling...';
      if (scanPollInterval) clearInterval(scanPollInterval);
      scanPollInterval = setInterval(pollScan, 2000);
    } else if (data.networks) {
      renderScanResults(data.networks);
    }
  } catch (err) {
    statusEl.textContent = `Scan error: ${err.message}`;
    statusEl.className = 'scan-status error';
  }
});

async function pollScan() {
  try {
    const data = await apiFetch('/api/network/wifi/scan', { auth: true });
    if (data.networks) {
      clearInterval(scanPollInterval);
      scanPollInterval = null;
      renderScanResults(data.networks);
    }
  } catch {
    clearInterval(scanPollInterval);
    scanPollInterval = null;
    $('scan-status').textContent = 'Scan failed';
    $('scan-status').className = 'scan-status error';
  }
}

function renderScanResults(networks) {
  const statusEl = $('scan-status');
  const resultsEl = $('scan-results');
  if (!networks || networks.length === 0) {
    statusEl.textContent = 'No networks found.';
    return;
  }
  const encLabels = ['Open', 'WEP', 'WPA/PSK', 'WPA2/PSK', 'WPA/WPA2'];
  const currentSsid = ($('wifi-ssid').value || $('wifi-ssid').placeholder || '').trim();
  statusEl.textContent = `${networks.length} network(s) found.`;
  statusEl.className = 'scan-status success';
  resultsEl.innerHTML = '';
  resultsEl.classList.remove('hidden');

  const table = document.createElement('table');
  table.className = 'scan-table';
  table.innerHTML = `<thead><tr><th>SSID</th><th>RSSI</th><th>SIGNAL</th><th>CH</th><th>ENC</th><th>STATUS</th></tr></thead>`;
  const tbody = document.createElement('tbody');
  for (const net of networks) {
    const row = document.createElement('tr');
    const enc = encLabels[net.encryption] || `Type ${net.encryption}`;
    const rssiPct = Math.max(0, Math.min(100, Math.round(((net.rssi + 100) / 60) * 100)));
    const isCurrent = net.ssid && currentSsid && net.ssid === currentSsid;
    const isSaved = net.ssid && savedSsidList.includes(net.ssid);
    let status = '';
    if (isCurrent) {
      row.className = 'scan-row-current';
      status = 'CURRENT';
    } else if (isSaved) {
      row.className = 'scan-row-saved';
      status = 'SAVED';
    }
    row.dataset.ssid = net.ssid || '';
    row.innerHTML = `<td>${escapeHtml(net.ssid)}</td><td>${net.rssi} dBm</td><td>${rssiPct}%</td><td>${net.channel}</td><td>${enc}</td><td class="scan-status-td">${status}</td>`;
    row.addEventListener('click', () => {
      document.querySelectorAll('.scan-table tbody tr.selected').forEach(r => r.classList.remove('selected'));
      row.classList.add('selected');
      const ssid = row.dataset.ssid;
      if (ssid) {
        $('wifi-ssid').value = ssid;
        $('wifi-pswd').focus();
      }
    });
    tbody.appendChild(row);
  }
  table.appendChild(tbody);
  resultsEl.appendChild(table);
}

// ── IP Config ───────────────────────────────────────────────
$('ip-mode-toggle').addEventListener('click', () => {
  const toggle = $('ip-mode-toggle');
  toggle.classList.toggle('active');
  const fields = $('ip-static-fields');
  fields.style.display = toggle.classList.contains('active') ? '' : 'none';
});

$('btn-ip-save').addEventListener('click', async () => {
  const isStatic = $('ip-mode-toggle').classList.contains('active');
  const body = new URLSearchParams({ dhcp: isStatic ? '0' : '1' });
  if (isStatic) {
    body.append('ip',      $('ip-addr').value.trim());
    body.append('gateway', $('ip-gateway').value.trim());
    body.append('subnet',  $('ip-subnet').value.trim());
  }
  try {
    const data = await apiFetch('/api/network/ip/config', { method: 'POST', auth: true, body: body.toString() });
    showResult('ip-result', `IP config saved: ${data.use_static_ip ? 'STATIC' : 'DHCP'}. Reconnect to apply.`, 'success');
    toast('IP config saved', 'success');
  } catch (err) {
    showResult('ip-result', `Error: ${err.message}`, 'error');
    toast('IP config failed', 'error');
  }
});

// ── mDNS Save ──────────────────────────────────────────────
$('btn-mdns-save').addEventListener('click', async () => {
  const mdns = $('mdns-name').value.trim();
  if (!mdns) { toast('Enter an mDNS name', 'warning'); return; }
  const body = new URLSearchParams({ mdns }).toString();
  try {
    const data = await apiFetch('/api/network/mdns', { method: 'POST', auth: true, body });
    showResult('mdns-result', `mDNS saved: "${data.mdns}". Reboot to apply.`, 'success');
    toast('mDNS saved', 'success');
  } catch (err) {
    showResult('mdns-result', `Error: ${err.message}`, 'error');
    toast('mDNS save failed', 'error');
  }
});

// ── DNS Custom Toggle ──────────────────────────────────────
function updateDnsToggleUI(enabled) {
  const toggle = $('dns-custom-toggle');
  toggle.classList.toggle('active', enabled);
  const warn = $('dns-warning');
  warn.classList.toggle('hidden', enabled);
}

$('dns-custom-toggle').addEventListener('click', () => {
  const enabled = $('dns-custom-toggle').classList.contains('active');
  updateDnsToggleUI(!enabled);
});

// ── DNS Save ───────────────────────────────────────────────
$('btn-dns-save').addEventListener('click', async () => {
  const dns1 = $('dns1').value.trim();
  const dns2 = $('dns2').value.trim();
  if (!dns1 && !dns2) { toast('Enter at least one DNS server', 'warning'); return; }
  const use_custom_dns = $('dns-custom-toggle').classList.contains('active') ? '1' : '0';
  const body = new URLSearchParams({ use_custom_dns });
  if (dns1) body.append('dns1', dns1);
  if (dns2) body.append('dns2', dns2);
  try {
    const data = await apiFetch('/api/network/dns/config', { method: 'POST', auth: true, body: body.toString() });
    showResult('dns-result', `DNS saved: ${data.dns1}, ${data.dns2}. Applied ${data.use_custom_dns ? 'custom' : 'DHCP'} DNS.`, 'success');
    updateDnsToggleUI(data.use_custom_dns === 1);
    toast('DNS saved', 'success');
  } catch (err) {
    showResult('dns-result', `Error: ${err.message}`, 'error');
    toast('DNS save failed', 'error');
  }
});

// ── WiFi Tab ───────────────────────────────────────────────
$('btn-save-ssid').addEventListener('click', async () => {
  const ssid = $('wifi-ssid').value.trim();
  if (!ssid) { toast('Enter an SSID', 'warning'); return; }
  const body = new URLSearchParams({ ssid }).toString();
  try {
    const data = await apiFetch('/api/network/wifi/ssid', { method: 'POST', auth: true, body });
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
  const body = new URLSearchParams({ password: pswd }).toString();
  try {
    const data = await apiFetch('/api/network/wifi/password', { method: 'POST', auth: true, body });
    showResult('wifi-result', 'WiFi password saved.', 'success');
    toast('Password saved', 'success');
  } catch (err) {
    showResult('wifi-result', `Error: ${err.message}`, 'error');
    toast('Password save failed', 'error');
  }
});

// ── PING ─────────────────────────────────────────────────
$('btn-ping').addEventListener('click', async () => {
  const host = $('ping-host').value.trim();
  if (!host) { toast('Enter a host or IP', 'warning'); return; }
  hideResult('ping-result');
  try {
    const data = await apiFetch('/api/ping', { method: 'POST', auth: true, body: 'host=' + encodeURIComponent(host) });
    if (data.success) {
      showResult('ping-result', `${data.host} is alive — ${data.rtt_ms}ms avg`, 'success');
    } else {
      showResult('ping-result', `${data.host} is unreachable`, 'error');
    }
  } catch (err) {
    showResult('ping-result', `Error: ${err.message}`, 'error');
    toast('Ping failed', 'error');
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
    const data = await apiFetch('/api/http/fetch', { method: 'POST', auth: true, body: 'url=' + encodeURIComponent(url) });
    result.textContent = data;
    toast('Fetch complete', 'success');
  } catch (err) {
    result.textContent = `Error: ${err.message}`;
    toast('Fetch failed', 'error');
  }
});

// ── API Explorer ─────────────────────────────────────────
let _apiEndpoints = [];
let _selectedEp = null;

const API_GROUPS = ['SYSTEM','NETWORK','NTP','WIFI','GPIO','DDNS','TOOLS','CONFIG'];

function getApiGroup(path) {
  if (path.startsWith('/api/system/')) {
    if (path.includes('ota')) return 'SYSTEM';
    return 'SYSTEM';
  }
  if (path.startsWith('/api/network/')) return 'NETWORK';
  if (path.startsWith('/api/wifi/')) return 'WIFI';
  if (path.startsWith('/api/ntp/')) return 'NTP';
  if (path.startsWith('/api/gpio/')) return 'GPIO';
  if (path.startsWith('/api/ddns/')) return 'DDNS';
  if (path.startsWith('/api/ping') || path.startsWith('/api/dns/') || path.startsWith('/api/http/')) return 'TOOLS';
  if (path.startsWith('/api/config/')) return 'CONFIG';
  if (path.startsWith('/api/status') || path.startsWith('/api/help')) return 'SYSTEM';
  return 'SYSTEM';
}

$('btn-api-refresh').addEventListener('click', loadApiExplorer);

async function loadApiExplorer() {
  const list = $('api-endpoints-list');
  const detail = $('api-tester-detail');
  detail.classList.add('hidden');
  list.innerHTML = '<div class="gpio-loading">Loading endpoints…</div>';
  try {
    const data = await apiFetch('/api/help');
    _apiEndpoints = data.endpoints || [];
    if (!_apiEndpoints.length) {
      list.innerHTML = '<div style="color:var(--text-dim);font-size:0.75rem">No endpoints returned.</div>';
      return;
    }
    // Group by path prefix
    const grouped = {};
    for (const ep of _apiEndpoints) {
      const grp = getApiGroup(ep.path);
      if (!grouped[grp]) grouped[grp] = [];
      grouped[grp].push(ep);
    }
    list.innerHTML = '';
    let flatIdx = 0;
    for (const grp of API_GROUPS) {
      const eps = grouped[grp];
      if (!eps || !eps.length) continue;
      const header = document.createElement('div');
      header.className = 'api-group-header';
      header.textContent = grp;
      list.appendChild(header);
      for (const ep of eps) {
        const item = document.createElement('div');
        item.className = 'api-list-item';
        item.dataset.index = flatIdx;
        const isPost = ep.method === 'POST';
        item.innerHTML = `<span class="api-tester-method">${isPost ? 'POST' : 'GET'}</span><span class="api-tester-path">${ep.path}</span>`;
        item.addEventListener('click', () => selectApiEndpoint(parseInt(item.dataset.index)));
        list.appendChild(item);
        flatIdx++;
      }
    }
    // Rebuild flat array in grouped order
    _apiEndpoints = [];
    for (const grp of API_GROUPS) {
      const eps = grouped[grp];
      if (eps) _apiEndpoints.push(...eps);
    }
    selectApiEndpoint(0);
    toast(`${_apiEndpoints.length} endpoints loaded`, 'success');
  } catch (err) {
    list.innerHTML = `<div style="color:var(--red);font-size:0.75rem">Failed: ${err.message}</div>`;
    toast('API load failed', 'error');
  }
}

function selectApiEndpoint(index) {
  _selectedEp = _apiEndpoints[index];
  if (!_selectedEp) return;
  document.querySelectorAll('.api-list-item').forEach(el => el.classList.remove('active'));
  const items = document.querySelectorAll('.api-list-item');
  if (items[index]) items[index].classList.add('active');

  const detail = $('api-tester-detail');
  detail.classList.remove('hidden');
  const isPost = _selectedEp.method === 'POST';
  const path = _selectedEp.path || '';
  const params = _selectedEp.params || [];

  setText('api-detail-method', isPost ? 'POST' : 'GET');
  setText('api-detail-path', path);

  const paramsContainer = $('api-detail-params');
  paramsContainer.innerHTML = '';
  if (params.length) {
    for (const p of params) {
      const row = document.createElement('div');
      row.className = 'api-tester-input-row';
      row.innerHTML = `<label class="field-label">${p}</label><input type="text" class="field-input" data-param="${p}" placeholder="${p}" />`;
      paramsContainer.appendChild(row);
    }
  }

  const respDiv = $('api-detail-response');
  respDiv.classList.add('hidden');
}

$('btn-api-call').addEventListener('click', async () => {
  if (!_selectedEp) return;
  const isPost = _selectedEp.method === 'POST';
  const path = _selectedEp.path || '';
  const authCb = $('api-detail-auth');
  const respDiv = $('api-detail-response');
  const respBody = $('api-detail-body');
  const btn = $('btn-api-call');

  btn.disabled = true;
  btn.textContent = '⟳';
  respDiv.classList.remove('hidden');
  respBody.textContent = 'Calling...';

  try {
    const paramInputs = $('api-detail-params').querySelectorAll('[data-param]');
    const urlParams = new URLSearchParams();
    const bodyParams = new URLSearchParams();
    paramInputs.forEach(inp => {
      if (inp.value.trim()) {
        if (isPost) bodyParams.append(inp.dataset.param, inp.value.trim());
        else urlParams.append(inp.dataset.param, inp.value.trim());
      }
    });
    const qs = urlParams.toString();
    const fullPath = qs ? path + '?' + qs : path;
    const opts = { auth: authCb.checked };
    if (isPost && bodyParams.toString()) {
      opts.method = 'POST';
      opts.body = bodyParams.toString();
    }
    const result = await apiFetch(fullPath, opts);
    respBody.textContent = typeof result === 'object' ? JSON.stringify(result, null, 2) : String(result);
  } catch (err) {
    respBody.textContent = `Error: ${err.message}`;
  } finally {
    btn.disabled = false;
    btn.textContent = 'CALL';
  }
});

$('btn-api-copy').addEventListener('click', () => {
  const text = $('api-detail-body').textContent;
  navigator.clipboard.writeText(text).then(() => {
    toast('Response copied', 'success', 1000);
  }).catch(() => {
    const ta = document.createElement('textarea');
    ta.value = text;
    document.body.appendChild(ta);
    ta.select();
    document.execCommand('copy');
    ta.remove();
    toast('Response copied', 'success', 1000);
  });
});

// ── Config Export/Import ──────────────────────────────────
$('btn-config-export').addEventListener('click', async () => {
  try {
    const data = await apiFetch('/api/config/export', { auth: true });
    const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'esp8266-config.json';
    a.click();
    URL.revokeObjectURL(url);
    toast('Config exported', 'success');
  } catch (err) {
    toast(`Export failed: ${err.message}`, 'error');
  }
});

$('btn-config-import').addEventListener('change', async (e) => {
  const file = e.target.files[0];
  if (!file) return;
  try {
    const text = await file.text();
    const json = JSON.parse(text);
    const body = new URLSearchParams({ config: text }).toString();
    const data = await apiFetch('/api/config/import', { method: 'POST', auth: true, body });
    showResult('config-import-result', `Import: ${data.status}`, 'success');
    toast('Config imported — reboot recommended', 'success');
  } catch (err) {
    showResult('config-import-result', `Error: ${err.message}`, 'error');
    toast('Import failed', 'error');
  }
  e.target.value = '';
});

// ── System Tab ────────────────────────────────────────────
$('btn-reboot').addEventListener('click', async () => {
  if (!confirm('Reboot the device? Connection will be lost.')) return;
  try {
    await apiFetch('/api/system/reboot', { method: 'POST', auth: true });
    showResult('reboot-result', 'Rebooting... device will be offline for a moment.', 'success');
    toast('Rebooting...', 'warning', 5000);
  } catch (err) {
    showResult('reboot-result', `Reboot command sent (device may be offline): ${err.message}`, 'warning');
  }
});

// ── Overview Reboot ─────────────────────────────────────
$('btn-ov-reboot').addEventListener('click', async () => {
  if (!confirm('Reboot the device? Connection will be lost.')) return;
  try {
    await apiFetch('/api/system/reboot', { method: 'POST', auth: true });
    showResult('ov-reboot-result', 'Rebooting... device will be offline.', 'success');
    toast('Rebooting...', 'warning', 5000);
  } catch (err) {
    showResult('ov-reboot-result', `Reboot sent: ${err.message}`, 'warning');
  }
});

$('btn-factory-reset').addEventListener('click', async () => {
  if (!confirm('⚠️ FACTORY RESET — ALL settings will be lost! Are you sure?')) return;
  if (!confirm('This CANNOT be undone. Proceed with factory reset?')) return;
  try {
    await apiFetch('/api/system/factory-reset', { method: 'POST', auth: true });
    showResult('factory-reset-result', 'Factory reset done — device will reboot with defaults.', 'success');
    toast('Factory reset — rebooting', 'warning', 5000);
  } catch (err) {
    showResult('factory-reset-result', `Reset command sent (device may be offline): ${err.message}`, 'warning');
  }
});

// ── Log Tab ────────────────────────────────────────────────
async function loadLog() {
  const container = $('log-container');
  container.innerHTML = '<div class="log-empty">Loading…</div>';
  try {
    const data = await apiFetch('/api/system/log', { auth: true });
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
      const timeStr = entry.time ? formatUptime(entry.time / 1000) : '—';
      div.innerHTML = `<span class="log-time">${timeStr}</span><span class="log-msg">${escapeHtml(entry.msg || '')}</span>`;
      container.appendChild(div);
    }
  } catch (err) {
    container.innerHTML = `<div class="log-empty" style="color:var(--red)">Error: ${err.message}</div>`;
    toast('Log fetch failed', 'error');
  }
}

$('btn-log-refresh').addEventListener('click', loadLog);

// ── Network Tab Init ────────────────────────────────────────
async function loadNetworkTab() {
  try {
    const info = await apiFetch('/api/info');
    if (info.ntp_server) $('ntp-server').value = info.ntp_server;
    if (info.tz_offset != null) $('ntp-tz').value = info.tz_offset;
    if (info.mdns) $('mdns-name').value = info.mdns;
    if (info.static_dns1) $('dns1').value = info.static_dns1;
    if (info.static_dns2) $('dns2').value = info.static_dns2;
    if (info.dns1) $('dns1').placeholder = info.dns1;
    if (info.dns2) $('dns2').placeholder = info.dns2;
    if (info.use_custom_dns != null) updateDnsToggleUI(info.use_custom_dns === 1);
    if (info.use_static_ip != null) {
      const isStatic = info.use_static_ip === 1;
      $('ip-mode-toggle').classList.toggle('active', isStatic);
      $('ip-static-fields').style.display = isStatic ? '' : 'none';
    }
    if (info.static_ip) $('ip-addr').value = info.static_ip;
    if (info.static_gateway) $('ip-gateway').value = info.static_gateway;
    if (info.static_subnet) $('ip-subnet').value = info.static_subnet;

  } catch {}
  loadSavedNetworks();
  loadApStatus();
}

// ── AP Mode ────────────────────────────────────────────────
async function loadApStatus() {
  try {
    const data = await apiFetch('/api/wifi/ap');
    const active = data.active || false;
    $('ap-toggle').classList.toggle('active', active);
    $('ap-toggle-label').textContent = active ? 'ON' : 'OFF';
    $('ap-fields').classList.toggle('hidden', !active);
    if (active) {
      if (data.configured_ssid) $('ap-ssid').value = data.configured_ssid;
      setText('ap-ip', data.ip || '—');
      setText('ap-clients', data.clients || 0);
    }
    if (data.ap_fallback != null) {
      $('ap-fallback-toggle').classList.toggle('active', data.ap_fallback === 1);
    }
  } catch {}
}

$('ap-toggle').addEventListener('click', async () => {
  const toggle = $('ap-toggle');
  const enable = !toggle.classList.contains('active');
  const body = new URLSearchParams({ enabled: enable ? '1' : '0' }).toString();
  try {
    await apiFetch('/api/wifi/ap', { method: 'POST', auth: true, body });
    toggle.classList.toggle('active', enable);
    $('ap-toggle-label').textContent = enable ? 'ON' : 'OFF';
    $('ap-fields').classList.toggle('hidden', !enable);
    toast(`AP ${enable ? 'started' : 'stopped'}`, enable ? 'success' : 'warning');
  } catch (err) {
    toast(`AP toggle failed: ${err.message}`, 'error');
  }
});

$('btn-ap-save').addEventListener('click', async () => {
  const ssid = $('ap-ssid').value.trim();
  const password = $('ap-password').value;
  const fallback = $('ap-fallback-toggle').classList.contains('active') ? '1' : '0';
  const body = new URLSearchParams({ ssid, password, ap_fallback: fallback }).toString();
  try {
    await apiFetch('/api/wifi/ap', { method: 'POST', auth: true, body });
    showResult('ap-result', 'AP configuration saved.', 'success');
    toast('AP config saved', 'success');
  } catch (err) {
    showResult('ap-result', `Error: ${err.message}`, 'error');
  }
});

$('btn-ap-toggle').addEventListener('click', async () => {
  const active = $('ap-toggle').classList.contains('active');
  const enable = active ? '0' : '1';
  const body = new URLSearchParams({ enabled: enable }).toString();
  try {
    await apiFetch('/api/wifi/ap', { method: 'POST', auth: true, body });
    $('ap-toggle').classList.toggle('active', !active);
    $('ap-toggle-label').textContent = !active ? 'ON' : 'OFF';
    $('ap-fields').classList.toggle('hidden', active);
    toast(`AP ${!active ? 'started' : 'stopped'}`, !active ? 'success' : 'warning');
  } catch (err) {
    toast(`AP failed: ${err.message}`, 'error');
  }
});

// ── Saved Networks ─────────────────────────────────────────
async function loadSavedNetworks() {
  const container = $('networks-list');
  container.innerHTML = '<div class="scan-status">Loading…</div>';
  try {
    const data = await apiFetch('/api/wifi/networks', { auth: true });
    const networks = data.networks || [];
    savedSsidList = networks.map(n => n.ssid);
    if (!networks.length) {
      container.innerHTML = '<div class="scan-status">No networks saved. Add one below.</div>';
      return;
    }
    container.innerHTML = '';
    const table = document.createElement('table');
    table.className = 'scan-table';
    table.innerHTML = '<thead><tr><th>#</th><th>SSID</th><th>PRIORITY</th><th>ACTIONS</th></tr></thead>';
    const tbody = document.createElement('tbody');
    for (const net of networks) {
      const idx = net.index;
      const tr = document.createElement('tr');
      const canUp = idx > 0;
      const canDown = idx < networks.length - 1;
      tr.innerHTML = `
        <td>${idx + 1}</td>
        <td>${escapeHtml(net.ssid)}</td>
        <td>
          <button class="card-action-btn network-up" data-idx="${idx}" ${canUp ? '' : 'disabled'}>▲</button>
          <button class="card-action-btn network-down" data-idx="${idx}" ${canDown ? '' : 'disabled'}>▼</button>
        </td>
        <td>
          <button class="card-action-btn network-connect" data-idx="${idx}">CONNECT</button>
          <button class="card-action-btn network-delete" data-idx="${idx}">DELETE</button>
        </td>`;
      tbody.appendChild(tr);
    }
    table.appendChild(tbody);
    container.appendChild(table);
  } catch (err) {
    container.innerHTML = `<div class="scan-status error">Error: ${err.message}</div>`;
  }
}

$('networks-list').addEventListener('click', async (e) => {
  const delBtn = e.target.closest('.network-delete');
  if (delBtn) {
    const idx = delBtn.dataset.idx;
    try {
      await apiFetch('/api/wifi/networks?index=' + idx, { method: 'DELETE', auth: true });
      toast('Network removed', 'success');
      loadSavedNetworks();
    } catch (err) {
      toast(`Delete failed: ${err.message}`, 'error');
    }
    return;
  }
  const connectBtn = e.target.closest('.network-connect');
  if (connectBtn) {
    const idx = connectBtn.dataset.idx;
    try {
      const data = await apiFetch('/api/wifi/connect', { method: 'POST', auth: true, body: 'index=' + idx });
      toast('Connecting to ' + (data.ssid || 'network ' + idx), 'warning');
    } catch (err) {
      toast(`Connect failed: ${err.message}`, 'error');
    }
    return;
  }
  const upBtn = e.target.closest('.network-up');
  if (upBtn) {
    const idx = parseInt(upBtn.dataset.idx);
    try {
      await apiFetch('/api/wifi/networks/reorder', { method: 'POST', auth: true, body: 'from=' + idx + '&to=' + (idx - 1) });
      loadSavedNetworks();
    } catch (err) {
      toast(`Reorder failed: ${err.message}`, 'error');
    }
    return;
  }
  const downBtn = e.target.closest('.network-down');
  if (downBtn) {
    const idx = parseInt(downBtn.dataset.idx);
    try {
      await apiFetch('/api/wifi/networks/reorder', { method: 'POST', auth: true, body: 'from=' + idx + '&to=' + (idx + 1) });
      loadSavedNetworks();
    } catch (err) {
      toast(`Reorder failed: ${err.message}`, 'error');
    }
    return;
  }
});

$('btn-networks-refresh').addEventListener('click', loadSavedNetworks);

$('btn-network-add').addEventListener('click', async () => {
  const ssid = $('network-add-ssid').value.trim();
  const password = $('network-add-pswd').value;
  if (!ssid) { toast('Enter an SSID', 'warning'); return; }
  const body = new URLSearchParams({ ssid, password }).toString();
  try {
    await apiFetch('/api/wifi/networks', { method: 'POST', auth: true, body });
    toast('Network added', 'success');
    $('network-add-ssid').value = '';
    $('network-add-pswd').value = '';
    loadSavedNetworks();
  } catch (err) {
    showResult('network-result', `Error: ${err.message}`, 'error');
    toast('Add failed', 'error');
  }
});

$('btn-network-connect').addEventListener('click', async () => {
  const ssid = $('network-add-ssid').value.trim();
  const password = $('network-add-pswd').value;
  if (!ssid) { toast('Enter an SSID', 'warning'); return; }
  const body = new URLSearchParams({ ssid, password }).toString();
  try {
    await apiFetch('/api/wifi/networks', { method: 'POST', auth: true, body });
    toast('Network added, connecting...', 'success');
    // Now try to connect to the last added network
    const data = await apiFetch('/api/wifi/networks', { auth: true });
    const networks = data.networks || [];
    if (networks.length > 0) {
      const lastIdx = networks[networks.length - 1].index;
      await apiFetch('/api/wifi/connect', { method: 'POST', auth: true, body: 'index=' + lastIdx });
      toast('Connecting...', 'warning');
    }
    loadSavedNetworks();
  } catch (err) {
    showResult('network-result', `Error: ${err.message}`, 'error');
    toast('Failed', 'error');
  }
});

// ── OTA ────────────────────────────────────────────────────
async function loadOtaConfig() {
  try {
    const data = await apiFetch('/api/system/ota');
    $('ota-url').value = data.ota_url || '';
    $('ota-interval').value = data.ota_interval || 0;
    setText('ota-current-version', data.current_version || '—');
    if (data.last_check_elapsed != null) {
      setText('ota-last-check', formatUptime(data.last_check_elapsed) + ' ago');
    } else {
      setText('ota-last-check', '—');
    }
  } catch {}
}

$('btn-ota-save').addEventListener('click', async () => {
  const url = $('ota-url').value.trim();
  const interval = parseInt($('ota-interval').value) || 0;
  const body = new URLSearchParams({ url, interval }).toString();
  try {
    await apiFetch('/api/system/ota', { method: 'POST', auth: true, body });
    showResult('ota-result', 'OTA config saved', 'success');
    toast('OTA config saved', 'success');
  } catch (err) {
    showResult('ota-result', `Error: ${err.message}`, 'error');
    toast('OTA save failed', 'error');
  }
});

$('btn-ota-check').addEventListener('click', async () => {
  const btn = $('btn-ota-check');
  btn.disabled = true;
  btn.textContent = '⟳ CHECKING…';
  try {
    const data = await apiFetch('/api/system/ota/check', { method: 'POST', auth: true });
    if (data.updating) {
      showResult('ota-result', 'Update found — flashing firmware, device will reboot...', 'success');
      toast('OTA update started', 'warning', 10000);
    } else {
      const errMsg = data.error || 'up to date';
      showResult('ota-result', `No update: ${errMsg}`, 'success');
      toast('Firmware is current', 'success');
    }
  } catch (err) {
    showResult('ota-result', `Check failed: ${err.message}`, 'error');
    toast('OTA check failed', 'error');
  } finally {
    btn.disabled = false;
    btn.innerHTML = 'CHECK FOR UPDATE';
  }
});

function escapeHtml(str) {
  return str.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}
