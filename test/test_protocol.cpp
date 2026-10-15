// Хост-тести парсера протоколу JK (без Arduino, без заліза).
// Запуск:  ./test/run.sh
// Очікування взяті з коментарів до кадрів у fixtures.h (дані з реальних BMS).

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "../src/jk_protocol.h"
#include "fixtures.h"

using namespace fixtures;

static int g_fail = 0, g_pass = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (cond) g_pass++;                                                      \
        else { g_fail++; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
    } while (0)

#define NEAR(a, b, eps) CHECK(std::fabs(double(a) - double(b)) <= (eps))

// Подає кадр шматками по 20 байт, як приходять BLE-нотифікації з MTU 23.
static bool feed(jk::FrameAssembler &fa, const std::vector<uint8_t> &v, size_t chunk = 20) {
    fa.reset();
    bool done = false;
    for (size_t i = 0; i < v.size() && !done; i += chunk) {
        size_t n = std::min(chunk, v.size() - i);
        done = fa.push(v.data() + i, n);
    }
    return done;
}

static jk::CellData decode(const std::vector<uint8_t> &frame, jk::Proto p) {
    jk::FrameAssembler fa;
    CHECK(feed(fa, frame));
    jk::CellData c;
    CHECK(jk::decodeCellInfo(fa.frame(), fa.size(), p, c));
    return c;
}

static void test_command() {
    std::puts("command frame");
    uint8_t b[20];
    jk::buildCommand(jk::CMD_CELL_INFO, b);
    const uint8_t expect[20] = {0xAA, 0x55, 0x90, 0xEB, 0x96, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x10};
    bool same = true;
    for (int i = 0; i < 20; i++) same &= b[i] == expect[i];
    CHECK(same);
}

static void test_device_info() {
    std::puts("device info + protocol detection");
    struct Case { const std::vector<uint8_t> *f; const char *model, *hw, *sw; jk::Proto p; } cases[] = {
        {&DEVICE_INFO_JK02_24S_V10, "JK-B2A24S20P", "10.XG", "10.07", jk::Proto::JK02_24S},
        {&DEVICE_INFO_JK02_32S_V11, "JK_PB2A16S15P", "14.XA", "14.20", jk::Proto::JK02_32S},
        {&DEVICE_INFO_JK02_32S_V19, "JK-PB2A16S20P", "19A", "19.27", jk::Proto::JK02_32S},
    };
    for (auto &c : cases) {
        jk::FrameAssembler fa;
        CHECK(feed(fa, *c.f));
        jk::DeviceInfo d;
        CHECK(jk::decodeDeviceInfo(fa.frame(), fa.size(), d));
        CHECK(d.model == c.model);
        CHECK(d.hwVersion == c.hw);
        CHECK(d.swVersion == c.sw);
        CHECK(jk::protoFromDeviceInfo(d) == c.p);
    }
}

static void test_guess() {
    std::puts("protocol heuristic");
    CHECK(jk::guessProto(CELL_INFO_JK02_24S_V10.data(), CELL_INFO_JK02_24S_V10.size()) == jk::Proto::JK02_24S);
    CHECK(jk::guessProto(CELL_INFO_JK02_32S_V11.data(), CELL_INFO_JK02_32S_V11.size()) == jk::Proto::JK02_32S);
    CHECK(jk::guessProto(CELL_INFO_JK02_32S_V19.data(), CELL_INFO_JK02_32S_V19.size()) == jk::Proto::JK02_32S);
}

static void test_24s_v10() {
    std::puts("JK02_24S v10");
    auto c = decode(CELL_INFO_JK02_24S_V10, jk::Proto::JK02_24S);
    CHECK(c.cellCount == 24);
    NEAR(c.cellV[0], 3.284, 0.0005);
    NEAR(c.cellMin, 3.279, 0.0005); CHECK(c.cellMinIdx == 3);
    NEAR(c.cellMax, 3.285, 0.0005); CHECK(c.cellMaxIdx == 23);
    NEAR(c.cellDelta, 0.006, 0.0005);
    NEAR(c.voltage, 78.735, 0.001);
    NEAR(c.current, 0.0, 0.001);
    NEAR(c.t1, 20.7, 0.05); NEAR(c.t2, 20.7, 0.05); NEAR(c.tMos, 23.4, 0.05);
    CHECK(c.soc == 85);
    NEAR(c.capRemain, 42.804, 0.001); NEAR(c.capNominal, 50.0, 0.001);
    CHECK(c.cycles == 0);
    CHECK(c.runtimeS == 269340);
    CHECK(c.chargeOn); CHECK(!c.dischargeOn);
    CHECK(c.errors == 0);
}

static void test_24s_negative_temps() {
    std::puts("JK02_24S v10, negative temperatures");
    auto c = decode(CELL_INFO_JK02_24S_V10_NEG_TEMPS, jk::Proto::JK02_24S);
    CHECK(c.cellCount == 13);
    NEAR(c.voltage, 42.786, 0.001);
    NEAR(c.t1, -3.1, 0.05); NEAR(c.t2, -2.9, 0.05); NEAR(c.tMos, 0.5, 0.05);
    CHECK(c.soc == 47);
    CHECK(c.cycles == 56);
    CHECK(!c.chargeOn); CHECK(!c.dischargeOn);
}

static void test_32s_v11() {
    std::puts("JK02_32S v11");
    auto c = decode(CELL_INFO_JK02_32S_V11, jk::Proto::JK02_32S);
    CHECK(c.cellCount == 16);
    NEAR(c.cellMin, 3.216, 0.0005); CHECK(c.cellMinIdx == 11);
    NEAR(c.cellMax, 3.246, 0.0005); CHECK(c.cellMaxIdx == 1);
    NEAR(c.voltage, 51.689, 0.001);
    NEAR(c.t1, 17.7, 0.05); NEAR(c.t2, 17.7, 0.05); NEAR(c.tMos, 17.3, 0.05);
    CHECK(c.soc == 52);
    NEAR(c.capRemain, 1.043, 0.001); NEAR(c.capNominal, 2.0, 0.001);
    CHECK(c.runtimeS == 107416);
    CHECK(c.chargeOn); CHECK(c.dischargeOn);
}

static void test_32s_v19() {
    std::puts("JK02_32S v19 (negative current)");
    auto c = decode(CELL_INFO_JK02_32S_V19, jk::Proto::JK02_32S);
    CHECK(c.cellCount == 16);
    NEAR(c.cellMin, 3.374, 0.0005); CHECK(c.cellMinIdx == 10);
    NEAR(c.cellMax, 3.381, 0.0005); CHECK(c.cellMaxIdx == 1);
    NEAR(c.voltage, 54.028, 0.001);
    NEAR(c.current, -0.727, 0.001);
    CHECK(c.power < 0);
    NEAR(c.t1, 11.9, 0.05); NEAR(c.t2, 12.7, 0.05); NEAR(c.tMos, 12.8, 0.05);
    CHECK(c.soc == 100);
    NEAR(c.capRemain, 312.898, 0.001); NEAR(c.capNominal, 314.0, 0.001);
    CHECK(c.cycles == 5);
    CHECK(c.runtimeS == 3609276);
}

static void test_errors() {
    std::puts("error bitmask");
    // 32S: 32-бітна LE маска з offset 134+32 = 166
    auto f = CELL_INFO_JK02_32S_V19;
    uint32_t m = (1u << 1) | (1u << 13) | (1u << 20);
    f[166] = m; f[167] = m >> 8; f[168] = m >> 16; f[169] = m >> 24;
    jk::CellData c;
    CHECK(jk::decodeCellInfo(f.data(), f.size(), jk::Proto::JK02_32S, c));   // CRC не перевіряється в decode
    CHECK(c.errors == m);
    CHECK(jk::errorsToString(c.errors) == "Перегрів MOSFET; Перевищення струму розряду; Не вдалося увімкнути розряд");

    // 24S: 16-бітна LE маска з offset 136
    auto g = CELL_INFO_JK02_24S_V10;
    g[136] = 0x00; g[137] = 0x08;   // біт 11
    CHECK(jk::decodeCellInfo(g.data(), g.size(), jk::Proto::JK02_24S, c));
    CHECK(c.errors == (1u << 11));
    CHECK(jk::errorsToString(c.errors) == "Недонапруга комірки");

    CHECK(jk::errorsToString(0).empty());
    CHECK(jk::errorsToString(1u << 30) == "Біт 30");
}

static void test_assembler() {
    std::puts("frame assembler");
    jk::FrameAssembler fa;

    // Битий CRC відкидається і рахується
    auto bad = CELL_INFO_JK02_24S_V10;
    bad[299] ^= 0x01;
    CHECK(!feed(fa, bad));
    CHECK(fa.crcErrors() == 1);

    // Різні розміри шматків (MTU 23 / 185 / цілий кадр)
    for (size_t chunk : {20u, 182u, 400u}) CHECK(feed(fa, CELL_INFO_JK02_24S_V10, chunk));

    // Сміття перед заголовком: новий заголовок скидає буфер
    fa.reset();
    const uint8_t junk[7] = {1, 2, 3, 4, 5, 6, 7};
    fa.push(junk, sizeof(junk));
    bool done = false;
    for (size_t i = 0; i < CELL_INFO_JK02_24S_V10.size() && !done; i += 20)
        done = fa.push(CELL_INFO_JK02_24S_V10.data() + i, std::min<size_t>(20, CELL_INFO_JK02_24S_V10.size() - i));
    CHECK(done);

    // Обрізаний кадр не декодується
    jk::CellData c;
    CHECK(!jk::decodeCellInfo(CELL_INFO_JK02_24S_V10.data(), 100, jk::Proto::JK02_24S, c));
    // Не той тип кадру
    CHECK(!jk::decodeCellInfo(DEVICE_INFO_JK02_24S_V10.data(), DEVICE_INFO_JK02_24S_V10.size(), jk::Proto::JK02_24S, c));
    // Невідомий протокол
    CHECK(!jk::decodeCellInfo(CELL_INFO_JK02_24S_V10.data(), CELL_INFO_JK02_24S_V10.size(), jk::Proto::Unknown, c));
}

int main() {
    test_command();
    test_device_info();
    test_guess();
    test_24s_v10();
    test_24s_negative_temps();
    test_32s_v11();
    test_32s_v19();
    test_errors();
    test_assembler();
    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
