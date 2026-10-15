#include "settings.h"
#include <Preferences.h>
#include <mutex>
#include <map>
#include "config.h"

namespace settings {

namespace {
Preferences prefs;
std::mutex mtx;
String ssid, pass, wpass;
std::map<String, String> aliases;
std::map<String, uint8_t> protos;

void loadMap(const char *key, JsonDocument &doc) {
    String s = prefs.getString(key, "{}");
    if (deserializeJson(doc, s)) doc.to<JsonObject>();
}

void saveAliases() {
    JsonDocument d;
    for (auto &kv : aliases) d[kv.first] = kv.second;
    String s;
    serializeJson(d, s);
    prefs.putString("aliases", s);
}

void saveProtos() {
    JsonDocument d;
    for (auto &kv : protos) d[kv.first] = kv.second;
    String s;
    serializeJson(d, s);
    prefs.putString("protos", s);
}

String norm(const String &mac) {
    String m = mac;
    m.toUpperCase();
    return m;
}
}  // namespace

void begin() {
    prefs.begin("jkbmsweb", false);
    std::lock_guard<std::mutex> lk(mtx);
    ssid = prefs.getString("ssid", "");
    pass = prefs.getString("pass", "");
    wpass = prefs.getString("wpass", WEB_DEFAULT_PASS);

    JsonDocument a;
    loadMap("aliases", a);
    for (JsonPair kv : a.as<JsonObject>()) aliases[kv.key().c_str()] = kv.value().as<const char *>();

    JsonDocument p;
    loadMap("protos", p);
    for (JsonPair kv : p.as<JsonObject>()) protos[kv.key().c_str()] = kv.value().as<uint8_t>();
}

String wifiSsid() { std::lock_guard<std::mutex> lk(mtx); return ssid; }
String wifiPass() { std::lock_guard<std::mutex> lk(mtx); return pass; }

void setWifi(const String &s, const String &p) {
    std::lock_guard<std::mutex> lk(mtx);
    ssid = s; pass = p;
    prefs.putString("ssid", s);
    prefs.putString("pass", p);
}

String webPass() { std::lock_guard<std::mutex> lk(mtx); return wpass; }
bool isDefaultPass() { std::lock_guard<std::mutex> lk(mtx); return wpass == WEB_DEFAULT_PASS; }

void setWebPass(const String &p) {
    std::lock_guard<std::mutex> lk(mtx);
    wpass = p;
    prefs.putString("wpass", p);
}

String alias(const String &mac) {
    std::lock_guard<std::mutex> lk(mtx);
    auto it = aliases.find(norm(mac));
    return it == aliases.end() ? String() : it->second;
}

void setAlias(const String &mac, const String &name) {
    std::lock_guard<std::mutex> lk(mtx);
    String n = name;
    n.trim();
    if (n.length() > 40) n = n.substring(0, 40);
    if (n.isEmpty()) aliases.erase(norm(mac));
    else aliases[norm(mac)] = n;
    saveAliases();
}

jk::Proto protoOverride(const String &mac) {
    std::lock_guard<std::mutex> lk(mtx);
    auto it = protos.find(norm(mac));
    if (it == protos.end()) return jk::Proto::Unknown;
    return static_cast<jk::Proto>(it->second);
}

void setProtoOverride(const String &mac, jk::Proto p) {
    std::lock_guard<std::mutex> lk(mtx);
    if (p == jk::Proto::Unknown) protos.erase(norm(mac));
    else protos[norm(mac)] = static_cast<uint8_t>(p);
    saveProtos();
}

void aliasesToJson(JsonObject out) {
    std::lock_guard<std::mutex> lk(mtx);
    for (auto &kv : aliases) out[kv.first] = kv.second;
}

void protoToJson(JsonObject out) {
    std::lock_guard<std::mutex> lk(mtx);
    for (auto &kv : protos) out[kv.first] = kv.second;
}

}  // namespace settings
