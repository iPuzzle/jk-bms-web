#pragma once
// Протокол JK-BMS по BLE (сервіс FFE0, характеристика FFE1).
// Чистий C++ без Arduino-залежностей, щоб можна було тестувати на ПК.
//
// Посилання: github.com/syssi/esphome-jk-bms (jk_bms_ble.cpp)

#include <cstdint>
#include <cstddef>
#include <string>

namespace jk {

enum class Proto : uint8_t { Unknown = 0, JK02_24S = 24, JK02_32S = 32 };

const char *protoName(Proto p);

constexpr uint8_t CMD_CELL_INFO   = 0x96;
constexpr uint8_t CMD_DEVICE_INFO = 0x97;

constexpr size_t FRAME_MIN = 300;   // розмір кадру відповіді
constexpr size_t FRAME_MAX = 320;

constexpr uint8_t FRAME_SETTINGS  = 0x01;
constexpr uint8_t FRAME_CELL_INFO = 0x02;
constexpr uint8_t FRAME_DEV_INFO  = 0x03;

constexpr uint8_t MAX_CELLS = 32;

// Формує 20-байтну команду запиту. out має бути щонайменше 20 байт.
void buildCommand(uint8_t address, uint8_t *out);

struct DeviceInfo {
    bool valid = false;
    std::string model;      // vendor id
    std::string hwVersion;
    std::string swVersion;
    std::string name;
    std::string mfgDate;
    std::string serial;
    uint32_t uptimeS = 0;
    uint32_t powerOnCount = 0;
};

struct CellData {
    bool valid = false;
    Proto proto = Proto::Unknown;
    uint8_t cellCount = 0;
    float cellV[MAX_CELLS] = {};
    float cellAvg = 0, cellDelta = 0;
    uint8_t cellMaxIdx = 0, cellMinIdx = 0;     // 1-based
    float cellMin = 0, cellMax = 0;
    float voltage = 0;       // В
    float current = 0;       // А, + заряд / - розряд
    float power = 0;         // Вт
    float tMos = 0, t1 = 0, t2 = 0;   // °C
    float balanceCurrent = 0;
    uint8_t balancing = 0;   // 0 - ні, 1 - заряд, 2 - розряд
    uint8_t soc = 0;
    float capRemain = 0;     // Аг
    float capNominal = 0;    // Аг
    uint32_t cycles = 0;
    float cycleCapacity = 0; // Аг
    uint32_t runtimeS = 0;
    bool chargeOn = false, dischargeOn = false, balancerOn = false;
    uint32_t errors = 0;     // бітова маска аварій (24S — 16 біт, 32S — 32 біти)
};

// Перетворює біти помилок на текст через "; ". Порожній рядок, якщо помилок немає.
std::string errorsToString(uint32_t mask);

// Збирає кадри з BLE-нотифікацій.
class FrameAssembler {
public:
    // Повертає true, коли зібрано повний кадр із правильною CRC (доступний через frame()).
    bool push(const uint8_t *data, size_t len);
    const uint8_t *frame() const { return buf_; }
    size_t size() const { return len_; }
    uint32_t crcErrors() const { return crcErrors_; }
    void reset() { len_ = 0; }
private:
    uint8_t buf_[FRAME_MAX + 32] = {};
    size_t len_ = 0;
    uint32_t crcErrors_ = 0;
};

inline uint8_t frameType(const uint8_t *f) { return f[4]; }

bool decodeDeviceInfo(const uint8_t *f, size_t len, DeviceInfo &out);

// Визначає протокол за версією HW/SW (>= 11 -> 32S). Unknown, якщо не вдалося.
Proto protoFromDeviceInfo(const DeviceInfo &di);

// Декодує кадр cell info за заданим протоколом.
bool decodeCellInfo(const uint8_t *f, size_t len, Proto proto, CellData &out);

// Евристика: пробує обидва протоколи й обирає той, де напруга пакета
// узгоджується з сумою напруг комірок. Unknown, якщо жоден не підходить.
Proto guessProto(const uint8_t *f, size_t len);

} // namespace jk
