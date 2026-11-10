#include "MakerindoWiFiManager.h"

#include <WiFi.h>
#include <cstring>

namespace {

constexpr uint32_t ACCESS_POINT_RETRY_INTERVAL_MS = 5000;

const char PORTAL_PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html lang="id">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="theme-color" content="#0b4565">
  <title>Pengaturan Wi-Fi | MAKERINDO</title>
  <style>
    :root {
      color-scheme: light;
      --navy: #0b4565;
      --blue: #167eaa;
      --cyan: #39b9d6;
      --ink: #18364a;
      --muted: #718797;
      --line: #e2edf2;
      --paper: #ffffff;
      --canvas: #f2f8fb;
      --success: #16845b;
      --danger: #b94343;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      padding: 30px 16px;
      color: var(--ink);
      background:
        radial-gradient(ellipse at 14% 5%, #d9f2f8 0, transparent 35%),
        radial-gradient(ellipse at 100% 90%, #e0eef8 0, transparent 36%),
        var(--canvas);
      font-family: Inter, ui-sans-serif, system-ui, -apple-system, "Segoe UI", sans-serif;
    }
    .shell { width: min(100%, 510px); margin: 0 auto; }
    .brand {
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 12px;
      margin: 0 auto 22px;
      color: var(--navy);
    }
    .brand-mark { width: 52px; height: 52px; flex: none; }
    .brand-name { font-size: 20px; font-weight: 850; letter-spacing: .075em; line-height: 1; }
    .brand-caption { margin-top: 5px; color: var(--muted); font-size: 9px; font-weight: 700; letter-spacing: .19em; }
    .card {
      overflow: hidden;
      border: 1px solid rgba(207, 225, 235, .8);
      border-radius: 24px;
      background: var(--paper);
      box-shadow: 0 22px 65px rgba(18, 70, 98, .12);
    }
    .hero {
      position: relative;
      overflow: hidden;
      padding: 27px 28px 25px;
      color: white;
      background: linear-gradient(125deg, #0b4565 0%, #117ba4 67%, #25a8c6 100%);
    }
    .hero::after {
      position: absolute;
      top: -78px;
      right: -57px;
      width: 210px;
      height: 210px;
      border: 1px solid rgba(255,255,255,.16);
      border-radius: 34px;
      content: "";
      transform: rotate(35deg);
    }
    .eyebrow { margin: 0 0 8px; color: #a9e9f5; font-size: 11px; font-weight: 800; letter-spacing: .14em; text-transform: uppercase; }
    h1 { position: relative; z-index: 1; margin: 0; font-size: clamp(25px, 7vw, 31px); letter-spacing: -.04em; }
    .hero p:last-child { position: relative; z-index: 1; margin: 9px 0 0; color: #d8f2f7; font-size: 14px; line-height: 1.6; }
    .content { padding: 25px 28px 27px; }
    .section-title { display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; }
    .section-title h2 { margin: 0; font-size: 14px; }
    .text-button { border: 0; padding: 5px 0 5px 8px; color: var(--blue); background: transparent; font: inherit; font-size: 12px; font-weight: 750; cursor: pointer; }
    .networks { display: grid; gap: 8px; max-height: 206px; overflow: auto; }
    .network {
      display: flex;
      width: 100%;
      align-items: center;
      gap: 11px;
      padding: 11px 13px;
      border: 1px solid var(--line);
      border-radius: 12px;
      color: var(--ink);
      background: #fff;
      text-align: left;
      cursor: pointer;
      transition: border-color .15s, background .15s, transform .15s;
    }
    .network:hover { border-color: #85c9db; transform: translateY(-1px); }
    .network.selected { border-color: var(--blue); background: #effaff; box-shadow: 0 0 0 2px rgba(22,126,170,.09); }
    .wifi-icon { width: 21px; height: 21px; color: var(--blue); flex: none; }
    .network-copy { min-width: 0; flex: 1; }
    .network-name { overflow: hidden; font-size: 13px; font-weight: 720; text-overflow: ellipsis; white-space: nowrap; }
    .network-meta { margin-top: 3px; color: var(--muted); font-size: 10px; }
    .signal { color: #9aafba; font-size: 10px; white-space: nowrap; }
    .empty { padding: 14px; border: 1px dashed #cbdde5; border-radius: 12px; color: var(--muted); font-size: 12px; line-height: 1.6; text-align: center; }
    .divider { height: 1px; margin: 22px 0; background: var(--line); }
    label { display: block; margin-bottom: 7px; font-size: 12px; font-weight: 750; }
    .field { margin-top: 15px; }
    input {
      width: 100%;
      min-height: 46px;
      padding: 11px 13px;
      border: 1px solid #d8e5eb;
      border-radius: 11px;
      outline: none;
      color: var(--ink);
      background: #fff;
      font: inherit;
      font-size: 14px;
      transition: border-color .15s, box-shadow .15s;
    }
    input:focus { border-color: var(--blue); box-shadow: 0 0 0 3px rgba(22,126,170,.12); }
    input::placeholder { color: #a2b3bc; }
    .password-wrap { position: relative; }
    .password-wrap input { padding-right: 72px; }
    .show-password { position: absolute; top: 50%; right: 12px; border: 0; padding: 6px 0 6px 8px; color: var(--blue); background: transparent; font: inherit; font-size: 11px; font-weight: 750; cursor: pointer; transform: translateY(-50%); }
    .hint { margin: 7px 0 0; color: var(--muted); font-size: 11px; line-height: 1.55; }
    .submit {
      display: flex;
      width: 100%;
      min-height: 49px;
      align-items: center;
      justify-content: center;
      gap: 9px;
      margin-top: 22px;
      border: 0;
      border-radius: 12px;
      color: #fff;
      background: linear-gradient(105deg, #0e648d, #159ec0);
      box-shadow: 0 8px 18px rgba(17,126,164,.2);
      font: inherit;
      font-size: 14px;
      font-weight: 800;
      cursor: pointer;
      transition: filter .15s, transform .15s;
    }
    .submit:hover { filter: brightness(1.06); transform: translateY(-1px); }
    .submit:disabled { cursor: wait; filter: grayscale(.25); opacity: .75; transform: none; }
    .status { display: none; margin-top: 14px; padding: 12px 13px; border-radius: 11px; font-size: 12px; line-height: 1.55; }
    .status.visible { display: block; }
    .status.info { color: #155778; background: #e9f7fc; }
    .status.success { color: #116646; background: #e9f7f0; }
    .status.error { color: #a43838; background: #fff0ef; }
    .footer { padding: 15px 18px; border-top: 1px solid var(--line); color: var(--muted); font-size: 10px; text-align: center; }
    .footer strong { color: var(--navy); }
    .pulse { display: inline-block; width: 7px; height: 7px; margin-right: 5px; border-radius: 50%; background: #45d4a0; box-shadow: 0 0 0 3px rgba(69,212,160,.16); vertical-align: 1px; }
    @media (max-width: 420px) {
      body { padding: 20px 12px; }
      .hero { padding: 24px 21px 22px; }
      .content { padding: 22px 20px 23px; }
      .brand { margin-bottom: 17px; }
    }
  </style>
</head>
<body>
  <main class="shell">
    <div class="brand" aria-label="MAKERINDO">
      <svg class="brand-mark" viewBox="0 0 64 64" aria-hidden="true">
        <path d="M32 3 57 17v30L32 61 7 47V17L32 3Z" fill="none" stroke="#167eaa" stroke-width="3"/>
        <path d="M32 8 52 19v24L32 55 12 43V20L32 8Z" fill="#0b4565"/>
        <path d="m14 34 10 0 7-13 5 23 6-22 5 12h7" fill="none" stroke="#fff" stroke-linecap="round" stroke-linejoin="round" stroke-width="3.5"/>
        <circle cx="32" cy="3" r="3" fill="#39b9d6"/><circle cx="57" cy="17" r="3" fill="#39b9d6"/>
        <circle cx="57" cy="47" r="3" fill="#39b9d6"/><circle cx="32" cy="61" r="3" fill="#39b9d6"/>
        <circle cx="7" cy="47" r="3" fill="#39b9d6"/><circle cx="7" cy="17" r="3" fill="#39b9d6"/>
      </svg>
      <div><div class="brand-name">MAKERINDO</div><div class="brand-caption">BUILD · CREATE · CONNECT</div></div>
    </div>

    <section class="card">
      <header class="hero">
        <p class="eyebrow">ESP32-S3 · Wi-Fi Setup</p>
        <h1>Hubungkan perangkat</h1>
        <p>Pilih jaringan Wi-Fi dan masukkan kata sandi untuk menghubungkan perangkat MAKERINDO kamu.</p>
      </header>
      <div class="content">
        <div class="section-title">
          <h2>Pilih jaringan</h2>
          <button class="text-button" id="refresh" type="button">↻ Pindai ulang</button>
        </div>
        <div class="networks" id="networks"><div class="empty">Sedang mencari jaringan Wi-Fi…</div></div>
        <div class="divider"></div>
        <form id="wifi-form">
          <div>
            <label for="ssid">Nama jaringan (SSID)</label>
            <input id="ssid" name="ssid" type="text" maxlength="32" placeholder="Pilih jaringan atau ketik nama Wi-Fi" autocomplete="off" required>
          </div>
          <div class="field">
            <label for="password">Kata sandi Wi-Fi</label>
            <div class="password-wrap">
              <input id="password" name="password" type="password" maxlength="64" placeholder="Masukkan kata sandi" autocomplete="new-password">
              <button class="show-password" id="toggle-password" type="button">Lihat</button>
            </div>
            <p class="hint">Jaringan terbuka tidak memerlukan kata sandi. Data hanya dikirim langsung ke perangkat ini.</p>
          </div>
          <button class="submit" id="submit" type="submit"><span>Hubungkan ke Wi-Fi</span><span aria-hidden="true">→</span></button>
        </form>
        <div class="status" id="status" role="status" aria-live="polite"></div>
      </div>
      <footer class="footer"><span class="pulse"></span>Portal lokal aman · <strong>MAKERINDO</strong></footer>
    </section>
  </main>
  <script>
    const networks = document.getElementById('networks');
    const ssidInput = document.getElementById('ssid');
    const passwordInput = document.getElementById('password');
    const statusBox = document.getElementById('status');
    const submitButton = document.getElementById('submit');
    const wifiIcon = '<svg class="wifi-icon" viewBox="0 0 24 24" fill="none" aria-hidden="true"><path d="M3 9a14 14 0 0 1 18 0M6.5 12.5a8.5 8.5 0 0 1 11 0M10 16a3.2 3.2 0 0 1 4 0M12 20h.01" stroke="currentColor" stroke-width="2" stroke-linecap="round"/></svg>';

    function showStatus(message, type) {
      statusBox.textContent = message;
      statusBox.className = 'status visible ' + type;
    }

    async function scanNetworks() {
      networks.innerHTML = '<div class="empty">Sedang mencari jaringan Wi-Fi…</div>';
      try {
        const response = await fetch('/api/networks');
        if (!response.ok) throw new Error('Gagal memindai jaringan.');
        const found = await response.json();
        if (!found.length) {
          networks.innerHTML = '<div class="empty">Belum ada jaringan ditemukan. Pastikan router menyala, lalu pindai ulang atau masukkan SSID secara manual.</div>';
          return;
        }
        networks.replaceChildren();
        found.forEach((network) => {
          const button = document.createElement('button');
          button.type = 'button';
          button.className = 'network';
          button.innerHTML = wifiIcon;
          const copy = document.createElement('span');
          copy.className = 'network-copy';
          const name = document.createElement('span');
          name.className = 'network-name';
          name.textContent = network.ssid || '(Jaringan tersembunyi)';
          const meta = document.createElement('span');
          meta.className = 'network-meta';
          meta.textContent = network.secure ? 'Terenkripsi · perlu kata sandi' : 'Jaringan terbuka';
          copy.append(name, meta);
          const signal = document.createElement('span');
          signal.className = 'signal';
          signal.textContent = network.rssi + ' dBm';
          button.append(copy, signal);
          button.addEventListener('click', () => {
            ssidInput.value = network.ssid;
            document.querySelectorAll('.network').forEach((item) => item.classList.remove('selected'));
            button.classList.add('selected');
            if (network.secure) passwordInput.focus();
            else passwordInput.value = '';
          });
          networks.append(button);
        });
      } catch (error) {
        networks.innerHTML = '<div class="empty">Tidak dapat memindai jaringan. Periksa koneksi ke portal lalu coba lagi.</div>';
      }
    }

    document.getElementById('refresh').addEventListener('click', scanNetworks);
    document.getElementById('toggle-password').addEventListener('click', (event) => {
      const showing = passwordInput.type === 'text';
      passwordInput.type = showing ? 'password' : 'text';
      event.currentTarget.textContent = showing ? 'Lihat' : 'Tutup';
    });

    document.getElementById('wifi-form').addEventListener('submit', async (event) => {
      event.preventDefault();
      const ssid = ssidInput.value.trim();
      if (!ssid) {
        showStatus('Nama jaringan Wi-Fi wajib diisi.', 'error');
        ssidInput.focus();
        return;
      }
      submitButton.disabled = true;
      submitButton.firstElementChild.textContent = 'Menghubungkan…';
      showStatus('Mencoba terhubung ke ' + ssid + '. Tunggu sebentar, halaman ini tetap tersedia.', 'info');
      try {
        const response = await fetch('/api/connect', {
          method: 'POST',
          headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
          body: new URLSearchParams({ ssid, password: passwordInput.value })
        });
        const result = await response.json();
        if (!response.ok) throw new Error(result.message || 'Gagal menghubungkan perangkat.');
        showStatus('Berhasil terhubung ke ' + result.ssid + '. Alamat IP: ' + result.ip + '. Pengaturan tersimpan di perangkat.', 'success');
      } catch (error) {
        showStatus(error.message || 'Terjadi kesalahan saat menghubungkan perangkat.', 'error');
      } finally {
        submitButton.disabled = false;
        submitButton.firstElementChild.textContent = 'Hubungkan ke Wi-Fi';
      }
    });

    scanNetworks();
  </script>
</body>
</html>
)HTML";

}  // namespace

MakerindoWiFiManager::MakerindoWiFiManager(uint16_t serverPort)
    : server_(serverPort) {}

bool MakerindoWiFiManager::begin(const char* apSsid, const char* apPassword,
                                 const char* preferencesNamespace,
                                 uint32_t connectionTimeoutMs) {
  if (started_) {
    Serial.println("[WiFiManager] begin() hanya boleh dipanggil sekali.");
    return false;
  }
  if (apSsid == nullptr || apPassword == nullptr ||
      preferencesNamespace == nullptr || apSsid[0] == '\0' ||
      strlen(apSsid) > 32 ||
      (strlen(apPassword) != 0 &&
       (strlen(apPassword) < 8 || strlen(apPassword) > 63)) ||
      preferencesNamespace[0] == '\0' ||
      strlen(preferencesNamespace) > 15 || connectionTimeoutMs == 0) {
    Serial.println("[WiFiManager] Konfigurasi portal tidak valid.");
    return false;
  }

  started_ = true;
  apSsid_ = apSsid;
  apPassword_ = apPassword;
  connectionTimeoutMs_ = connectionTimeoutMs;
  preferencesReady_ = preferences_.begin(preferencesNamespace, false);
  if (!preferencesReady_) {
    Serial.println("[WiFiManager] Gagal membuka penyimpanan NVS.");
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  registerRoutes();
  const bool savedNetworkConnected = connectSavedNetwork();
  bool accessPointStarted = false;
  if (!savedNetworkConnected) {
    accessPointStarted = startPortal();
  }
  return preferencesReady_ && (savedNetworkConnected || accessPointStarted);
}

void MakerindoWiFiManager::loop() {
  if (!started_) {
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (portalRunning_) {
      stopPortal();
    }
  } else if (!portalRunning_ &&
             millis() - lastAccessPointAttemptAt_ >=
                 ACCESS_POINT_RETRY_INTERVAL_MS) {
    startPortal();
  }

  if (portalRunning_) {
    server_.handleClient();
  }
}

bool MakerindoWiFiManager::isConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

String MakerindoWiFiManager::localIP() const {
  return WiFi.localIP().toString();
}

String MakerindoWiFiManager::accessPointIP() const {
  return WiFi.softAPIP().toString();
}

bool MakerindoWiFiManager::startPortal() {
  lastAccessPointAttemptAt_ = millis();
  WiFi.mode(WIFI_AP_STA);
  const char* password = apPassword_.isEmpty() ? nullptr : apPassword_.c_str();
  if (!WiFi.softAP(apSsid_.c_str(), password)) {
    Serial.println("[WiFiManager] Gagal membuat access point konfigurasi.");
    return false;
  }

  server_.begin();
  portalRunning_ = true;
  Serial.printf("[WiFiManager] Portal aktif: SSID %s\n", apSsid_.c_str());
  Serial.printf("[WiFiManager] Buka http://%s di browser\n",
                WiFi.softAPIP().toString().c_str());
  return true;
}

void MakerindoWiFiManager::stopPortal() {
  server_.stop();
  WiFi.softAPdisconnect(false);
  WiFi.mode(WIFI_STA);
  portalRunning_ = false;
  Serial.println(
      "[WiFiManager] Wi-Fi tersambung; access point konfigurasi dimatikan.");
}

void MakerindoWiFiManager::registerRoutes() {
  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/api/networks", HTTP_GET, [this]() { handleNetworkScan(); });
  server_.on("/api/connect", HTTP_POST, [this]() { handleConnect(); });
  server_.onNotFound([this]() {
    server_.sendHeader("Location", "/", true);
    server_.send(302, "text/plain", "");
  });
}

void MakerindoWiFiManager::handleRoot() {
  server_.send_P(200, "text/html; charset=utf-8", PORTAL_PAGE);
}

void MakerindoWiFiManager::handleNetworkScan() {
  const int count = WiFi.scanNetworks();
  if (count < 0) {
    WiFi.scanDelete();
    sendJsonError(503, "Pemindaian Wi-Fi gagal. Coba lagi.");
    return;
  }

  String response = "[";
  for (int i = 0; i < count; ++i) {
    if (i > 0) {
      response += ',';
    }
    response += "{\"ssid\":\"";
    response += jsonEscape(WiFi.SSID(i));
    response += "\",\"rssi\":";
    response += String(WiFi.RSSI(i));
    response += ",\"secure\":";
    response += WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true";
    response += '}';
  }
  response += ']';
  WiFi.scanDelete();
  server_.send(200, "application/json; charset=utf-8", response);
}

void MakerindoWiFiManager::handleConnect() {
  if (!server_.hasArg("ssid") || !server_.hasArg("password")) {
    sendJsonError(400, "Nama jaringan dan kata sandi harus dikirim.");
    return;
  }
  if (!preferencesReady_) {
    sendJsonError(500, "Penyimpanan Wi-Fi tidak tersedia.");
    return;
  }

  const String ssid = server_.arg("ssid");
  const String password = server_.arg("password");
  if (ssid.isEmpty() || ssid.length() > 32 || password.length() > 64) {
    sendJsonError(400, "Nama jaringan atau kata sandi tidak valid.");
    return;
  }

  WiFi.disconnect(false, false);
  WiFi.begin(ssid.c_str(), password.c_str());

  const uint32_t startedAt = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - startedAt < connectionTimeoutMs_) {
    delay(250);
  }

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect(false, false);
    sendJsonError(
        400,
        "Tidak dapat terhubung. Periksa nama jaringan dan kata sandi, lalu coba lagi.");
    return;
  }

  preferences_.putString("ssid", ssid);
  preferences_.putString("password", password);
  if (preferences_.getString("ssid", "") != ssid ||
      preferences_.getString("password", String("\x01")) != password) {
    sendJsonError(
        500, "Wi-Fi terhubung, tetapi pengaturan gagal disimpan. Coba lagi.");
    return;
  }

  const String response =
      String("{\"ssid\":\"") + jsonEscape(ssid) + "\",\"ip\":\"" +
      WiFi.localIP().toString() + "\"}";
  server_.send(200, "application/json; charset=utf-8", response);
  Serial.printf("[WiFiManager] Terhubung ke %s, IP: %s\n", ssid.c_str(),
                WiFi.localIP().toString().c_str());
}

void MakerindoWiFiManager::sendJsonError(int statusCode,
                                         const String& message) {
  server_.send(statusCode, "application/json; charset=utf-8",
               String("{\"message\":\"") + jsonEscape(message) + "\"}");
}

bool MakerindoWiFiManager::connectSavedNetwork() {
  if (!preferencesReady_) {
    return false;
  }

  const String ssid = preferences_.getString("ssid", "");
  const String password = preferences_.getString("password", "");
  if (ssid.isEmpty()) {
    Serial.println(
        "[WiFiManager] Belum ada Wi-Fi tersimpan; gunakan portal untuk konfigurasi.");
    return false;
  }

  Serial.printf("[WiFiManager] Mencoba Wi-Fi tersimpan: %s\n", ssid.c_str());
  WiFi.begin(ssid.c_str(), password.c_str());
  const uint32_t startedAt = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - startedAt < connectionTimeoutMs_) {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFiManager] Wi-Fi tersambung, IP: %s\n",
                  WiFi.localIP().toString().c_str());
    return true;
  }

  Serial.println(
      "[WiFiManager] Gagal tersambung ke Wi-Fi tersimpan; portal tetap aktif.");
  return false;
}

String MakerindoWiFiManager::jsonEscape(const String& value) {
  String escaped;
  escaped.reserve(value.length() + 8);

  for (size_t i = 0; i < value.length(); ++i) {
    const char character = value[i];
    switch (character) {
      case '"':
        escaped += F("\\\"");
        break;
      case '\\':
        escaped += F("\\\\");
        break;
      case '\b':
        escaped += F("\\b");
        break;
      case '\f':
        escaped += F("\\f");
        break;
      case '\n':
        escaped += F("\\n");
        break;
      case '\r':
        escaped += F("\\r");
        break;
      case '\t':
        escaped += F("\\t");
        break;
      default:
        if (static_cast<uint8_t>(character) < 0x20) {
          escaped += ' ';
        } else {
          escaped += character;
        }
        break;
    }
  }

  return escaped;
}
