#pragma once
// Керування BLE: сканування, підключення до JK-BMS, прийом кадрів.
// Уся робота з NimBLE виконується в окремій FreeRTOS-задачі;
// вебсервер лише ставить запити і читає знімок стану.

#include <Arduino.h>
#include <string>
#include <vector>
#include "jk_protocol.h"

enum class BleState : uint8_t {
    Idle,          // нічого не робимо
    Scanning,      // йде сканування
    Connecting,    // встановлення BLE-з'єднання
    Discovering,   // пошук сервісу FFE0 / FFE1, підписка
    WaitInfo,      // запит device info
    Streaming,     // отримуємо cell info
    Error,         // остання спроба завершилась помилкою (далі як Idle)
};

const char *bleStateKey(BleState s);

struct ScanEntry {
    std::string mac;
    std::string name;
    int rssi = 0;
    uint8_t addrType = 0;
    uint32_t lastSeenMs = 0;
    bool jk = false;              // у рекламі є сервіс FFE0 (ознака JK-BMS, навіть без імені)
};

struct BleSnapshot {
    BleState state = BleState::Idle;
    std::string message;          // поточний крок / причина
    std::string mac, name;        // цільовий пристрій
    uint8_t addrType = 0;
    jk::Proto proto = jk::Proto::Unknown;
    bool protoForced = false;
    uint32_t connectedAtMs = 0;
    uint32_t lastFrameMs = 0;
    int rssi = 0;
    uint32_t frames = 0;
    uint32_t crcErrors = 0;
    jk::DeviceInfo info;
    jk::CellData data;
    std::vector<ScanEntry> devices;
    uint32_t scanStartedMs = 0;
    uint32_t lastScanMs = 0;
};

class BleManager {
public:
    void begin();

    void requestScan();
    void requestConnect(const std::string &mac, uint8_t addrType, const std::string &name, jk::Proto forced);
    // error=true — показати як помилку (червоним), а не як звичайне відключення.
    // Якщо BLE-задача зараз заблокована в Discovering, розриває лінк напряму (див. cpp).
    void requestDisconnect(const char *reason, bool error = false);
    // Мітка початку поточного етапу Discovering (0, якщо стан інший) — для таймауту в loop().
    uint32_t discoverStartMs() const;

    bool isConnected() const;
    BleState state() const;   // дешево, без копіювання знімка
    void snapshot(BleSnapshot &out);
    // Тип адреси та ім'я з результатів останнього сканування. false, якщо MAC не бачили.
    bool lookupScan(const std::string &mac, uint8_t &addrType, std::string &name);
    uint32_t heartbeatMs() const { return beat_; }

    // Внутрішнє (викликається з колбеків NimBLE)
    void onNotify(const uint8_t *data, size_t len);
    void onPeerDisconnected(int reason);
    void onScanResult(const std::string &mac, uint8_t type, const std::string &name, int rssi, bool hasFfe0);
    void onScanEnd();

private:
    static void taskEntry(void *arg);
    void taskLoop();
    void doConnect();
    void doDisconnect(const std::string &reason, bool error);
    bool sendCmd(uint8_t cmd);
    void setState(BleState s, const std::string &msg);

    volatile uint32_t beat_ = 0;
};

extern BleManager Ble;
