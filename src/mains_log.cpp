#include "mains_log.h"
#include "config.h"
#include "time_sync.h"

namespace mainslog {

namespace {

struct Entry {
    uint32_t ts;     // UTC-epoch, 0 — час невідомий
    uint32_t ms;     // millis() у момент ПОДІЇ
    bool on;
    bool pending;    // ts ще не уточнено синхронізацією з сервісу часу (тимчасово — за годинником ESP або 0)
};

Entry buf[MAINS_LOG_SIZE];   // найстаріші першими; лише RAM — після перезапуску журнал починається заново
size_t count = 0;
uint32_t ver = 0;
bool anyPending = false;     // є записи з pending
uint32_t seenGen = 0;        // остання врахована синхронізація годинника (timesync::syncGen())

}  // namespace

void begin(bool currentOn) {
    count = 0;
    seenGen = timesync::syncGen();
    record(currentOn, millis());
}

void record(bool on, uint32_t eventMs) {
    if (count == MAINS_LOG_SIZE) {
        memmove(buf, buf + 1, (MAINS_LOG_SIZE - 1) * sizeof(Entry));
        count--;
    }
    Entry &e = buf[count++];
    e.on = on;
    e.ms = eventMs | 1;
    e.pending = true;
    // Тимчасова мітка — за годинником ESP (він міг відстати від сервісу з моменту останньої синхронізації)
    // або 0, якщо його ще не виставлено. Точну мітку tick() порахує після свіжої синхронізації.
    e.ts = timeValid() ? (uint32_t)time(nullptr) - (millis() - eventMs) / 1000 : 0;
    anyPending = true;
    ver++;
    timesync::syncNow();   // не чекаємо наступної планової синхронізації
}

void tick() {
    // Годинник щойно виставлено з сервісу (time.now / NTP): перераховуємо мітки очікуючих записів від нього
    // за їхнім millis(). Записи, створені ПІСЛЯ цієї синхронізації, лишаються pending до наступної.
    const uint32_t gen = timesync::syncGen();
    if (gen == seenGen) return;
    seenGen = gen;
    if (!anyPending || !timeValid()) return;

    const uint32_t now = (uint32_t)time(nullptr), nowMs = millis();
    for (size_t i = 0; i < count; i++) {
        if (!buf[i].pending) continue;
        buf[i].ts = now - (nowMs - buf[i].ms) / 1000;
        buf[i].pending = false;
    }
    anyPending = false;
    ver++;
}

uint32_t version() { return ver; }

void toJson(JsonArray out) {
    for (size_t i = count; i-- > 0;) {
        JsonArray a = out.add<JsonArray>();
        a.add(buf[i].ts);
        a.add(buf[i].on ? 1 : 0);
    }
}

}  // namespace mainslog
