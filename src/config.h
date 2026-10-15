#pragma once

// ---------- Мережа ----------
#define HOSTNAME              "jkbms"            // http://jkbms.local
#define SETUP_AP_SSID         "JKBMS-Setup"
#define SETUP_AP_PASS         "jkbms1234"        // мін. 8 символів
#define WIFI_CONNECT_TIMEOUT  20000UL            // мс, після чого піднімаємо AP налаштування
#define WIFI_RETRY_AP_MS      300000UL           // в режимі AP раз на 5 хв пробуємо знову STA

// ---------- Вебморда ----------
#define WEB_USER              "admin"
#define WEB_DEFAULT_PASS      "admin"            // змінити в налаштуваннях!
#define TZ_INFO               "EET-2EEST,M3.5.0/3,M10.5.0/4"   // Київ
#define NTP_SERVER            "pool.ntp.org"

// ---------- BLE / сесія ----------
#define SCAN_DURATION_MS      8000UL
#define CONNECT_TIMEOUT_MS    10000UL
// Вікно сканування під час connect() (одиниці 0.625 мс). Стандартні 16/16 = 100% радіочасу,
// що не лишає місця Wi-Fi (спільне радіо) -> вебсокет рветься. 160/30 ≈ 19%.
#define CONNECT_SCAN_ITVL     160
#define CONNECT_SCAN_WINDOW   30
#define DEVINFO_WAIT_MS      3000UL    // скільки чекати device info перед запитом cell info
#define DISCOVER_TIMEOUT_MS   15000UL   // пошук сервісу/характеристики/підписка довше -> розрив (не JK-BMS)
#define STALE_DATA_MS        20000UL   // немає кадрів довше -> розриваємо з'єднання
#define IDLE_TIMEOUT_MS       180000UL  // 3 хв без жодного видимого клієнта -> відключення
#define SESSION_LIMIT_MS      1800000UL // жорсткий ліміт сесії 30 хв (продовжується кнопкою)
#define CLIENT_HB_VALID_MS    30000UL   // клієнт вважається живим, якщо був heartbeat за 30 с

// ---------- Сторожовий таймер ----------
#define BLE_TASK_STALL_MS     90000UL   // BLE-задача не відповідає -> перезавантаження
