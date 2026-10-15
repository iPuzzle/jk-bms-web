#include "ble_manager.h"
#include <NimBLEDevice.h>
#include <mutex>
#include <map>
#include <atomic>
#include "config.h"
#include "event_log.h"

BleManager Ble;

const char *bleStateKey(BleState s) {
    switch (s) {
        case BleState::Idle:        return "idle";
        case BleState::Scanning:    return "scanning";
        case BleState::Connecting:  return "connecting";
        case BleState::Discovering: return "discovering";
        case BleState::WaitInfo:    return "waitinfo";
        case BleState::Streaming:   return "streaming";
        case BleState::Error:       return "error";
    }
    return "?";
}

namespace {

// ---- Спільний стан (захищений mtx) ----
std::mutex mtx;
BleSnapshot st;
std::map<std::string, ScanEntry> scanMap;
jk::FrameAssembler assembler;
uint8_t pendingCell[jk::FRAME_MAX];      // кадр cell info, що прийшов до визначення протоколу
size_t pendingCellLen = 0;

// ---- Запити від вебу ----
struct Request {
    bool scan = false;
    bool connect = false;
    bool disconnect = false;
    std::string mac, name, reason;
    bool disconnectError = false;
    uint8_t addrType = 0;
    jk::Proto forced = jk::Proto::Unknown;
} req;

// ---- Прапорці з колбеків ----
std::atomic<bool> peerDisconnected{false};
std::atomic<int> peerDiscReason{0};
std::atomic<bool> gotDevInfo{false};
std::atomic<bool> scanEnded{false};
std::atomic<bool> linkUp{false};          // onConnect: з'єднання повністю встановлено
std::atomic<bool> connectFailed{false};   // onConnectFail: спроба з'єднання провалилась
constexpr uint16_t NO_CONN = 0xFFFF;      // BLE_HS_CONN_HANDLE_NONE
std::atomic<uint16_t> connHandle{NO_CONN};   // handle активного лінка (для аварійного розриву)
std::atomic<uint32_t> discoverSince{0};      // початок етапу Discovering

// ---- Лише для задачі BLE ----
NimBLEClient *client = nullptr;
NimBLERemoteCharacteristic *chr = nullptr;
uint32_t waitInfoSince = 0;
uint32_t streamSince = 0;
bool cellCmdResent = false;
uint32_t lastRssiPoll = 0;

class ScanCB : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice *d) override {
        Ble.onScanResult(d->getAddress().toString(), d->getAddress().getType(), d->getName(), d->getRSSI(),
                         d->isAdvertisingService(NimBLEUUID((uint16_t)0xFFE0)));
    }
    void onScanEnd(const NimBLEScanResults &, int) override { Ble.onScanEnd(); }
} scanCb;

class ClientCB : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient *c) override {
        connHandle = c->getConnHandle();
        linkUp = true;
    }
    void onConnectFail(NimBLEClient *, int) override { connectFailed = true; }
    void onDisconnect(NimBLEClient *, int reason) override {
        connHandle = NO_CONN;
        Ble.onPeerDisconnected(reason);
    }
} clientCb;

// Чи просили відключитися (забирає запит). Для скасування під час підключення/пошуку сервісів.
bool takeDisconnectRequest(std::string &reason, bool &error) {
    std::lock_guard<std::mutex> lk(mtx);
    if (!req.disconnect) return false;
    req.disconnect = false;
    reason = req.reason;
    error = req.disconnectError;
    return true;
}

void notifyCb(NimBLERemoteCharacteristic *, uint8_t *data, size_t len, bool) {
    Ble.onNotify(data, len);
}

std::string upper(std::string s) {
    for (auto &c : s) c = toupper(c);
    return s;
}

}  // namespace

// ================= Публічний API =================

void BleManager::begin() {
    // Ядро 0 — там же працює стек BLE; вебсервер і loop() на ядрі 1.
    xTaskCreatePinnedToCore(taskEntry, "ble", 8192, this, 3, nullptr, 0);
}

void BleManager::requestScan() {
    std::lock_guard<std::mutex> lk(mtx);
    req.scan = true;
}

void BleManager::requestConnect(const std::string &mac, uint8_t addrType, const std::string &name, jk::Proto forced) {
    std::lock_guard<std::mutex> lk(mtx);
    req.connect = true;
    req.disconnect = false;
    req.scan = false;
    req.mac = mac;
    req.addrType = addrType;
    req.name = name;
    req.forced = forced;
}

void BleManager::requestDisconnect(const char *reason, bool error) {
    {
        std::lock_guard<std::mutex> lk(mtx);
        req.disconnect = true;
        req.connect = false;
        req.reason = reason;
        req.disconnectError = error;
    }
    // Єдиний виняток із правила «NimBLE лише з задачі ble»: під час Discovering задача заблокована
    // у getService()/subscribe() (чекає відповіді GATT без таймауту), тож сама запит не побачить.
    // ble_gap_terminate() неблокуючий і потокобезпечний — він лише ставить HCI-команду в чергу;
    // розрив розблокує очікування, а далі прибирання робить сама задача ble.
    if (st.state == BleState::Discovering) {
        uint16_t h = connHandle;
        if (h != NO_CONN) ble_gap_terminate(h, BLE_ERR_REM_USER_CONN_TERM);
    }
}

uint32_t BleManager::discoverStartMs() const {
    return st.state == BleState::Discovering ? discoverSince.load() : 0;
}

bool BleManager::isConnected() const {
    BleState s = st.state;   // читання одного байта атомарне
    return s == BleState::Connecting || s == BleState::Discovering ||
           s == BleState::WaitInfo || s == BleState::Streaming;
}

BleState BleManager::state() const { return st.state; }   // один байт — атомарне читання

void BleManager::snapshot(BleSnapshot &out) {
    std::lock_guard<std::mutex> lk(mtx);
    out = st;
    out.devices.clear();
    out.devices.reserve(scanMap.size());
    for (auto &kv : scanMap) out.devices.push_back(kv.second);
}

bool BleManager::lookupScan(const std::string &mac, uint8_t &addrType, std::string &name) {
    std::lock_guard<std::mutex> lk(mtx);
    auto it = scanMap.find(upper(mac));
    if (it == scanMap.end()) return false;
    addrType = it->second.addrType;
    name = it->second.name;
    return true;
}

// ================= Колбеки =================

void BleManager::onScanResult(const std::string &mac, uint8_t type, const std::string &name, int rssi, bool hasFfe0) {
    std::string key = upper(mac);
    std::lock_guard<std::mutex> lk(mtx);
    auto it = scanMap.find(key);
    if (it == scanMap.end()) {
        if (scanMap.size() >= 80) return;
        ScanEntry e;
        e.mac = key;
        e.addrType = type;
        it = scanMap.emplace(key, e).first;
    }
    if (!name.empty()) it->second.name = name;
    if (hasFfe0) it->second.jk = true;
    it->second.rssi = rssi;
    it->second.lastSeenMs = millis();
}

void BleManager::onScanEnd() { scanEnded = true; }

void BleManager::onPeerDisconnected(int reason) {
    peerDiscReason = reason;
    peerDisconnected = true;
}

void BleManager::onNotify(const uint8_t *data, size_t len) {
    std::lock_guard<std::mutex> lk(mtx);
    if (!assembler.push(data, len)) {
        st.crcErrors = assembler.crcErrors();
        return;
    }
    const uint8_t *f = assembler.frame();
    size_t n = assembler.size();
    st.frames++;
    st.lastFrameMs = millis();

    switch (jk::frameType(f)) {
        case jk::FRAME_DEV_INFO: {
            jk::DeviceInfo di;
            if (jk::decodeDeviceInfo(f, n, di)) {
                st.info = di;
                if (!st.protoForced) {
                    jk::Proto p = jk::protoFromDeviceInfo(di);
                    if (p != jk::Proto::Unknown) st.proto = p;
                }
                gotDevInfo = true;
                // Якщо cell info прийшов раніше — розберемо його тепер
                if (pendingCellLen && st.proto != jk::Proto::Unknown) {
                    jk::decodeCellInfo(pendingCell, pendingCellLen, st.proto, st.data);
                    pendingCellLen = 0;
                }
            }
            break;
        }
        case jk::FRAME_CELL_INFO: {
            if (st.proto == jk::Proto::Unknown) {
                jk::Proto g = jk::guessProto(f, n);
                if (g != jk::Proto::Unknown) {
                    st.proto = g;
                } else {
                    memcpy(pendingCell, f, n);
                    pendingCellLen = n;
                    break;
                }
            }
            jk::decodeCellInfo(f, n, st.proto, st.data);
            break;
        }
        default:
            break;   // кадр налаштувань (0x01) не використовуємо
    }
}

// ================= Задача =================

void BleManager::taskEntry(void *arg) { static_cast<BleManager *>(arg)->taskLoop(); }

void BleManager::setState(BleState s, const std::string &msg) {
    std::lock_guard<std::mutex> lk(mtx);
    st.state = s;
    st.message = msg;
}

bool BleManager::sendCmd(uint8_t cmd) {
    if (!chr) return false;
    uint8_t buf[20];
    jk::buildCommand(cmd, buf);
    bool ok = chr->writeValue(buf, sizeof(buf), false);
    if (!ok) ok = chr->writeValue(buf, sizeof(buf), true);
    if (!ok) LOGW("Не вдалося надіслати команду 0x%02X", cmd);
    return ok;
}

void BleManager::doDisconnect(const std::string &reason, bool error) {
    chr = nullptr;
    // Після disconnect() isConnected() одразу стає false, тому підтвердження розриву
    // чекаємо за прапорцем з onDisconnect (до 3 с).
    if (client && client->isConnected() && !peerDisconnected) {
        client->disconnect();
        uint32_t t0 = millis();
        while (!peerDisconnected && millis() - t0 < 3000) vTaskDelay(pdMS_TO_TICKS(20));
        if (!peerDisconnected) LOGW("Розрив не підтверджено за 3 с");
    }
    peerDisconnected = false;

    std::string mac;
    {
        std::lock_guard<std::mutex> lk(mtx);
        mac = st.mac;
        st.state = error ? BleState::Error : BleState::Idle;
        st.message = reason;
        st.connectedAtMs = 0;
        st.rssi = 0;
    }
    if (error) LOGE("%s: %s", mac.c_str(), reason.c_str());
    else       LOGI("Відключено від %s: %s", mac.c_str(), reason.c_str());
}

void BleManager::doConnect() {
    std::string mac, name;
    uint8_t type;
    jk::Proto forced;
    {
        std::lock_guard<std::mutex> lk(mtx);
        mac = req.mac; name = req.name; type = req.addrType; forced = req.forced;
        req.connect = false;

        st.mac = mac;
        st.name = name;
        st.addrType = type;
        st.proto = forced;
        st.protoForced = forced != jk::Proto::Unknown;
        st.info = jk::DeviceInfo{};
        st.data = jk::CellData{};
        st.frames = 0;
        st.lastFrameMs = 0;
        st.rssi = 0;
        assembler.reset();
        pendingCellLen = 0;
        st.state = BleState::Connecting;
        st.message = "Встановлення BLE-з'єднання…";
    }
    gotDevInfo = false;
    peerDisconnected = false;
    linkUp = false;
    connectFailed = false;
    connHandle = NO_CONN;
    cellCmdResent = false;

    NimBLEScan *scan = NimBLEDevice::getScan();
    if (scan->isScanning()) {
        scan->stop();
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    LOGI("Підключення до %s (%s)…", mac.c_str(), name.c_str());
    // Асинхронний connect: BLE-задача не блокується, тож «Відключитися» працює і під час підключення.
    const char *failMsg = "Не вдалося встановити з'єднання (пристрій недоступний або зайнятий іншим клієнтом)";
    if (!client->connect(NimBLEAddress(mac, type), true, true)) {
        doDisconnect(failMsg, true);
        return;
    }
    std::string abortReason;
    bool abortError = false;
    const uint32_t t0 = millis();
    while (!linkUp) {
        beat_ = millis();
        if (connectFailed) { doDisconnect(failMsg, true); return; }
        if (takeDisconnectRequest(abortReason, abortError)) {
            client->cancelConnect();
            // Дочекаємось результату скасування (або встановленого з'єднання, яке тоді розірвемо)
            uint32_t w = millis();
            while (!connectFailed && !linkUp && millis() - w < 2000) vTaskDelay(pdMS_TO_TICKS(20));
            doDisconnect(abortReason, abortError);
            return;
        }
        if (millis() - t0 > CONNECT_TIMEOUT_MS + 2000) {
            client->cancelConnect();
            doDisconnect("Таймаут BLE-з'єднання", true);
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    beat_ = millis();

    // Етапи нижче блокуються в NimBLE без таймауту (чекають відповіді GATT). Вийти з них можна лише
    // розривом лінка: його робить requestDisconnect() (кнопка) або loop() за DISCOVER_TIMEOUT_MS.
    // Після кожного кроку спершу дивимось, чи це не наслідок запиту користувача.
    discoverSince = millis();
    setState(BleState::Discovering, "Пошук сервісу FFE0…");
    if (takeDisconnectRequest(abortReason, abortError)) { doDisconnect(abortReason, abortError); return; }
    NimBLERemoteService *svc = client->getService("ffe0");
    if (takeDisconnectRequest(abortReason, abortError)) { doDisconnect(abortReason, abortError); return; }
    if (!svc) { doDisconnect("Сервіс FFE0 не знайдено — це не JK-BMS?", true); return; }

    setState(BleState::Discovering, "Пошук характеристики FFE1…");
    NimBLERemoteCharacteristic *c = svc->getCharacteristic("ffe1");
    if (takeDisconnectRequest(abortReason, abortError)) { doDisconnect(abortReason, abortError); return; }
    if (!c) { doDisconnect("Характеристику FFE1 не знайдено", true); return; }

    setState(BleState::Discovering, "Підписка на нотифікації…");
    bool subscribed = c->canNotify() && c->subscribe(true, notifyCb, false);
    if (takeDisconnectRequest(abortReason, abortError)) { doDisconnect(abortReason, abortError); return; }
    if (!subscribed) {
        doDisconnect("Не вдалося підписатися на нотифікації FFE1", true);
        return;
    }
    chr = c;

    {
        std::lock_guard<std::mutex> lk(mtx);
        st.connectedAtMs = millis();
        st.state = BleState::WaitInfo;
        st.message = "Запит інформації про пристрій…";
    }
    LOGI("З'єднано з %s, MTU %u", mac.c_str(), client->getMTU());
    sendCmd(jk::CMD_DEVICE_INFO);
    waitInfoSince = millis();
}

void BleManager::taskLoop() {
    NimBLEDevice::init("");
    NimBLEDevice::setPower(9);

    NimBLEScan *scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(&scanCb, true);   // з дублікатами, щоб оновлювати RSSI
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(80);

    client = NimBLEDevice::createClient();
    client->setClientCallbacks(&clientCb, false);
    client->setConnectTimeout(CONNECT_TIMEOUT_MS);
    // Інтервал з'єднання 30–50 мс, latency 0, supervision 2.56 с — як у NimBLE за замовчуванням;
    // міняємо лише вікно сканування під час connect(), щоб не душити Wi-Fi.
    client->setConnectionParams(24, 40, 0, 256, CONNECT_SCAN_ITVL, CONNECT_SCAN_WINDOW);

    LOGI("BLE готовий");

    for (;;) {
        beat_ = millis();
        const uint32_t now = millis();

        // ---- Забираємо запити ----
        bool wantScan, wantConnect, wantDisc, discError;
        std::string discReason;
        {
            std::lock_guard<std::mutex> lk(mtx);
            wantScan = req.scan;
            wantConnect = req.connect;
            wantDisc = req.disconnect;
            discReason = req.reason;
            discError = req.disconnectError;
            req.scan = false;
            req.disconnect = false;
        }
        BleState s = st.state;
        bool linked = s == BleState::WaitInfo || s == BleState::Streaming || s == BleState::Discovering;

        if (wantDisc) {
            if (linked) doDisconnect(discReason, discError);
            else if (s == BleState::Scanning) scan->stop();
        }

        if (wantConnect) {
            if (linked) doDisconnect("Перемикання на інший пристрій", false);
            doConnect();
            continue;
        }

        if (wantScan && (s == BleState::Idle || s == BleState::Error)) {
            {
                std::lock_guard<std::mutex> lk(mtx);
                scanMap.clear();
                st.state = BleState::Scanning;
                st.message = "Сканування…";
                st.scanStartedMs = now;
            }
            scanEnded = false;
            if (!scan->start(SCAN_DURATION_MS, false, true)) {
                setState(BleState::Error, "Не вдалося запустити сканування");
                LOGE("Не вдалося запустити сканування");
            } else {
                LOGI("Сканування %lu с…", SCAN_DURATION_MS / 1000);
            }
        }

        // ---- Кінець сканування ----
        if (scanEnded) {
            scanEnded = false;
            size_t n;
            {
                std::lock_guard<std::mutex> lk(mtx);
                if (st.state == BleState::Scanning) {
                    st.state = BleState::Idle;
                    st.message = "";
                }
                st.lastScanMs = millis();
                n = scanMap.size();
            }
            LOGI("Сканування завершено, знайдено %u пристроїв", (unsigned)n);
        }

        // ---- Розрив з боку BMS ----
        s = st.state;
        linked = s == BleState::WaitInfo || s == BleState::Streaming;
        if (peerDisconnected && linked) {
            char buf[64];
            snprintf(buf, sizeof(buf), "З'єднання розірвано (код %d)", (int)peerDiscReason);
            doDisconnect(buf, true);
            continue;
        }
        peerDisconnected = false;

        // ---- Очікування device info ----
        if (s == BleState::WaitInfo) {
            if (gotDevInfo || now - waitInfoSince > DEVINFO_WAIT_MS) {
                if (!gotDevInfo) LOGW("Device info не отримано, запитую дані комірок");
                else {
                    std::lock_guard<std::mutex> lk(mtx);
                    LOGI("BMS: %s, HW %s, SW %s, протокол %s%s", st.info.model.c_str(),
                         st.info.hwVersion.c_str(), st.info.swVersion.c_str(), jk::protoName(st.proto),
                         st.protoForced ? " (вручну)" : "");
                }
                vTaskDelay(pdMS_TO_TICKS(200));
                sendCmd(jk::CMD_CELL_INFO);
                streamSince = millis();
                setState(BleState::Streaming, "Отримання даних");
            }
        }

        // ---- Потік даних ----
        if (s == BleState::Streaming) {
            uint32_t last = st.lastFrameMs;
            uint32_t since = last ? last : streamSince;
            if (!cellCmdResent && !st.data.valid && now - streamSince > 5000) {
                LOGW("Даних немає 5 с, повторюю запит");
                sendCmd(jk::CMD_CELL_INFO);
                cellCmdResent = true;
            }
            if (now - since > STALE_DATA_MS) {
                doDisconnect("Немає даних від BMS понад " + std::to_string(STALE_DATA_MS / 1000) + " с", true);
                continue;
            }
            if (now - lastRssiPoll > 2000) {
                lastRssiPoll = now;
                int r = client->getRssi();
                std::lock_guard<std::mutex> lk(mtx);
                st.rssi = r;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
