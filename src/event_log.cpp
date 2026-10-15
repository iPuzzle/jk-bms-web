#include "event_log.h"
#include <mutex>
#include <time.h>

namespace evlog {

namespace {
constexpr size_t N = 60;
struct Entry {
    uint32_t id;
    uint32_t ms;
    time_t ts;
    Level lvl;
    char msg[112];
};
Entry ring[N];
uint32_t nextId = 1;
std::mutex mtx;
}  // namespace

void add(Level lvl, const char *fmt, ...) {
    char buf[112];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    static const char L[] = {'I', 'W', 'E'};
    Serial.printf("[%c] %s\n", L[lvl], buf);

    std::lock_guard<std::mutex> lk(mtx);
    Entry &e = ring[nextId % N];
    e.id = nextId++;
    e.ms = millis();
    time_t now = time(nullptr);
    e.ts = now > 1700000000 ? now : 0;   // 0 = час ще не синхронізовано
    e.lvl = lvl;
    strlcpy(e.msg, buf, sizeof(e.msg));
}

uint32_t dump(JsonArray arr, uint32_t afterId) {
    std::lock_guard<std::mutex> lk(mtx);
    uint32_t last = nextId - 1;
    uint32_t from = afterId + 1;
    if (last >= N && from < last - N + 1) from = last - N + 1;
    for (uint32_t id = from; id <= last; id++) {
        const Entry &e = ring[id % N];
        JsonObject o = arr.add<JsonObject>();
        o["id"] = e.id;
        o["ms"] = e.ms;
        o["ts"] = (uint32_t)e.ts;
        o["l"] = (uint8_t)e.lvl;
        o["m"] = e.msg;
    }
    return last;
}

}  // namespace evlog
