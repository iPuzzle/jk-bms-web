#pragma once
// Кільцевий лог подій для вебморди (потокобезпечний).

#include <Arduino.h>
#include <ArduinoJson.h>

namespace evlog {

enum Level : uint8_t { INFO = 0, WARN = 1, ERR = 2 };

void add(Level lvl, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

// Записує записи з id > afterId у масив. Повертає останній id.
uint32_t dump(JsonArray arr, uint32_t afterId);

}  // namespace evlog

#define LOGI(...) evlog::add(evlog::INFO, __VA_ARGS__)
#define LOGW(...) evlog::add(evlog::WARN, __VA_ARGS__)
#define LOGE(...) evlog::add(evlog::ERR, __VA_ARGS__)
