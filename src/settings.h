#pragma once
// Налаштування в NVS: Wi-Fi, пароль вебморди, аліаси пристроїв, ручний вибір протоколу.

#include <Arduino.h>
#include <ArduinoJson.h>
#include "jk_protocol.h"

namespace settings {

void begin();

String wifiSsid();
String wifiPass();
void setWifi(const String &ssid, const String &pass);

String webPass();
bool isDefaultPass();
void setWebPass(const String &pass);

String alias(const String &mac);
void setAlias(const String &mac, const String &name);   // порожнє ім'я = видалити

jk::Proto protoOverride(const String &mac);
void setProtoOverride(const String &mac, jk::Proto p);  // Unknown = авто

void aliasesToJson(JsonObject out);
void protoToJson(JsonObject out);

}  // namespace settings
