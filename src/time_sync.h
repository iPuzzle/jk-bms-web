#pragma once
// Синхронізація системного годинника: основне джерело — time.now, запасне — NTP.
// Результат пишеться в системний час (settimeofday), решта коду просто викликає time().

#include <Arduino.h>
#include <time.h>

// false, поки час не синхронізовано (годинник показує 1970 рік)
inline bool timeValid() { return time(nullptr) > 1700000000; }

namespace timesync {

void begin();              // один раз, коли піднято STA: часовий пояс + задача синхронізації
const char *source();      // "timenow" | "ntp" | "none"
int32_t lastSyncAgeS();    // секунд від останньої синхронізації, -1 — ще не було

// Лічильник успішних синхронізацій (time.now або NTP). Зміна = годинник щойно виставлено з сервісу.
uint32_t syncGen();

// Попросити позачергову синхронізацію (наприклад, щойно змінився стан мережі). Не блокує:
// виконається в задачі `timesync` — одразу, якщо є Wi-Fi, інакше щойно він з'явиться.
void syncNow();

}  // namespace timesync
