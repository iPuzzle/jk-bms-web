#include "jk_protocol.h"
#include <cstring>
#include <cmath>

namespace jk {

const char *protoName(Proto p) {
    switch (p) {
        case Proto::JK02_24S: return "JK02_24S";
        case Proto::JK02_32S: return "JK02_32S";
        default: return "?";
    }
}

static uint8_t sum8(const uint8_t *d, size_t n) {
    uint8_t s = 0;
    for (size_t i = 0; i < n; i++) s += d[i];
    return s;
}

static inline uint16_t u16(const uint8_t *f, size_t i) { return uint16_t(f[i]) | (uint16_t(f[i + 1]) << 8); }
static inline int16_t  s16(const uint8_t *f, size_t i) { return int16_t(u16(f, i)); }
static inline uint32_t u32(const uint8_t *f, size_t i) {
    return uint32_t(f[i]) | (uint32_t(f[i + 1]) << 8) | (uint32_t(f[i + 2]) << 16) | (uint32_t(f[i + 3]) << 24);
}
static inline int32_t s32(const uint8_t *f, size_t i) { return int32_t(u32(f, i)); }

void buildCommand(uint8_t address, uint8_t *out) {
    memset(out, 0, 20);
    out[0] = 0xAA; out[1] = 0x55; out[2] = 0x90; out[3] = 0xEB;
    out[4] = address;
    out[5] = 0x00;          // довжина значення
    out[19] = sum8(out, 19);
}

std::string errorsToString(uint32_t mask) {
    // Таблиця бітів за syssi/esphome-jk-bms (DEFAULT_ERRORS_JK02)
    static const char *const ERR[32] = {
        "Опір дротів балансування",          // 0
        "Перегрів MOSFET",                   // 1
        "Кількість комірок ≠ налаштуванням", // 2
        nullptr,                             // 3
        "Батарея повністю заряджена",        // 4
        "Перенапруга пакета",                // 5
        "Перевищення струму заряду",         // 6
        "КЗ при заряді",                     // 7
        "Перегрів при заряді",               // 8
        "Низька температура при заряді",     // 9
        "Помилка зв'язку з копроцесором",    // 10
        "Недонапруга комірки",               // 11
        "Недонапруга пакета",                // 12
        "Перевищення струму розряду",        // 13
        "КЗ при розряді",                    // 14
        "Перегрів при розряді",              // 15
        "Несправність MOSFET заряду",        // 16
        "Несправність MOSFET розряду",       // 17
        "GPS від'єднано",                    // 18
        "Час змінити пароль",                // 19
        "Не вдалося увімкнути розряд",       // 20
        "Перегрів батареї",                  // 21
        "Несправність датчика температури",  // 22
        "Несправність модуля PL",            // 23
        "Не вдалося зняти захист КЗ",        // 24
        "Струм розряду OCP II",              // 25
        "Струм розряду OCP III",             // 26
        "Низька температура при розряді",    // 27
        "GPS: віддалене блокування",         // 28
        nullptr, nullptr, nullptr,
    };
    std::string s;
    for (unsigned i = 0; i < 32; i++) {
        if (!(mask & (1u << i))) continue;
        if (!s.empty()) s += "; ";
        if (ERR[i]) s += ERR[i];
        else { s += "Біт "; s += std::to_string(i); }
    }
    return s;
}

// ---------------- FrameAssembler ----------------

bool FrameAssembler::push(const uint8_t *data, size_t len) {
    // Початок нового кадру: 55 AA EB 90
    if (len >= 4 && data[0] == 0x55 && data[1] == 0xAA && data[2] == 0xEB && data[3] == 0x90) {
        len_ = 0;
    }
    if (len_ + len > sizeof(buf_)) {   // переповнення -> сміття, починаємо з нуля
        len_ = 0;
        return false;
    }
    memcpy(buf_ + len_, data, len);
    len_ += len;

    if (len_ < FRAME_MIN) return false;

    bool ok = buf_[0] == 0x55 && buf_[1] == 0xAA && buf_[2] == 0xEB && buf_[3] == 0x90 &&
              sum8(buf_, FRAME_MIN - 1) == buf_[FRAME_MIN - 1];
    if (!ok) {
        crcErrors_++;
        len_ = 0;
        return false;
    }
    return true;   // наступна нотифікація з заголовком скине буфер
}

// ---------------- Device info ----------------

static std::string str(const uint8_t *f, size_t off, size_t n) {
    std::string s;
    for (size_t i = 0; i < n; i++) {
        char c = char(f[off + i]);
        if (c == 0) break;
        if (c >= 0x20 && c < 0x7F) s += c;
    }
    return s;
}

bool decodeDeviceInfo(const uint8_t *f, size_t len, DeviceInfo &o) {
    if (len < FRAME_MIN || frameType(f) != FRAME_DEV_INFO) return false;
    o.model        = str(f, 6, 16);
    o.hwVersion    = str(f, 22, 8);
    o.swVersion    = str(f, 30, 8);
    o.uptimeS      = u32(f, 38);
    o.powerOnCount = u32(f, 42);
    o.name         = str(f, 46, 16);
    o.mfgDate      = str(f, 78, 8);
    o.serial       = str(f, 86, 11);
    o.valid = true;
    return true;
}

static int leadingInt(const std::string &s) {
    int v = 0; bool any = false;
    for (char c : s) {
        if (c >= '0' && c <= '9') { v = v * 10 + (c - '0'); any = true; }
        else break;
    }
    return any ? v : -1;
}

Proto protoFromDeviceInfo(const DeviceInfo &di) {
    if (!di.valid) return Proto::Unknown;
    int hw = leadingInt(di.hwVersion);
    if (hw < 0) hw = leadingInt(di.swVersion);
    if (hw < 0) return Proto::Unknown;
    return hw >= 11 ? Proto::JK02_32S : Proto::JK02_24S;
}

// ---------------- Cell info ----------------

bool decodeCellInfo(const uint8_t *f, size_t len, Proto proto, CellData &o) {
    if (len < FRAME_MIN || frameType(f) != FRAME_CELL_INFO) return false;
    if (proto != Proto::JK02_24S && proto != Proto::JK02_32S) return false;

    const bool is32 = proto == Proto::JK02_32S;
    size_t off = is32 ? 16 : 0;
    const uint8_t cells = is32 ? 32 : 24;

    o = CellData{};
    o.proto = proto;

    // Напруги комірок: 6 + 2*i
    for (uint8_t i = 0; i < cells; i++) {
        float v = u16(f, 6 + i * 2) * 0.001f;
        o.cellV[i] = v;
        if (v > 0.1f) o.cellCount = i + 1;
    }
    // Min/max/дельту/середнє рахуємо самі: поля BMS (58..63 + off) на частині прошивок
    // оновлюються із запізненням і не збігаються з фактичними напругами.
    float sum = 0;
    uint8_t active = 0;
    o.cellMin = 100; o.cellMax = 0;
    for (uint8_t i = 0; i < o.cellCount; i++) {
        float v = o.cellV[i];
        if (v <= 0.1f) continue;
        sum += v;
        active++;
        if (v < o.cellMin) { o.cellMin = v; o.cellMinIdx = i + 1; }
        if (v > o.cellMax) { o.cellMax = v; o.cellMaxIdx = i + 1; }
    }
    if (active == 0) o.cellMin = 0;
    o.cellAvg   = active ? sum / active : 0;
    o.cellDelta = active ? o.cellMax - o.cellMin : 0;

    off *= 2;   // далі зсув подвоюється: 0 або 32

    o.voltage = u32(f, 118 + off) * 0.001f;
    o.current = s32(f, 126 + off) * 0.001f;
    o.power   = o.voltage * o.current;
    o.t1      = s16(f, 130 + off) * 0.1f;
    o.t2      = s16(f, 132 + off) * 0.1f;

    if (is32) {
        o.tMos   = s16(f, 112 + off) * 0.1f;
        o.errors = u32(f, 134 + off);
    } else {
        o.tMos   = s16(f, 134 + off) * 0.1f;
        o.errors = u16(f, 136 + off);
    }

    o.balanceCurrent = s16(f, 138 + off) * 0.001f;
    o.balancing      = f[140 + off];
    o.balancerOn     = o.balancing != 0;
    o.soc            = f[141 + off];
    o.capRemain      = u32(f, 142 + off) * 0.001f;
    o.capNominal     = u32(f, 146 + off) * 0.001f;
    o.cycles         = u32(f, 150 + off);
    o.cycleCapacity  = u32(f, 154 + off) * 0.001f;
    o.runtimeS       = u32(f, 162 + off);
    o.chargeOn       = f[166 + off] != 0;
    o.dischargeOn    = f[167 + off] != 0;

    o.valid = true;
    return true;
}

static bool plausible(const CellData &d) {
    if (!d.valid || d.cellCount < 2 || d.soc > 100) return false;
    float sum = 0;
    for (uint8_t i = 0; i < d.cellCount; i++) sum += d.cellV[i];
    if (sum < 2.0f) return false;
    float diff = std::fabs(sum - d.voltage);
    return diff < 0.5f + sum * 0.02f;   // 2 % + 0.5 В
}

Proto guessProto(const uint8_t *f, size_t len) {
    CellData a, b;
    bool pa = decodeCellInfo(f, len, Proto::JK02_24S, a) && plausible(a);
    bool pb = decodeCellInfo(f, len, Proto::JK02_32S, b) && plausible(b);
    if (pa && !pb) return Proto::JK02_24S;
    if (pb && !pa) return Proto::JK02_32S;
    return Proto::Unknown;
}

} // namespace jk
