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

// ---------- Індикатор мережі (наявність зовнішніх +5 В) ----------
#define MAINS_SENSE_ENABLED   1         // 1 — читати пін і показувати індикатор у вебморді; 0 — вимкнено повністю
#define MAINS_SENSE_PIN       4         // GPIO4: вільний на N16R8 (не strapping, не flash/PSRAM, не USB, не RGB-LED)
#define MAINS_ACTIVE_LEVEL    LOW       // рівень на піні, коли мережа є: LOW — оптопара PC817, HIGH — резисторний дільник
#define MAINS_DEBOUNCE_MS     2000UL    // новий стан має триматися так довго, щоб його прийняти
#define MAINS_LOG_SIZE        25        // скільки останніх змін стану мережі тримати в RAM і показувати

// ---------- Синхронізація часу ----------
// Основне джерело — time.now (https://time.now/developer, HTTPS, без ключа; потрібне посилання на Time.Now
// у README/вебморді). При збоях час бере запасний NTP. 0 — лише NTP.
#define TIMENOW_ENABLED       1
#define TIMENOW_HOST          "time.now"
#define TIMENOW_PATH          "/developer/api/time?timezone=Europe%2FKyiv"   // відповідь містить unixtime (UTC)
#define TIMENOW_TIMEOUT_MS    8000UL
#define TIMENOW_RESYNC_MS     21600000UL  // раз на 6 год
#define TIMENOW_ONDEMAND_MIN_MS 30000UL   // позачергова синхронізація (подія мережі) — не частіше за цей інтервал
#define TIMENOW_RETRY_MS      60000UL     // повтор після збою
#define TIMENOW_RETRY_SLOW_MS 1800000UL   // після TIMENOW_FAILS_TO_NTP збоїв поспіль — раз на 30 хв
#define TIMENOW_FAILS_TO_NTP  3           // стільки збоїв поспіль -> вмикаємо запасний NTP

// ---------- Сторожовий таймер ----------
#define BLE_TASK_STALL_MS     90000UL   // BLE-задача не відповідає -> перезавантаження
