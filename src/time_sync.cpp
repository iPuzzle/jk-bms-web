#include "time_sync.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include <atomic>
#include "config.h"
#include "event_log.h"

namespace timesync {

namespace {

enum Src : uint8_t { None = 0, Ntp = 1, TimeNow = 2 };

std::atomic<uint8_t> src{None};
std::atomic<uint32_t> lastSyncMs{0};   // 0 — ще не було (тому завжди | 1)
std::atomic<uint32_t> gen{0};          // +1 після кожної успішної синхронізації (див. syncGen())
std::atomic<bool> syncReq{false};      // запит позачергової синхронізації (syncNow())
std::atomic<bool> ntpOn{false};

void onNtpSync(struct timeval *) {
    src = Ntp;
    lastSyncMs = millis() | 1;
    gen++;
}

void startNtp() {
    if (ntpOn.exchange(true)) return;
    sntp_set_time_sync_notification_cb(onNtpSync);
    configTzTime(TZ_INFO, NTP_SERVER);
    LOGI("NTP запущено (%s)", NTP_SERVER);
}

void stopNtp() {
    if (!ntpOn.exchange(false)) return;
    esp_sntp_stop();   // time.now знову основне джерело, SNTP не має його перебивати
}

#if TIMENOW_ENABLED
// Повертає UTC-epoch або 0 (тоді err — коротка причина).
uint32_t fetchTimeNow(String &err) {
    WiFiClientSecure client;
    // Перевірку сертифіката вимкнено свідомо: вона потребує правильного часу, а ми його саме й запитуємо.
    client.setInsecure();
    HTTPClient http;
    http.setConnectTimeout(TIMENOW_TIMEOUT_MS);
    http.setTimeout(TIMENOW_TIMEOUT_MS);
    if (!http.begin(client, "https://" TIMENOW_HOST TIMENOW_PATH)) {   // звичайний HTTP сервіс редіректить, тож лише HTTPS
        err = "не вдалося почати запит";
        return 0;
    }
    http.addHeader("Accept", "application/json");

    uint32_t t = 0;
    const int code = http.GET();
    if (code == HTTP_CODE_OK) {
        // getString() коректно збирає chunked-відповідь (getStream() віддав би сирі чанки)
        String body = http.getString();
        JsonDocument filter;
        filter["unixtime"] = true;
        JsonDocument doc;
        if (body.length() > 2048 || deserializeJson(doc, body, DeserializationOption::Filter(filter))) {
            err = "некоректна відповідь";
        } else {
            t = doc["unixtime"] | 0u;
            if (t < 1700000000) {
                err = "немає поля unixtime";
                t = 0;
            }
        }
    } else if (code > 0) {
        err = "HTTP " + String(code);
    } else {
        err = String("з'єднання: ") + HTTPClient::errorToString(code);
    }
    http.end();
    return t;
}

void task(void *) {
    uint32_t nextTry = 0, lastAttempt = 0;
    bool attempted = false;
    uint8_t fails = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (!WiFi.isConnected()) continue;   // запит syncReq при цьому не губиться — виконається, щойно з'явиться Wi-Fi

        const uint32_t now = millis();
        const bool due = (int32_t)(now - nextTry) >= 0;
        // Позачергова синхронізація (подія мережі) — не частіше за TIMENOW_ONDEMAND_MIN_MS
        const bool wanted = syncReq.load() && (!attempted || now - lastAttempt >= TIMENOW_ONDEMAND_MIN_MS);
        if (!due && !wanted) continue;
        syncReq = false;
        attempted = true;
        lastAttempt = now;

        String err;
        const uint32_t t = fetchTimeNow(err);
        if (t) {
            const bool wasValid = timeValid();
            const long delta = (long)t - (long)time(nullptr);
            struct timeval tv = {(time_t)t, 0};
            settimeofday(&tv, nullptr);
            src = TimeNow;
            lastSyncMs = millis() | 1;
            gen++;
            fails = 0;
            nextTry = millis() + TIMENOW_RESYNC_MS;
            stopNtp();
            if (wasValid) LOGI("Час синхронізовано з time.now (корекція %+ld с)", delta);
            else LOGI("Час синхронізовано з time.now");
        } else {
            fails++;
            nextTry = millis() + (fails < TIMENOW_FAILS_TO_NTP ? TIMENOW_RETRY_MS : TIMENOW_RETRY_SLOW_MS);
            if (fails <= TIMENOW_FAILS_TO_NTP) LOGW("time.now: %s (спроба %u)", err.c_str(), fails);
            if (fails >= TIMENOW_FAILS_TO_NTP) startNtp();
        }
    }
}
#endif

}  // namespace

void begin() {
    static bool started = false;
    if (started) return;
    started = true;
    setenv("TZ", TZ_INFO, 1);
    tzset();
#if TIMENOW_ENABLED
    xTaskCreatePinnedToCore(task, "timesync", 12288, nullptr, 1, nullptr, 1);   // TLS-рукостискання потребує стеку
#else
    startNtp();
#endif
}

const char *source() {
    switch (src.load()) {
        case TimeNow: return "timenow";
        case Ntp:     return "ntp";
        default:      return "none";
    }
}

int32_t lastSyncAgeS() {
    const uint32_t ms = lastSyncMs.load();
    return ms ? (int32_t)((millis() - ms) / 1000) : -1;
}

uint32_t syncGen() { return gen.load(); }

void syncNow() { syncReq = true; }

}  // namespace timesync
