// JK-BMS Web — віддалений доступ до JK-BMS по BLE через вебінтерфейс.
// ESP32-S3 N16R8, Arduino core 3.x (pioarduino), NimBLE-Arduino 2.x.

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <esp_system.h>
#include <map>
#include <vector>
#include <mutex>

#include "config.h"
#include "event_log.h"
#include "settings.h"
#include "time_sync.h"
#include "mains_log.h"
#include "ble_manager.h"
#include "web_ui.h"

#ifndef FW_VERSION
#define FW_VERSION "1.0.0"
#endif

static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");
static AsyncAuthenticationMiddleware auth;
static DNSServer dns;

// ---------- Стан сесії ----------
static std::mutex clientsMtx;
static std::map<uint32_t, uint32_t> clientVisibleHb;   // id клієнта -> час останнього видимого heartbeat
static volatile uint32_t lastActivityMs = 0;
static volatile uint32_t sessionStartMs = 0;
static bool wasLinked = false;
static uint32_t restartAtMs = 0;
static uint32_t lastBroadcastLogId = 0;
#if MAINS_SENSE_ENABLED
static uint32_t lastBroadcastMainsVer = 0;   // версія журналу мережі, яку вже розіслано
#endif

// ---------- Мережа ----------
static bool apActive = false;
static bool netServicesStarted = false;
static uint32_t staConnectedSinceMs = 0;
static uint32_t wifiStartMs = 0;

static const char *resetReasonStr() {
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON:  return "Увімкнення живлення";
        case ESP_RST_SW:       return "Програмний перезапуск";
        case ESP_RST_PANIC:    return "Паніка (exception)";
        case ESP_RST_INT_WDT:  return "Interrupt watchdog";
        case ESP_RST_TASK_WDT: return "Task watchdog";
        case ESP_RST_WDT:      return "Watchdog";
        case ESP_RST_BROWNOUT: return "Просідання живлення";
        case ESP_RST_DEEPSLEEP:return "Вихід зі сну";
        case ESP_RST_EXT:      return "Зовнішній reset";
        default:               return "Невідомо";
    }
}

// Число з фіксованою кількістю знаків (без хвостів на кшталт 3.2999999)
static inline auto fx(float v, int digits) { return serialized(String(v, digits)); }

// ---------- Індикатор мережі ----------
#if MAINS_SENSE_ENABLED
static bool mainsOn = false;   // стабільний (після антидребезгу) стан; пише лише loop()

static bool mainsRaw() { return digitalRead(MAINS_SENSE_PIN) == MAINS_ACTIVE_LEVEL; }

static void setupMains() {
    // Для оптопари (активний LOW) потрібен підтяг до 3V3; з дільником він не потрібен.
    pinMode(MAINS_SENSE_PIN, MAINS_ACTIVE_LEVEL == LOW ? INPUT_PULLUP : INPUT);
    delay(20);   // дати піну встановитися (на випадок фільтрувального конденсатора)
    mainsOn = mainsRaw();
    LOGI("Мережа (GPIO%d): %s", MAINS_SENSE_PIN, mainsOn ? "є" : "немає");
    mainslog::begin(mainsOn);
}

static void loopMains() {
    static uint32_t diffSince = 0;   // з якого моменту сирий стан відрізняється від прийнятого
    mainslog::tick();
    const bool raw = mainsRaw();
    if (raw == mainsOn) {
        diffSince = 0;
        return;
    }
    const uint32_t now = millis();
    if (!diffSince) diffSince = now ? now : 1;
    if (now - diffSince < MAINS_DEBOUNCE_MS) return;
    mainsOn = raw;
    const uint32_t eventMs = diffSince;   // реальний момент зміни сигналу, а не кінець антидребезгу
    diffSince = 0;
    mainslog::record(mainsOn, eventMs);
    if (mainsOn) LOGI("Мережа з'явилась");
    else LOGW("Мережа зникла");
}
#endif

static void touchActivity() { lastActivityMs = millis(); }

static int activeClients() {
    std::lock_guard<std::mutex> lk(clientsMtx);
    int n = 0;
    uint32_t now = millis();
    for (auto &kv : clientVisibleHb)
        if (kv.second && now - kv.second < CLIENT_HB_VALID_MS) n++;
    return n;
}

static bool linkedState(BleState s) {
    return s == BleState::Connecting || s == BleState::Discovering ||
           s == BleState::WaitInfo || s == BleState::Streaming;
}

// ================= JSON стану =================

static void buildState(JsonDocument &doc, bool fullLog, uint32_t logAfter) {
    // Статичний, щоб не тримати великий об'єкт на стеку.
    // Тому buildState() викликається ЛИШЕ з loop() — не з колбеків AsyncTCP.
    static BleSnapshot s;
    Ble.snapshot(s);
    const uint32_t now = millis();

    doc["t"] = "state";
    doc["st"] = bleStateKey(s.state);
    doc["msg"] = s.message;
#if MAINS_SENSE_ENABLED
    doc["mains"] = mainsOn;
    doc["tm"] = timeValid() ? (uint32_t)time(nullptr) : 0;   // «зараз» за годинником ESP — для тривалості поточного стану
    // Журнал змін шлемо лише новим клієнтам і коли він змінився
    if (fullLog || mainslog::version() != lastBroadcastMainsVer) {
        mainslog::toJson(doc["mh"].to<JsonArray>());
        if (!fullLog) lastBroadcastMainsVer = mainslog::version();
    }
#endif

    if (!s.mac.empty()) {
        JsonObject tg = doc["target"].to<JsonObject>();
        tg["mac"] = s.mac;
        tg["name"] = s.name;
        tg["alias"] = settings::alias(s.mac.c_str());
        tg["proto"] = jk::protoName(s.proto);
        tg["forced"] = s.protoForced;
        tg["rssi"] = s.rssi;
        tg["connS"] = s.connectedAtMs ? (now - s.connectedAtMs) / 1000 : 0;
        tg["ageMs"] = s.lastFrameMs ? now - s.lastFrameMs : -1;
        tg["frames"] = s.frames;
        tg["crc"] = s.crcErrors;
    }

    if (linkedState(s.state)) {
        int32_t idle = (int32_t)IDLE_TIMEOUT_MS - (int32_t)(now - lastActivityMs);
        int32_t sess = (int32_t)SESSION_LIMIT_MS - (int32_t)(now - sessionStartMs);
        doc["idleLeft"] = idle > 0 ? idle / 1000 : 0;
        doc["sessLeft"] = sess > 0 ? sess / 1000 : 0;
    }

    // Сканування
    JsonObject sc = doc["scan"].to<JsonObject>();
    sc["running"] = s.state == BleState::Scanning;
    sc["leftS"] = s.state == BleState::Scanning
                      ? (int32_t)max<int32_t>(0, ((int32_t)SCAN_DURATION_MS - (int32_t)(now - s.scanStartedMs)) / 1000)
                      : 0;
    sc["agoS"] = s.lastScanMs ? (now - s.lastScanMs) / 1000 : -1;
    JsonArray devs = sc["devices"].to<JsonArray>();
    for (auto &d : s.devices) {
        JsonObject o = devs.add<JsonObject>();
        o["mac"] = d.mac;
        o["name"] = d.name;
        o["rssi"] = d.rssi;
        o["type"] = d.addrType;
        if (d.jk) o["jk"] = true;
        o["ago"] = (now - d.lastSeenMs) / 1000;
    }

    // Дані BMS
    if (s.data.valid) {
        const auto &c = s.data;
        JsonObject d = doc["data"].to<JsonObject>();
        d["v"] = fx(c.voltage, 2);
        d["i"] = fx(c.current, 2);
        d["p"] = fx(c.power, 0);
        d["soc"] = c.soc;
        d["capR"] = fx(c.capRemain, 1);
        d["capN"] = fx(c.capNominal, 1);
        d["cyc"] = c.cycles;
        d["cycCap"] = fx(c.cycleCapacity, 0);
        d["cMin"] = fx(c.cellMin, 3);
        d["cMax"] = fx(c.cellMax, 3);
        d["cMinI"] = c.cellMinIdx;
        d["cMaxI"] = c.cellMaxIdx;
        d["cAvg"] = fx(c.cellAvg, 3);
        d["cDelta"] = fx(c.cellDelta, 3);
        d["tMos"] = fx(c.tMos, 1);
        d["t1"] = fx(c.t1, 1);
        d["t2"] = fx(c.t2, 1);
        d["chg"] = c.chargeOn;
        d["dsg"] = c.dischargeOn;
        d["bal"] = c.balancing;
        d["balI"] = fx(c.balanceCurrent, 3);
        d["run"] = c.runtimeS;
        d["err"] = c.errors;
        d["errTxt"] = jk::errorsToString(c.errors);
        JsonArray cells = d["cells"].to<JsonArray>();
        for (uint8_t i = 0; i < c.cellCount; i++) cells.add(fx(c.cellV[i], 3));
    }
    if (s.info.valid) {
        JsonObject in = doc["info"].to<JsonObject>();
        in["model"] = s.info.model;
        in["hw"] = s.info.hwVersion;
        in["sw"] = s.info.swVersion;
        in["name"] = s.info.name;
        in["serial"] = s.info.serial;
        in["mfg"] = s.info.mfgDate;
        in["pwrOn"] = s.info.powerOnCount;
    }

    // Діагностика
    JsonObject dg = doc["diag"].to<JsonObject>();
    dg["fw"] = FW_VERSION;
    dg["up"] = now / 1000;
    dg["heap"] = ESP.getFreeHeap();
    dg["heapMin"] = ESP.getMinFreeHeap();
    dg["psram"] = ESP.getFreePsram();
    dg["reset"] = resetReasonStr();
    dg["wifiRssi"] = WiFi.isConnected() ? WiFi.RSSI() : 0;
    dg["ip"] = WiFi.isConnected() ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    dg["clients"] = ws.count();
    dg["active"] = activeClients();
    dg["defPass"] = settings::isDefaultPass();
    dg["tsrc"] = timesync::source();
    dg["tage"] = timesync::lastSyncAgeS();

    JsonObject al = doc["aliases"].to<JsonObject>();
    settings::aliasesToJson(al);
    JsonObject pr = doc["protos"].to<JsonObject>();
    settings::protoToJson(pr);

    JsonArray logs = doc["log"].to<JsonArray>();
    uint32_t last = evlog::dump(logs, fullLog ? 0 : logAfter);
    if (!fullLog) lastBroadcastLogId = last;
}

// Новим клієнтам надсилаємо повний стан (з усім журналом) із loop(),
// а не з колбека AsyncTCP: buildState() не реентерабельна.
static std::vector<uint32_t> pendingFullState;   // під clientsMtx

static void sendPendingFullState() {
    std::vector<uint32_t> ids;
    {
        std::lock_guard<std::mutex> lk(clientsMtx);
        ids.swap(pendingFullState);
    }
    if (ids.empty()) return;
    JsonDocument doc;
    buildState(doc, true, 0);
    String out;
    serializeJson(doc, out);
    for (uint32_t id : ids) ws.text(id, out);   // якщо клієнт уже пішов — просто false
}

static void broadcastState() {
    if (ws.count() == 0) {
        // Нікого немає — просто зсуваємо курсор логу
        JsonDocument tmp;
        lastBroadcastLogId = evlog::dump(tmp.to<JsonArray>(), lastBroadcastLogId);
        return;
    }
    JsonDocument doc;
    buildState(doc, false, lastBroadcastLogId);
    String out;
    serializeJson(doc, out);
    ws.textAll(out);
}

// ================= Команди з вебсокета =================

static void handleCommand(AsyncWebSocketClient *client, const char *data, size_t len) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) return;
    const char *cmd = doc["cmd"] | "";

    if (!strcmp(cmd, "hb")) {
        bool vis = doc["vis"] | false;
        {
            std::lock_guard<std::mutex> lk(clientsMtx);
            clientVisibleHb[client->id()] = vis ? millis() : 0;
        }
        if (vis) touchActivity();
        return;
    }

    touchActivity();

    if (!strcmp(cmd, "scan")) {
        Ble.requestScan();
    } else if (!strcmp(cmd, "connect")) {
        String mac = doc["mac"] | "";
        mac.trim();
        mac.toUpperCase();
        if (mac.length() != 17) return;
        // Тип адреси та ім'я беремо з результатів сканування
        uint8_t type = 0;   // JK-B використовує публічну адресу
        std::string name;
        Ble.lookupScan(mac.c_str(), type, name);
        sessionStartMs = millis();
        Ble.requestConnect(mac.c_str(), type, name, settings::protoOverride(mac));
    } else if (!strcmp(cmd, "disconnect")) {
        Ble.requestDisconnect("Відключено користувачем");
    } else if (!strcmp(cmd, "extend")) {
        sessionStartMs = millis();
        LOGI("Сесію продовжено");
    } else if (!strcmp(cmd, "alias")) {
        settings::setAlias(doc["mac"] | "", doc["name"] | "");
    } else if (!strcmp(cmd, "proto")) {
        int p = doc["p"] | 0;
        jk::Proto pr = p == 24 ? jk::Proto::JK02_24S : p == 32 ? jk::Proto::JK02_32S : jk::Proto::Unknown;
        settings::setProtoOverride(doc["mac"] | "", pr);
        LOGI("Протокол для %s: %s", (const char *)(doc["mac"] | ""), pr == jk::Proto::Unknown ? "авто" : jk::protoName(pr));
    }
}

static void onWsEvent(AsyncWebSocket *, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data,
                      size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            LOGI("Вебклієнт #%lu підключився (%s)", client->id(), client->remoteIP().toString().c_str());
            {
                std::lock_guard<std::mutex> lk(clientsMtx);
                clientVisibleHb[client->id()] = millis();
                pendingFullState.push_back(client->id());
            }
            touchActivity();
            break;
        case WS_EVT_DISCONNECT: {
            std::lock_guard<std::mutex> lk(clientsMtx);
            clientVisibleHb.erase(client->id());
            break;
        }
        case WS_EVT_DATA: {
            auto *info = (AwsFrameInfo *)arg;
            if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
                handleCommand(client, (const char *)data, len);
            break;
        }
        default:
            break;
    }
}

// ================= HTTP =================

static void setupHttp() {
    auth.setUsername(WEB_USER);
    auth.setPassword(settings::webPass().c_str());
    auth.setRealm("JK-BMS Web");
    auth.setAuthFailureMessage("Потрібна автентифікація");
    auth.setAuthType(AsyncAuthType::AUTH_BASIC);
    auth.generateHash();

    ws.onEvent(onWsEvent);
    ws.addMiddleware(&auth);
    server.addHandler(&ws);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *r) {
        if (apActive && !WiFi.isConnected() && settings::wifiSsid().isEmpty()) return r->redirect("/setup");
        r->send(200, "text/html; charset=utf-8", (const uint8_t *)INDEX_HTML, strlen(INDEX_HTML));
    }).addMiddleware(&auth);

    // Сторінка налаштування Wi-Fi: без пароля, але лише коли піднята точка доступу
    server.on("/setup", HTTP_GET, [](AsyncWebServerRequest *r) {
        if (!apActive) return r->send(404);
        r->send(200, "text/html; charset=utf-8", (const uint8_t *)SETUP_HTML, strlen(SETUP_HTML));
    });
    server.on("/setup", HTTP_POST, [](AsyncWebServerRequest *r) {
        if (!apActive) return r->send(404);
        if (!r->hasParam("ssid", true)) return r->send(400, "text/plain", "ssid?");
        settings::setWifi(r->getParam("ssid", true)->value(), r->hasParam("pass", true) ? r->getParam("pass", true)->value() : "");
        LOGI("Wi-Fi збережено, перезапуск");
        r->send(200, "text/plain; charset=utf-8", "Збережено. Перезапуск…");
        restartAtMs = millis() + 1500;
    });

    server.on("/api/wifi", HTTP_POST, [](AsyncWebServerRequest *r) {
        if (!r->hasParam("ssid", true)) return r->send(400, "text/plain", "ssid?");
        settings::setWifi(r->getParam("ssid", true)->value(), r->hasParam("pass", true) ? r->getParam("pass", true)->value() : "");
        LOGI("Wi-Fi змінено, перезапуск");
        r->send(200, "text/plain; charset=utf-8", "OK");
        restartAtMs = millis() + 1500;
    }).addMiddleware(&auth);

    server.on("/api/password", HTTP_POST, [](AsyncWebServerRequest *r) {
        if (!r->hasParam("p", true)) return r->send(400, "text/plain", "p?");
        String p = r->getParam("p", true)->value();
        if (p.length() < 6) return r->send(400, "text/plain; charset=utf-8", "Мінімум 6 символів");
        settings::setWebPass(p);
        LOGI("Пароль змінено, перезапуск");
        r->send(200, "text/plain; charset=utf-8", "OK");
        restartAtMs = millis() + 1500;
    }).addMiddleware(&auth);

    server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest *r) {
        LOGI("Перезапуск з вебінтерфейсу");
        r->send(200, "text/plain", "OK");
        restartAtMs = millis() + 1000;
    }).addMiddleware(&auth);

    // Оновлення прошивки через браузер (firmware.bin)
    server.on(
        "/update", HTTP_POST,
        [](AsyncWebServerRequest *r) {
            bool ok = !Update.hasError();
            r->send(ok ? 200 : 500, "text/plain; charset=utf-8", ok ? "OK, перезапуск…" : Update.errorString());
            if (ok) restartAtMs = millis() + 1500;
        },
        [](AsyncWebServerRequest *r, const String &fn, size_t index, uint8_t *data, size_t len, bool final) {
            if (index == 0) {
                Ble.requestDisconnect("Оновлення прошивки");
                LOGI("Оновлення прошивки: %s", fn.c_str());
                if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) LOGE("Update.begin: %s", Update.errorString());
            }
            if (!Update.hasError() && Update.write(data, len) != len) LOGE("Update.write: %s", Update.errorString());
            if (final) {
                if (Update.end(true)) LOGI("Прошивку записано: %u байт", (unsigned)(index + len));
                else LOGE("Update.end: %s", Update.errorString());
            }
        }).addMiddleware(&auth);

    server.onNotFound([](AsyncWebServerRequest *r) {
        if (apActive) return r->redirect("http://192.168.4.1/setup");   // captive portal
        r->send(404, "text/plain", "Not found");
    });

    server.begin();
}

// ================= Wi-Fi =================

static void startAp() {
    if (apActive) return;
    WiFi.mode(settings::wifiSsid().isEmpty() ? WIFI_AP : WIFI_AP_STA);
    WiFi.softAP(SETUP_AP_SSID, SETUP_AP_PASS);
    dns.start(53, "*", WiFi.softAPIP());
    apActive = true;
    LOGW("Точка доступу %s, http://%s/setup", SETUP_AP_SSID, WiFi.softAPIP().toString().c_str());
}

static void stopAp() {
    if (!apActive) return;
    dns.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    apActive = false;
    LOGI("Точку доступу вимкнено");
}

static void startNetServices() {
    if (netServicesStarted) return;
    netServicesStarted = true;
    timesync::begin();   // часовий пояс + time.now (запасний NTP)
    ArduinoOTA.setHostname(HOSTNAME);
    ArduinoOTA.setPassword(settings::webPass().c_str());
    ArduinoOTA.onStart([]() { Ble.requestDisconnect("Оновлення прошивки (OTA)"); });
    ArduinoOTA.begin();   // також запускає mDNS
    MDNS.addService("http", "tcp", 80);
    LOGI("http://%s.local  (%s)", HOSTNAME, WiFi.localIP().toString().c_str());
}

static void setupWifi() {
    WiFi.persistent(false);
    WiFi.setHostname(HOSTNAME);
    // Увага: при одночасній роботі Wi-Fi і BLE modem sleep має лишатися увімкненим
    // (WiFi.setSleep(false) з BLE призводить до abort() у ESP-IDF).
    WiFi.setAutoReconnect(true);
    String ssid = settings::wifiSsid();
    if (ssid.isEmpty()) {
        startAp();
        return;
    }
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), settings::wifiPass().c_str());
    wifiStartMs = millis();
    LOGI("Підключення до Wi-Fi %s…", ssid.c_str());
}

static void loopWifi() {
    const uint32_t now = millis();
    if (apActive) dns.processNextRequest();

    if (WiFi.isConnected()) {
        if (!staConnectedSinceMs) {
            staConnectedSinceMs = now;
            LOGI("Wi-Fi підключено, IP %s, RSSI %d", WiFi.localIP().toString().c_str(), WiFi.RSSI());
        }
        startNetServices();
        // AP більше не потрібна — вимикаємо через хвилину стабільного з'єднання
        if (apActive && now - staConnectedSinceMs > 60000 && WiFi.softAPgetStationNum() == 0) stopAp();
    } else {
        if (staConnectedSinceMs) LOGW("Wi-Fi втрачено");
        staConnectedSinceMs = 0;
        if (!apActive && wifiStartMs && now - wifiStartMs > WIFI_CONNECT_TIMEOUT) startAp();
    }
}

// ================= Сесія і сторожі =================

static void loopSession() {
    const uint32_t now = millis();
    bool linked = linkedState(Ble.state());

    if (linked && !wasLinked) {
        sessionStartMs = now;
        touchActivity();
    }
    wasLinked = linked;
    if (!linked) return;

    // Видимий клієнт продовжує активність автоматично
    if (activeClients() > 0) {
        std::lock_guard<std::mutex> lk(clientsMtx);
        for (auto &kv : clientVisibleHb)
            if (kv.second && kv.second > lastActivityMs) lastActivityMs = kv.second;
    }

    // Пристрій не відповідає на GATT (не JK-BMS): BLE-задача заблокована, тож розриваємо ззовні.
    static uint32_t abortedDiscover = 0;
    uint32_t dStart = Ble.discoverStartMs();
    if (dStart && dStart != abortedDiscover && now - dStart > DISCOVER_TIMEOUT_MS) {
        abortedDiscover = dStart;
        Ble.requestDisconnect("Пристрій не відповідає на пошук сервісів (не JK-BMS?)", true);
    }

    if (now - lastActivityMs > IDLE_TIMEOUT_MS) {
        Ble.requestDisconnect("Простій 3 хв: немає жодного активного вебклієнта");
        wasLinked = false;
    } else if (now - sessionStartMs > SESSION_LIMIT_MS) {
        Ble.requestDisconnect("Досягнуто ліміт сесії 30 хв");
        wasLinked = false;
    }
}

static void loopWatchdog() {
    uint32_t beat = Ble.heartbeatMs();
    if (beat && millis() - beat > BLE_TASK_STALL_MS) {
        LOGE("BLE-задача не відповідає %lu с — перезапуск", (millis() - beat) / 1000);
        delay(200);
        ESP.restart();
    }
}

// ================= setup / loop =================

void setup() {
    Serial.begin(115200);
    delay(300);
    LOGI("JK-BMS Web %s, причина старту: %s", FW_VERSION, resetReasonStr());

#if MAINS_SENSE_ENABLED
    setupMains();
#endif
    settings::begin();
    setupWifi();
    setupHttp();
    Ble.begin();
    enableLoopWDT();
}

void loop() {
    static uint32_t lastBroadcast = 0, lastCleanup = 0;
    const uint32_t now = millis();

#if MAINS_SENSE_ENABLED
    loopMains();
#endif
    loopWifi();
    if (netServicesStarted) ArduinoOTA.handle();
    loopSession();
    loopWatchdog();

    sendPendingFullState();
    if (now - lastBroadcast >= 1000) {
        lastBroadcast = now;
        broadcastState();
    }
    if (now - lastCleanup >= 5000) {
        lastCleanup = now;
        ws.cleanupClients(4);
    }
    if (restartAtMs && (int32_t)(now - restartAtMs) > 0) {
        Ble.requestDisconnect("Перезапуск");
        delay(500);
        ESP.restart();
    }
    delay(10);
}
