# JK-BMS Web

🇺🇦 Українська · [🇬🇧 English](#english)

Віддалений доступ до JK-BMS по Bluetooth через браузер.

ESP32-S3 стоїть поруч із батареями, а з будь-якого пристрою в мережі (або через VPN) можна
відкрити http://jkbms.local, просканувати BMS навколо, обрати потрібну і подивитися її стан.
Коли вкладку закрито — через 3 хвилини ESP сам відключається, і BMS знову доступна для
телефонного застосунку JK.

Пристрій лише **читає** дані: нічого не перемикає і не змінює в налаштуваннях BMS.

## Можливості

**Сканування**
- Активне BLE-сканування 8 с, фільтр: ім'я починається з `JK` або в рекламі є сервіс FFE0 (перемикач «показати всі»).
- Ім'я, MAC, RSSI з індикатором, сортування за сигналом.
- Свої назви пакетів («Пакет 1 · 280 Аг»), збережені за MAC.
- Підключення за MAC вручну, без сканування.

**Підключення**
- Покрокова індикація: з'єднання → сервіс FFE0/FFE1 → device info → дані. Помилка показується на кроці, де сталася.
- Автовизначення протоколу (JK02_24S / JK02_32S) за версією HW, запасна евристика за кадром, ручний вибір для кожного пристрою.

**Дані**
- Напруга, струм (заряд/розряд), потужність, SOC, залишкова/номінальна ємність.
- Напруги всіх комірок з підсвіткою min/max, дельта, середня.
- Температури (T1, T2, MOSFET), стан ключів заряду й розряду, балансування.
- Цикли, модель, версії HW/SW, серійний номер.
- «Вік» даних: видно, якщо кадри перестали приходити.

**Сесія**
- Відключення після **3 хв** без жодної відкритої *видимої* вкладки (прихована вкладка не тримає з'єднання).
- Жорсткий ліміт **30 хв**, за 2 хв до кінця — банер «Продовжити».
- Немає кадрів 20 с → з'єднання вважається мертвим і розривається.
- Кілька вкладок бачать одну й ту саму сесію.

**Сервіс**
- Точка доступу з captive-сторінкою для першого налаштування Wi-Fi.
- mDNS `jkbms.local`, NTP, basic auth.
- Оновлення прошивки з браузера або через `espota`.
- Журнал подій і діагностика: heap/PSRAM, аптайм, причина перезапуску, RSSI Wi-Fi і BMS, помилки CRC.
- Сторожі: watchdog `loop()` і перезапуск, якщо BLE-задача зависла.
- Вебінтерфейс українською та англійською: перемикач UK/EN у шапці, за замовчуванням — за мовою браузера. Журнал подій лишається українською.

## Залізо

- **ESP32-S3 N16R8** (16 МБ flash, 8 МБ PSRAM), наприклад ESP32-S3-DevKitC-1.
- Живлення 5 В через USB або від стабільного джерела.
- Відстань до BMS — до кількох метрів без металевих перешкод; зовнішня антена не потрібна.
- Корпус — пластиковий.

Підтримувані BMS: JK-B (прошивки 10.x — JK02_24S, 11.x+ — JK02_32S), JK-PB (JK02_32S).
Старі JK-BD з протоколом JK04 не підтримуються.

### Індикатор мережі (необов'язково)

У шапці вебморди — товстий напис: зелений **«МЕРЕЖА Є»** або червоний **«МЕРЕЖІ НЕМАЄ»**
(сірий, якщо зв'язок із ESP втрачено і стан невідомий). Сигнал — зовнішні **+5 В**, які є, поки є мережа
(наприклад, від зарядки, увімкненої в розетку). Зміни стану пишуться в журнал.
Вимкнути: `MAINS_SENSE_ENABLED 0` у `src/config.h` — тоді ні піна, ні напису.

Нижче — випадаючий список **«Статистика мережі»**: останні 25 змін стану (час, «з'явилась» / «зникла», тривалість),
підсумок — кількість відключень, сумарний час без мережі, відсоток наявності. Журнал живе **лише в RAM**:
після перезапуску ESP історія не відновлюється, а перший запис — поточний стан мережі.

**Час запису береться з сервісу часу.** У момент зміни стану запис отримує тимчасову мітку за годинником ESP
(або без мітки, якщо годинник ще не виставлено), і одразу запитується позачергова синхронізація з Time.Now
(не частіше ніж раз на 30 с). Щойно годинник виставлено, мітка перераховується від свіжого часу з урахуванням того,
скільки секунд минуло від самої події, — тож вона відповідає моменту події, а не моменту відповіді сервісу.
Якщо сервісу немає (типово: зникло світло, а з ним і роутер), запит чекає, і щойно з'явиться Wi-Fi, записи без
точного часу уточнюються автоматично.

Пін **GPIO4** (вільний на N16R8). Не використовувати: GPIO 0, 3, 45, 46 (strapping), 19/20 (USB),
26–37 (flash/PSRAM), 43/44 (UART0), 38/48 (RGB-світлодіод на DevKitC-1). **Пін ESP32 не витримує 5 В** —
напряму підключати не можна.

**Варіант А (рекомендовано): оптопара, гальванічна розв'язка.** `MAINS_ACTIVE_LEVEL LOW` (за замовчуванням).

```
 +5 В (зовнішній) ──[ 1 кОм ]──► PC817 (1, анод)
 GND (зовнішній) ─────────────── PC817 (2, катод)       опц.: 1N4148 антипаралельно світлодіоду
 GPIO4 ───────────────────────── PC817 (4, колектор)    підтяг до 3V3 — внутрішній (INPUT_PULLUP)
 GND ESP ─────────────────────── PC817 (3, емітер)
```

Потрібно: оптопара PC817 (або EL817 / 4N35) і резистор 1 кОм. Мережа є → оптопара відкрита → на піні LOW.
Спільна земля не потрібна, ESP захищений від збоїв зовнішнього джерела.

**Варіант Б (найпростіший): дільник напруги.** Потрібна спільна земля. У `config.h`: `MAINS_ACTIVE_LEVEL HIGH`.

```
 +5 В ──[ 10 кОм ]──┬── GPIO4          5 В → ≈3,0 В (поріг HIGH у S3 ≈ 2,5 В)
                    └──[ 15 кОм ]── GND
```

Потрібно: два резистори, 10 кОм і 15 кОм (до 100 нФ між GPIO4 і GND — за бажанням, проти наведень на довгому дроті).

### Синхронізація часу

Час (для журналу подій і статистики мережі) ESP бере з **[Time.Now](https://time.now/developer)**: HTTPS-запит
`https://time.now/developer/api/time?timezone=Europe%2FKyiv` без ключа, раз на 6 годин (поле `unixtime`).
Якщо сервіс недоступний 3 спроби поспіль — вмикається запасний NTP (`pool.ntp.org`), а щойно Time.Now
відповість знову, NTP зупиняється. Джерело й «вік» синхронізації видно в «Діагностиці». `TIMENOW_ENABLED 0` — лише NTP.

Сертифікат сервера не перевіряється: для цього потрібен правильний час, а ми його саме й запитуємо.
Через це не слід використовувати цей канал для чогось, окрім часу.

### Перше налаштування

1. ESP піднімає Wi-Fi **JKBMS-Setup** (пароль `jkbms1234`). Підключіться — відкриється сторінка
   налаштування (або вручну http://192.168.4.1/setup).
2. Введіть SSID і пароль вашої мережі. ESP перезапуститься.
3. Відкрийте **http://jkbms.local**. Логін `admin`, пароль `admin`.
4. **Одразу змініть пароль** у «Налаштуваннях». Він же використовується для OTA.

Якщо після старту Wi-Fi недоступна 20 с, точка доступу піднімається знову.
Коли ESP підключиться до мережі, AP вимкнеться через хвилину.


## Важливо знати

- **JK-BMS приймає лише одне BLE-підключення.** Поки ESP підключений, застосунок JK на телефоні
  до цієї BMS не підключиться, і навпаки. Тому і є автовідключення та кнопка «Відключитися».
- Якщо якась BMS у вас моніториться по BLE постійно (наприклад, ESPHome-проксі для Home Assistant),
  цей пристрій з нею конфліктуватиме. З BMS, підключеними по UART/RS485, конфлікту немає.
- **Безпека:** не відкривайте порт у інтернет. Basic auth передає пароль без шифрування — доступ
  ззовні лише через Tailscale / WireGuard.

## Налаштування в коді

Усі таймаути й константи — у `src/config.h`:

| Константа | За замовчуванням | Що робить |
|---|---|---|
| `IDLE_TIMEOUT_MS` | 3 хв | Відключення, якщо немає видимих вкладок |
| `SESSION_LIMIT_MS` | 30 хв | Жорсткий ліміт сесії |
| `STALE_DATA_MS` | 20 с | Немає кадрів → розрив |
| `SCAN_DURATION_MS` | 8 с | Тривалість сканування |
| `CONNECT_TIMEOUT_MS` | 10 с | Таймаут BLE-з'єднання |
| `HOSTNAME` | `jkbms` | Ім'я в mDNS |
| `TZ_INFO` | Київ | Часовий пояс журналу |
| `MAINS_SENSE_ENABLED` | 1 | Індикатор мережі: 1 — увімкнено, 0 — вимкнено повністю |
| `MAINS_SENSE_PIN` | GPIO4 | Пін входу сигналу «мережа є» |
| `MAINS_ACTIVE_LEVEL` | `LOW` | Рівень на піні, коли мережа є (`LOW` — оптопара, `HIGH` — дільник) |
| `MAINS_DEBOUNCE_MS` | 2 с | Скільки має триматися новий стан, щоб його прийняти |
| `MAINS_LOG_SIZE` | 25 | Скільки останніх змін стану мережі тримати в RAM і показувати |
| `TIMENOW_ENABLED` | 1 | Синхронізація часу з Time.Now (0 — лише NTP) |
| `TIMENOW_PATH` | `/developer/api/time?timezone=Europe%2FKyiv` | Запит часу (важливе лише поле `unixtime`, UTC) |
| `TIMENOW_RESYNC_MS` | 6 год | Як часто пересинхронізувати час |
| `TIMENOW_ONDEMAND_MIN_MS` | 30 с | Мін. інтервал між позачерговими синхронізаціями (подія мережі) |
| `TIMENOW_FAILS_TO_NTP` | 3 | Скільки збоїв поспіль до вмикання запасного NTP |

## Структура проєкту

```
src/
  main.cpp          Wi-Fi, вебсервер, WebSocket, сесія, OTA
  ble_manager.*     BLE: сканування, підключення, прийом кадрів (окрема задача)
  jk_protocol.*     протокол JK: кадри, CRC, розбір (без Arduino)
  settings.*        збереження налаштувань у NVS
  event_log.*       журнал подій
  mains_log.*       журнал змін стану мережі (25 записів, лише RAM)
  time_sync.*       синхронізація часу: Time.Now, запасний NTP
  web_ui.h          вебінтерфейс
  config.h          константи
test/
  test_protocol.cpp тести парсера на реальних кадрах
  fixtures.h        кадри JK-B / JK-PB
  run.sh            запуск тестів
```

## Тести

Парсер протоколу не залежить від Arduino і тестується на комп'ютері:

```sh
./test/run.sh
```

Тести проганяють реальні кадри від JK-B 10.x та JK-PB 14.x/19.x (з проєкту
[syssi/esphome-jk-bms](https://github.com/syssi/esphome-jk-bms), Apache-2.0) і перевіряють
напруги, струм, SOC, температури, ключі, аварії, CRC та визначення протоколу.

## Подяки

Опис протоколу JK по BLE — за реалізацією [syssi/esphome-jk-bms](https://github.com/syssi/esphome-jk-bms).
Точний час — безкоштовний сервіс [Time.Now](https://time.now).

---

<a id="english"></a>

# JK-BMS Web (English)

[🇺🇦 Українська](#jk-bms-web) · 🇬🇧 English

Remote access to a JK-BMS over Bluetooth through a web browser.

The ESP32-S3 sits next to the batteries. From any device on your network (or over a VPN) you can
open http://jkbms.local, scan for nearby BMS units, pick one and view its state.
When the tab is closed, the ESP disconnects by itself after 3 minutes, and the BMS is again
available to the JK phone app.

The device only **reads** data: it never switches anything and never changes BMS settings.

## Features

**Scanning**
- Active BLE scan for 8 s; the filter keeps devices whose name starts with `JK` or whose advertisement contains service FFE0 (the "show all" toggle disables it).
- Name, MAC, RSSI with a signal indicator, sorted by signal strength.
- Custom pack names ("Pack 1 · 280 Ah"), stored per MAC.
- Manual connection by MAC, without scanning.

**Connection**
- Step-by-step indication: connection → service FFE0/FFE1 → device info → data. An error is shown at the step where it happened.
- Protocol auto-detection (JK02_24S / JK02_32S) by HW version, a fallback heuristic based on the frame, and a manual choice per device.

**Data**
- Voltage, current (charge/discharge), power, SOC, remaining/nominal capacity.
- All cell voltages with min/max highlighting, delta, average.
- Temperatures (T1, T2, MOSFET), charge and discharge switch state, balancing.
- Cycles, model, HW/SW versions, serial number.
- Data "age": you can see when frames stop arriving.

**Session**
- Disconnects after **3 min** without any open *visible* tab (a hidden tab does not keep the connection alive).
- Hard limit of **30 min**; a "Extend" banner appears 2 min before the end.
- No frames for 20 s → the connection is considered dead and is dropped.
- Several tabs share the same session.

**Service**
- Access point with a captive page for the first Wi-Fi setup.
- mDNS `jkbms.local`, NTP, basic auth.
- Firmware update from the browser or via `espota`.
- Event log and diagnostics: heap/PSRAM, uptime, reset reason, Wi-Fi and BMS RSSI, CRC errors.
- Watchdogs: `loop()` watchdog and a restart if the BLE task hangs.
- Web interface in Ukrainian and English: a UK/EN switch in the header, defaulting to the browser language. The event log stays in Ukrainian.

## Hardware

- **ESP32-S3 N16R8** (16 MB flash, 8 MB PSRAM), e.g. ESP32-S3-DevKitC-1.
- 5 V power over USB or from a stable supply.
- Distance to the BMS: up to a few metres without metal obstacles; no external antenna needed.
- Enclosure: plastic.

Supported BMS: JK-B (firmware 10.x — JK02_24S, 11.x+ — JK02_32S), JK-PB (JK02_32S).
Old JK-BD units with the JK04 protocol are not supported.

### Mains indicator (optional)

The page header shows bold text: green **"MAINS OK"** or red **"NO MAINS"** (grey if the connection
to the ESP is lost and the state is unknown). The signal is an external **+5 V** that is present while the
mains is up (e.g. from a charger plugged into a wall socket). State changes are written to the event log.
To disable it set `MAINS_SENSE_ENABLED 0` in `src/config.h` — then there is no pin and no text.

Below is a collapsible **"Mains statistics"** list: the last 25 state changes (time, "restored" / "lost", duration)
plus a summary — number of outages, total time without mains, availability percentage. The log lives **in RAM only**:
after an ESP restart the history is not restored, and the first entry is the current mains state.

**The record time comes from the time service.** When the state changes, the record gets a provisional timestamp from
the ESP clock (or none if the clock is not set yet) and an immediate extra sync with Time.Now is requested (at most
once per 30 s). As soon as the clock is set, the timestamp is recomputed from the fresh time, taking into account how many
seconds have passed since the event itself — so it matches the moment of the event, not of the service reply.
If the service is unreachable (typically: the power is out, and the router with it), the request waits, and once Wi-Fi
is back the records without an exact time are refined automatically.

Pin **GPIO4** (free on the N16R8). Avoid: GPIO 0, 3, 45, 46 (strapping), 19/20 (USB), 26–37 (flash/PSRAM),
43/44 (UART0), 38/48 (RGB LED on the DevKitC-1). **ESP32 pins are not 5 V tolerant** — never connect 5 V directly.

**Option A (recommended): optocoupler, galvanic isolation.** `MAINS_ACTIVE_LEVEL LOW` (default).

```
 +5 V (external) ──[ 1 kΩ ]──► PC817 (1, anode)
 GND (external) ─────────────── PC817 (2, cathode)      optional: 1N4148 anti-parallel to the LED
 GPIO4 ──────────────────────── PC817 (4, collector)    pull-up to 3V3 is internal (INPUT_PULLUP)
 ESP GND ────────────────────── PC817 (3, emitter)
```

Parts: a PC817 optocoupler (or EL817 / 4N35) and a 1 kΩ resistor. Mains up → optocoupler on → pin reads LOW.
No common ground is needed, and the ESP is protected from faults of the external source.

**Option B (simplest): voltage divider.** Needs a common ground. In `config.h` set `MAINS_ACTIVE_LEVEL HIGH`.

```
 +5 V ──[ 10 kΩ ]──┬── GPIO4          5 V → ≈3.0 V (HIGH threshold on the S3 ≈ 2.5 V)
                   └──[ 15 kΩ ]── GND
```

Parts: two resistors, 10 kΩ and 15 kΩ (optionally up to 100 nF between GPIO4 and GND against pickup on a long wire).

### Time synchronisation

The time (for the event log and the mains statistics) comes from **[Time.Now](https://time.now/developer)**: an HTTPS request
to `https://time.now/developer/api/time?timezone=Europe%2FKyiv`, no key, every 6 hours (the `unixtime` field).
If the service is unreachable 3 times in a row, a fallback NTP (`pool.ntp.org`) is started, and it is stopped again as soon
as Time.Now answers. The source and the "age" of the last sync are shown in "Diagnostics". `TIMENOW_ENABLED 0` — NTP only.

The server certificate is not verified: that needs a correct clock, and the clock is exactly what we request.
So do not reuse this channel for anything but the time.

### First-time setup

1. The ESP starts a Wi-Fi network **JKBMS-Setup** (password `jkbms1234`). Connect to it — the setup
   page opens (or go to http://192.168.4.1/setup manually).
2. Enter the SSID and password of your network. The ESP restarts.
3. Open **http://jkbms.local**. Login `admin`, password `admin`.
4. **Change the password right away** in "Settings". The same password is used for OTA.

If Wi-Fi is unavailable 20 s after start, the access point is raised again.
Once the ESP joins your network, the AP shuts down after a minute.

## Good to know

- **A JK-BMS accepts only one BLE connection.** While the ESP is connected, the JK phone app
  cannot connect to that BMS, and vice versa. That is why there is an auto-disconnect and a "Disconnect" button.
- If one of your BMS units is monitored over BLE permanently (for example by an ESPHome proxy for Home Assistant),
  this device will conflict with it. There is no conflict with BMS units connected over UART/RS485.
- **Security:** do not expose the port to the internet. Basic auth sends the password unencrypted —
  reach the device from outside only through Tailscale / WireGuard.

## Configuration in code

All timeouts and constants are in `src/config.h`:

| Constant | Default | What it does |
|---|---|---|
| `IDLE_TIMEOUT_MS` | 3 min | Disconnect if there are no visible tabs |
| `SESSION_LIMIT_MS` | 30 min | Hard session limit |
| `STALE_DATA_MS` | 20 s | No frames → drop the connection |
| `SCAN_DURATION_MS` | 8 s | Scan duration |
| `CONNECT_TIMEOUT_MS` | 10 s | BLE connection timeout |
| `HOSTNAME` | `jkbms` | mDNS name |
| `TZ_INFO` | Kyiv | Log time zone |
| `MAINS_SENSE_ENABLED` | 1 | Mains indicator: 1 — on, 0 — fully disabled |
| `MAINS_SENSE_PIN` | GPIO4 | Input pin for the "mains present" signal |
| `MAINS_ACTIVE_LEVEL` | `LOW` | Pin level when mains is present (`LOW` — optocoupler, `HIGH` — divider) |
| `MAINS_DEBOUNCE_MS` | 2 s | How long a new state must hold before it is accepted |
| `MAINS_LOG_SIZE` | 25 | How many last mains state changes to keep in RAM and show |
| `TIMENOW_ENABLED` | 1 | Time sync from Time.Now (0 — NTP only) |
| `TIMENOW_PATH` | `/developer/api/time?timezone=Europe%2FKyiv` | Time request (only the `unixtime` field, UTC, matters) |
| `TIMENOW_RESYNC_MS` | 6 h | How often to resync the clock |
| `TIMENOW_ONDEMAND_MIN_MS` | 30 s | Minimum interval between on-demand syncs (mains event) |
| `TIMENOW_FAILS_TO_NTP` | 3 | Consecutive failures before the fallback NTP starts |

## Project structure

```
src/
  main.cpp          Wi-Fi, web server, WebSocket, session, OTA
  ble_manager.*     BLE: scanning, connecting, frame reception (separate task)
  jk_protocol.*     JK protocol: frames, CRC, parsing (no Arduino)
  settings.*        settings storage in NVS
  event_log.*       event log
  mains_log.*       mains state-change log (25 entries, RAM only)
  time_sync.*       time sync: Time.Now, fallback NTP
  web_ui.h          web interface
  config.h          constants
test/
  test_protocol.cpp parser tests on real frames
  fixtures.h        JK-B / JK-PB frames
  run.sh            test runner
```

## Tests

The protocol parser does not depend on Arduino and is tested on a computer:

```sh
./test/run.sh
```

The tests run real frames from JK-B 10.x and JK-PB 14.x/19.x (from the
[syssi/esphome-jk-bms](https://github.com/syssi/esphome-jk-bms) project, Apache-2.0) and check
voltages, current, SOC, temperatures, switches, alarms, CRC and protocol detection.

## Acknowledgements

The description of the JK BLE protocol is based on the [syssi/esphome-jk-bms](https://github.com/syssi/esphome-jk-bms) implementation.
Accurate time comes from the free [Time.Now](https://time.now) service.
