#pragma once
// Вебінтерфейс (вбудований у прошивку, без LittleFS).

static const char INDEX_HTML[] = R"HTML_RAW(<!doctype html>
<html lang="uk"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>JK-BMS Web</title>
<style>
:root{--bg:#0f1216;--card:#171b21;--line:#262c35;--txt:#e6e9ee;--mut:#8a93a1;--acc:#4ea1ff;--ok:#38c172;--warn:#f0b429;--bad:#ef5350;--chg:#38c172;--dsg:#f08c3a}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--txt);font:15px/1.4 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
header{display:flex;align-items:center;gap:12px;padding:12px 16px;border-bottom:1px solid var(--line);position:sticky;top:0;background:var(--bg);z-index:5}
header h1{font-size:17px;margin:0;font-weight:600}header .sp{flex:1}
main{max-width:980px;margin:0 auto;padding:16px;display:grid;gap:14px}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:14px}
.card h2{font-size:13px;text-transform:uppercase;letter-spacing:.06em;color:var(--mut);margin:0 0 10px;font-weight:600;display:flex;align-items:center;gap:8px}
.pill{font-size:12px;padding:3px 9px;border-radius:99px;background:var(--line);color:var(--mut);white-space:nowrap}
.pill.ok{background:#173b27;color:var(--ok)}.pill.warn{background:#3d3215;color:var(--warn)}.pill.bad{background:#3e1c1c;color:var(--bad)}.pill.acc{background:#16304d;color:var(--acc)}
button,select,input{font:inherit;color:var(--txt);background:#20262e;border:1px solid var(--line);border-radius:7px;padding:7px 12px}
button{cursor:pointer}button:hover{border-color:#3a4350}button.pri{background:var(--acc);border-color:var(--acc);color:#04121f;font-weight:600}
button.dan{border-color:#5a2a2a;color:#ff8a80}button:disabled{opacity:.45;cursor:default}
input{min-width:0}.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap}.mut{color:var(--mut)}.sm{font-size:13px}
.banner{padding:10px 14px;border-radius:8px;display:none;align-items:center;gap:10px}.banner.show{display:flex}
.banner.warn{background:#3d3215;color:#ffd666}.banner.bad{background:#3e1c1c;color:#ff8a80}
.tiles{display:grid;grid-template-columns:repeat(4,1fr);gap:10px}
.tile{background:#11151a;border:1px solid var(--line);border-radius:8px;padding:10px 12px}
.tile .l{font-size:12px;color:var(--mut)}.tile .v{font-size:26px;font-weight:600;font-variant-numeric:tabular-nums}.tile .v small{font-size:14px;color:var(--mut);font-weight:400;margin-left:3px}
.bar{height:6px;background:#0b0e12;border-radius:3px;overflow:hidden;margin-top:6px}.bar i{display:block;height:100%;background:var(--ok)}
.kv{display:grid;grid-template-columns:repeat(auto-fill,minmax(200px,1fr));gap:6px 20px;margin-top:12px}
.kv div{display:flex;justify-content:space-between;gap:8px;border-bottom:1px dashed var(--line);padding:3px 0}.kv b{font-weight:500;font-variant-numeric:tabular-nums}
.cells{display:grid;grid-template-columns:repeat(auto-fill,minmax(78px,1fr));gap:6px}
.cell{background:#11151a;border:1px solid var(--line);border-radius:6px;padding:5px 7px;font-variant-numeric:tabular-nums}
.cell .n{font-size:11px;color:var(--mut)}.cell .v{font-weight:600}.cell.min{border-color:#4a6fa5}.cell.max{border-color:#a5794a}
table{width:100%;border-collapse:collapse}td,th{padding:7px 6px;border-bottom:1px solid var(--line);text-align:left;vertical-align:middle}th{font-size:12px;color:var(--mut);font-weight:500}
td.mac{font-family:ui-monospace,Menlo,monospace;font-size:13px;color:var(--mut)}
.sig{display:inline-flex;gap:2px;align-items:flex-end;height:14px;margin-right:6px;vertical-align:-2px}.sig i{width:4px;background:#333b46;border-radius:1px}.sig i.on{background:var(--ok)}
.steps{display:flex;gap:6px;flex-wrap:wrap;margin:8px 0}.steps span{font-size:12px;padding:3px 8px;border-radius:5px;background:#11151a;color:var(--mut)}
.steps span.done{color:var(--ok)}.steps span.cur{color:var(--acc);background:#16304d}.steps span.err{color:var(--bad);background:#3e1c1c}
details summary{cursor:pointer;color:var(--mut);font-size:13px;text-transform:uppercase;letter-spacing:.06em;font-weight:600}
.log{font-family:ui-monospace,Menlo,monospace;font-size:12px;max-height:260px;overflow:auto;background:#0b0e12;border-radius:6px;padding:8px;margin-top:8px}
.log div{white-space:pre-wrap}.log .l1{color:var(--warn)}.log .l2{color:var(--bad)}
.hidden{display:none!important}.big{font-size:18px;font-weight:600}
@media(max-width:640px){.tiles{grid-template-columns:repeat(2,1fr)}.hide-m{display:none}}
</style></head><body>
<header><h1>JK-BMS Web</h1><span id="wsPill" class="pill">…</span><span class="sp"></span><span id="hdrInfo" class="mut sm hide-m"></span><select id="lang" class="sm" onchange="setLang(this.value)" title="Language"><option value="uk">UK</option><option value="en">EN</option></select></header>
<main>
<div id="bnPass" class="banner warn" data-i="passBn">Використовується пароль за замовчуванням. Змініть його в розділі «Налаштування».</div>
<div id="bnSess" class="banner warn"><span id="bnSessTxt"></span><span class="sp" style="flex:1"></span><button onclick="send({cmd:'extend'})" data-i="extend">Продовжити</button></div>

<section class="card" id="cConn">
  <h2><span data-i="hConn">Підключення</span> <span id="stPill" class="pill">—</span></h2>
  <div id="connNone" class="mut" data-i="connNone">Не підключено. Проскануйте пристрої нижче та оберіть BMS.</div>
  <div id="connBox" class="hidden">
    <div class="row"><span class="big" id="tgName"></span><span class="mut sm" id="tgMac"></span></div>
    <div class="steps" id="steps"></div>
    <div id="stMsg" class="sm"></div>
    <div class="kv">
      <div><span class="mut" data-i="lProto">Протокол</span><b id="tgProto">—</b></div>
      <div><span class="mut">RSSI</span><b id="tgRssi">—</b></div>
      <div><span class="mut" data-i="lData">Дані</span><b id="tgAge">—</b></div>
      <div><span class="mut" data-i="lDur">Тривалість</span><b id="tgConn">—</b></div>
      <div><span class="mut" data-i="lIdle">Автовідключення</span><b id="tgIdle">—</b></div>
      <div><span class="mut" data-i="lSess">Ліміт сесії</span><b id="tgSess">—</b></div>
    </div>
    <div class="row" style="margin-top:12px">
      <button class="dan" id="btnDisc" onclick="send({cmd:'disconnect'})" data-i="disc">Відключитися</button>
      <button id="btnExt" onclick="send({cmd:'extend'})" data-i="extSess">Продовжити сесію</button>
      <button id="btnRe" class="hidden" onclick="connectTo(S.target.mac)" data-i="reconn">Підключитися знову</button>
    </div>
  </div>
</section>

<section class="card hidden" id="cData">
  <h2><span data-i="hBat">Батарея</span> <span id="mosPill" class="pill"></span><span id="balPill" class="pill hidden" data-i="bal">Балансування</span></h2>
  <div id="errBox" class="banner bad"></div>
  <div class="tiles">
    <div class="tile"><div class="l" data-i="lV">Напруга</div><div class="v"><span id="dV">—</span><small data-i="uV">В</small></div></div>
    <div class="tile"><div class="l" id="dIl">Струм</div><div class="v"><span id="dI">—</span><small data-i="uA">А</small></div></div>
    <div class="tile"><div class="l" data-i="lP">Потужність</div><div class="v"><span id="dP">—</span><small data-i="uW">Вт</small></div></div>
    <div class="tile"><div class="l">SOC</div><div class="v"><span id="dSoc">—</span><small>%</small></div><div class="bar"><i id="dSocBar" style="width:0"></i></div></div>
  </div>
  <div class="kv">
    <div><span class="mut" data-i="lCap">Залишок</span><b id="dCap">—</b></div>
    <div><span class="mut" data-i="lCmin">Комірка min</span><b id="dCmin">—</b></div>
    <div><span class="mut" data-i="lCmax">Комірка max</span><b id="dCmax">—</b></div>
    <div><span class="mut" data-i="lDelta">Дельта</span><b id="dDelta">—</b></div>
    <div><span class="mut" data-i="lAvg">Середня</span><b id="dAvg">—</b></div>
    <div><span class="mut" data-i="lTmos">T MOSFET</span><b id="dTm">—</b></div>
    <div><span class="mut">T1</span><b id="dT1">—</b></div>
    <div><span class="mut">T2</span><b id="dT2">—</b></div>
    <div><span class="mut" data-i="lChg">Заряд</span><b id="dChg">—</b></div>
    <div><span class="mut" data-i="lDsg">Розряд</span><b id="dDsg">—</b></div>
    <div><span class="mut" data-i="lBalI">Струм балансу</span><b id="dBal">—</b></div>
    <div><span class="mut" data-i="lCyc">Цикли</span><b id="dCyc">—</b></div>
  </div>
  <details style="margin-top:14px" id="cellsDet"><summary><span data-i="cells">Комірки</span> (<span id="cellsN">0</span>)</summary><div class="cells" id="cells" style="margin-top:10px"></div></details>
  <details style="margin-top:10px"><summary data-i="sInfo">Інформація про BMS</summary><div class="kv" id="infoKv"></div></details>
</section>

<section class="card">
  <h2><span data-i="hDev">Пристрої поблизу</span> <span id="scPill" class="pill hidden"></span></h2>
  <div class="row">
    <button class="pri" id="btnScan" onclick="send({cmd:'scan'})">Сканувати</button>
    <label class="row sm mut"><input type="checkbox" id="showAll" onchange="renderDevices()"> <span data-i="showAll">показати всі BLE-пристрої</span></label>
    <span class="mut sm" id="scInfo"></span>
  </div>
  <table style="margin-top:10px"><thead><tr><th data-i="thDev">Пристрій</th><th class="hide-m">MAC</th><th data-i="thSig">Сигнал</th><th class="hide-m" data-i="lProto">Протокол</th><th></th></tr></thead><tbody id="devs"></tbody></table>
  <div id="devEmpty" class="mut sm" style="padding:8px 0" data-i="devEmpty">Немає результатів.</div>
  <div class="row sm" style="margin-top:10px"><span class="mut" data-i="manual">Вручну:</span><input id="manMac" placeholder="C8:47:80:XX:XX:XX" size="18"><button onclick="connectTo(manMac.value)" data-i="connect">Підключити</button></div>
</section>

<details class="card"><summary data-i="sDiag">Діагностика і журнал</summary>
  <div class="kv" id="diagKv"></div>
  <div class="log" id="log"></div>
</details>

<details class="card"><summary data-i="sSet">Налаштування</summary>
  <div style="display:grid;gap:14px;margin-top:12px">
    <form class="row" onsubmit="return post(event,'/api/password',{p:pw.value},t('pwOk'))">
      <span class="mut" style="min-width:120px" data-i="lPass">Пароль (admin)</span><input id="pw" type="password" placeholder="новий пароль, мін. 6" data-ip="phPw" minlength="6" required><button data-i="change">Змінити</button></form>
    <form class="row" onsubmit="return post(event,'/api/wifi',{ssid:wss.value,pass:wps.value},t('wifiOk'))">
      <span class="mut" style="min-width:120px">Wi-Fi</span><input id="wss" placeholder="SSID" required><input id="wps" type="password" placeholder="пароль" data-ip="phWifiPass"><button data-i="save">Зберегти</button></form>
    <form class="row" onsubmit="return upload(event)">
      <span class="mut" style="min-width:120px" data-i="lFw">Прошивка</span><input type="file" id="fw" accept=".bin" required><button data-i="update">Оновити</button><span id="fwSt" class="mut sm"></span></form>
    <div class="row"><span class="mut" style="min-width:120px" data-i="lDevice">Пристрій</span><button class="dan" data-i="reboot" onclick="if(confirm(t('rebootQ')))post(null,'/api/reboot',{},t('rebooting'))">Перезапустити</button></div>
  </div>
</details>
</main>
<script>
const $=id=>document.getElementById(id);
let ws,S={},retry=0,lastLogId=0,hbTimer,wsOn=false;

// ---- Мова (uk / en). Статичні тексти — data-i / data-ip, динамічні — t(). ----
const I={
passBn:['Використовується пароль за замовчуванням. Змініть його в розділі «Налаштування».','The default password is in use. Change it in "Settings".'],
extend:['Продовжити','Extend'],hConn:['Підключення','Connection'],
connNone:['Не підключено. Проскануйте пристрої нижче та оберіть BMS.','Not connected. Scan for devices below and pick a BMS.'],
lProto:['Протокол','Protocol'],lData:['Дані','Data'],lDur:['Тривалість','Duration'],lIdle:['Автовідключення','Auto-disconnect'],lSess:['Ліміт сесії','Session limit'],
disc:['Відключитися','Disconnect'],extSess:['Продовжити сесію','Extend session'],reconn:['Підключитися знову','Reconnect'],
hBat:['Батарея','Battery'],bal:['Балансування','Balancing'],lV:['Напруга','Voltage'],uV:['В','V'],uA:['А','A'],lP:['Потужність','Power'],uW:['Вт','W'],
lCap:['Залишок','Remaining'],lCmin:['Комірка min','Cell min'],lCmax:['Комірка max','Cell max'],lDelta:['Дельта','Delta'],lAvg:['Середня','Average'],lTmos:['T MOSFET','T MOSFET'],
lChg:['Заряд','Charge'],lDsg:['Розряд','Discharge'],lBalI:['Струм балансу','Balance current'],lCyc:['Цикли','Cycles'],
cells:['Комірки','Cells'],sInfo:['Інформація про BMS','BMS information'],hDev:['Пристрої поблизу','Nearby devices'],showAll:['показати всі BLE-пристрої','show all BLE devices'],
thDev:['Пристрій','Device'],thSig:['Сигнал','Signal'],devEmpty:['Немає результатів.','No results.'],manual:['Вручну:','Manual:'],connect:['Підключити','Connect'],
sDiag:['Діагностика і журнал','Diagnostics and log'],sSet:['Налаштування','Settings'],lPass:['Пароль (admin)','Password (admin)'],phPw:['новий пароль, мін. 6','new password, min. 6'],
change:['Змінити','Change'],phWifiPass:['пароль','password'],save:['Зберегти','Save'],lFw:['Прошивка','Firmware'],update:['Оновити','Update'],lDevice:['Пристрій','Device'],reboot:['Перезапустити','Restart'],
pwOk:['Пароль змінено. Пристрій перезапускається, увійдіть з новим паролем.','Password changed. The device is restarting, sign in with the new password.'],
wifiOk:['Wi-Fi збережено, перезапуск…','Wi-Fi saved, restarting…'],rebootQ:['Перезапустити ESP32?','Restart the ESP32?'],rebooting:['Перезапуск…','Restarting…'],
online:['Онлайн','Online'],offline:['Немає зв\'язку','No connection'],
sIdle:['Не підключено','Not connected'],sScanning:['Сканування','Scanning'],sConnecting:['Підключення','Connecting'],sDiscovering:['Пошук сервісу','Finding service'],
sWaitinfo:['Запит інформації','Requesting info'],sStreaming:['Підключено','Connected'],sError:['Помилка','Error'],
stepConn:['З\'єднання','Link'],stepSvc:['Сервіс FFE0/FFE1','Service FFE0/FFE1'],stepInfo:['Device info','Device info'],stepData:['Дані','Data'],
dbm:['дБм','dBm'],badMac:['Невірна MAC-адреса','Invalid MAC address'],
switchQ:['Зараз підключено до іншого пристрою. Відключитися і перейти?','Currently connected to another device. Disconnect and switch?'],
aliasP:['Назва для {0} (порожньо — видалити):','Name for {0} (empty to remove):'],aliasT:['Назва','Name'],
min:['хв','m'],sec:['с','s'],uD:['д','d'],uH:['год','h'],unnamed:['Без назви','Unnamed'],manualTag:['(вручну)','(manual)'],
justNow:['щойно','just now'],agoS:['{0} с тому','{0} s ago'],frames:['кадрів','frames'],
sessEnds:['Сесія завершиться через ','Session ends in '],iCharge:['Струм · заряд','Current · charge'],iDisch:['Струм · розряд','Current · discharge'],iCur:['Струм','Current'],
ah:['Аг','Ah'],mv:['мВ','mV'],on:['увімк.','on'],off:['вимк.','off'],mosOff:['MOS вимк.','MOS off'],alarms:['Аварії: ','Alarms: '],bit:['Біт ','Bit '],
iModel:['Модель','Model'],iName:['Ім\'я','Name'],iSerial:['Серійний №','Serial #'],iMfg:['Дата виробн.','Mfg. date'],iPwrOn:['Увімкнень','Power-ons'],
scanning:['Сканування… {0} с','Scanning… {0} s'],scan:['Сканувати','Scan'],scanBusy:['Сканування недоступне під час підключення','Scanning is unavailable while connected'],scanLast:['останнє: {0} тому','last: {0} ago'],
dFw:['Прошивка','Firmware'],dUp:['Аптайм','Uptime'],dReset:['Причина старту','Reset reason'],dClients:['Вебклієнтів','Web clients'],dActive:['активних','active'],dCrc:['Помилок CRC','CRC errors'],heapMin:['мін','min'],kb:['КБ','KB'],
connected:['підключено','connected'],auto:['Авто','Auto'],err:['Помилка: ','Error: '],
fwQ:['Оновити прошивку з {0}? BMS буде відключено.','Update firmware from {0}? The BMS will be disconnected.'],fwDone:['Готово, перезапуск…','Done, restarting…']
};
// Тексти, які надсилає прошивка (статус / причина розриву / причина старту) -> англійською.
const M={
'Встановлення BLE-з\'єднання…':'Establishing BLE connection…','Пошук сервісу FFE0…':'Looking for service FFE0…','Пошук характеристики FFE1…':'Looking for characteristic FFE1…',
'Підписка на нотифікації…':'Subscribing to notifications…','Запит інформації про пристрій…':'Requesting device info…','Отримання даних':'Receiving data','Сканування…':'Scanning…',
'Не вдалося запустити сканування':'Failed to start scanning',
'Не вдалося встановити з\'єднання (пристрій недоступний або зайнятий іншим клієнтом)':'Failed to connect (device unreachable or in use by another client)',
'Таймаут BLE-з\'єднання':'BLE connection timeout','Пристрій не відповідає на пошук сервісів (не JK-BMS?)':'Device does not respond to service discovery (not a JK-BMS?)','Сервіс FFE0 не знайдено — це не JK-BMS?':'Service FFE0 not found — is this a JK-BMS?','Характеристику FFE1 не знайдено':'Characteristic FFE1 not found',
'Не вдалося підписатися на нотифікації FFE1':'Failed to subscribe to FFE1 notifications','Перемикання на інший пристрій':'Switching to another device',
'Відключено користувачем':'Disconnected by user','Оновлення прошивки':'Firmware update','Оновлення прошивки (OTA)':'Firmware update (OTA)','Перезапуск':'Restart',
'Простій 3 хв: немає жодного активного вебклієнта':'Idle for 3 min: no active web client','Досягнуто ліміт сесії 30 хв':'30 min session limit reached',
'Увімкнення живлення':'Power on','Програмний перезапуск':'Software restart','Паніка (exception)':'Panic (exception)','Interrupt watchdog':'Interrupt watchdog','Task watchdog':'Task watchdog',
'Watchdog':'Watchdog','Просідання живлення':'Brownout','Вихід зі сну':'Deep sleep wake','Зовнішній reset':'External reset','Невідомо':'Unknown'
};
const MR=[[/^З'єднання розірвано \(код (-?\d+)\)$/,'Connection lost (code $1)'],[/^Немає даних від BMS понад (\d+) с$/,'No data from BMS for over $1 s']];
const E_EN=['Balance wire resistance','MOSFET overtemperature','Cell count differs from settings',null,'Battery fully charged','Pack overvoltage','Charge overcurrent','Charge short circuit','Charge overtemperature','Charge low temperature','Coprocessor communication error','Cell undervoltage','Pack undervoltage','Discharge overcurrent','Discharge short circuit','Discharge overtemperature','Charge MOSFET fault','Discharge MOSFET fault','GPS disconnected','Time to change password','Failed to enable discharge','Battery overtemperature','Temperature sensor fault','PL module fault','Failed to clear short-circuit protection','Discharge current OCP II','Discharge current OCP III','Discharge low temperature','GPS remote lock'];

let lang='uk';
try{lang=localStorage.getItem('lang')||''}catch(x){}
if(lang!=='uk'&&lang!=='en')lang=(navigator.language||'').toLowerCase().startsWith('uk')?'uk':'en';
const li=()=>lang==='uk'?0:1;
function t(k,...a){const v=(I[k]||[k,k])[li()];return a.length?v.replace(/\{(\d+)\}/g,(m,n)=>a[n]):v}
function srv(s){
  if(lang==='uk'||!s)return s;
  if(M[s])return M[s];
  for(const [re,r] of MR)if(re.test(s))return s.replace(re,r);
  return s;
}
function errText(D){
  if(lang==='uk')return D.errTxt;
  const o=[];for(let b=0;b<32;b++)if(D.err>>>b&1)o.push(E_EN[b]||t('bit')+b);return o.join('; ');
}
function applyLang(){
  document.documentElement.lang=lang;$('lang').value=lang;
  document.querySelectorAll('[data-i]').forEach(e=>{e.textContent=t(e.dataset.i)});
  document.querySelectorAll('[data-ip]').forEach(e=>{e.placeholder=t(e.dataset.ip)});
  setWsPill();
  if(S.t)render();
}
function setLang(l){lang=l;try{localStorage.setItem('lang',l)}catch(x){}applyLang()}

const ST={idle:['sIdle',''],scanning:['sScanning','acc'],connecting:['sConnecting','acc'],discovering:['sDiscovering','acc'],waitinfo:['sWaitinfo','acc'],streaming:['sStreaming','ok'],error:['sError','bad']};
const STEPS=[['connecting','stepConn'],['discovering','stepSvc'],['waitinfo','stepInfo'],['streaming','stepData']];

function setWsPill(){pill('wsPill',wsOn?t('online'):t('offline'),wsOn?'ok':'bad')}
function connectWs(){
  ws=new WebSocket((location.protocol==='https:'?'wss://':'ws://')+location.host+'/ws');
  ws.onopen=()=>{retry=0;wsOn=true;setWsPill();hb()};
  ws.onclose=()=>{wsOn=false;setWsPill();setTimeout(connectWs,Math.min(10000,1000*++retry))};
  ws.onmessage=e=>{try{const m=JSON.parse(e.data);if(m.t==='state'){S=m;render()}}catch(x){console.error(x)}};
}
function send(o){if(ws&&ws.readyState===1)ws.send(JSON.stringify(o))}
function hb(){send({cmd:'hb',vis:document.visibilityState==='visible'})}
document.addEventListener('visibilitychange',hb);
setInterval(hb,10000);

function pill(id,t,c){const e=$(id);e.textContent=t;e.className='pill '+(c||'')}
function fmtDur(s){if(s==null||s<0)return '—';const m=Math.floor(s/60),x=s%60;return m?`${m} ${t('min')} ${String(x).padStart(2,'0')} ${t('sec')}`:`${x} ${t('sec')}`}
function fmtUp(s,short){const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);return (d?d+' '+t('uD')+' ':'')+(short&&!d&&!h?'':h+' '+t('uH')+' ')+m+' '+t('min')}
function esc(s){return String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]))}
function sig(r){const n=r>=-60?4:r>=-70?3:r>=-80?2:r>=-90?1:0;return '<span class="sig">'+[6,9,12,14].map((h,i)=>`<i style="height:${h}px" class="${i<n?'on':''}"></i>`).join('')+'</span>'+r+' '+t('dbm')}
function aliasOf(mac){return (S.aliases||{})[mac]||''}
function linked(st){return ['connecting','discovering','waitinfo','streaming'].includes(st)}

function connectTo(mac){
  mac=(mac||'').trim().toUpperCase();
  if(!/^([0-9A-F]{2}:){5}[0-9A-F]{2}$/.test(mac)){alert(t('badMac'));return}
  if(linked(S.st)&&S.target&&S.target.mac!==mac&&!confirm(t('switchQ')))return;
  send({cmd:'connect',mac});
}
function editAlias(mac){const n=prompt(t('aliasP',mac),aliasOf(mac));if(n!==null)send({cmd:'alias',mac,name:n})}
function setProto(mac,p){send({cmd:'proto',mac,p:+p})}

function render(){
  const st=S.st||'idle',[stKey,stCls]=ST[st]||[st,''];
  pill('stPill',ST[st]?t(stKey):st,stCls);
  const d=S.diag||{};
  $('hdrInfo').textContent=(d.ip||'')+(d.wifiRssi?' · Wi-Fi '+d.wifiRssi+' '+t('dbm'):'');
  $('bnPass').classList.toggle('show',!!d.defPass);

  // --- Підключення ---
  const tg=S.target,isL=linked(st);
  $('connNone').classList.toggle('hidden',!!tg&&(isL||st==='error'||S.data));
  $('connBox').classList.toggle('hidden',!(tg&&(isL||st==='error'||S.data)));
  if(tg){
    $('tgName').textContent=tg.alias||tg.name||t('unnamed');
    $('tgMac').textContent=tg.mac+(tg.alias&&tg.name?' · '+tg.name:'');
    $('tgProto').textContent=tg.proto+(tg.forced?' '+t('manualTag'):'');
    $('tgRssi').innerHTML=isL&&tg.rssi?sig(tg.rssi):'—';
    $('tgAge').textContent=tg.ageMs>=0&&isL?(tg.ageMs<1500?t('justNow'):t('agoS',Math.round(tg.ageMs/1000)))+' · '+t('frames')+' '+tg.frames:'—';
    $('tgAge').style.color=tg.ageMs>10000&&isL?'var(--warn)':'';
    $('tgConn').textContent=isL?fmtDur(tg.connS):'—';
  }
  $('tgIdle').textContent=isL?fmtDur(S.idleLeft):'—';
  $('tgSess').textContent=isL?fmtDur(S.sessLeft):'—';
  $('stMsg').textContent=srv(S.msg||'');
  $('stMsg').style.color=st==='error'?'var(--bad)':'';
  $('btnDisc').classList.toggle('hidden',!isL);$('btnExt').classList.toggle('hidden',st!=='streaming');
  $('btnRe').classList.toggle('hidden',isL||!tg);
  const cur=STEPS.findIndex(s=>s[0]===st);
  $('steps').innerHTML=STEPS.map((s,i)=>{let c='';if(st==='streaming'||i<cur)c='done';if(i===cur&&st!=='streaming')c='cur';if(st==='streaming'&&i===cur)c='done';return `<span class="${c}">${i<cur||st==='streaming'?'✓ ':''}${t(s[1])}</span>`}).join('');
  $('steps').classList.toggle('hidden',!isL);

  const sl=S.sessLeft;
  $('bnSess').classList.toggle('show',isL&&sl!=null&&sl<120);
  $('bnSessTxt').textContent=t('sessEnds')+fmtDur(sl);

  // --- Дані ---
  const D=S.data;
  $('cData').classList.toggle('hidden',!D);
  if(D){
    $('dV').textContent=D.v;$('dP').textContent=D.p;$('dSoc').textContent=D.soc;
    const i=+D.i;$('dI').textContent=(i>0?'+':'')+D.i;
    $('dIl').textContent=i>0.05?t('iCharge'):i<-0.05?t('iDisch'):t('iCur');
    $('dI').style.color=i>0.05?'var(--chg)':i<-0.05?'var(--dsg)':'';
    $('dSocBar').style.width=D.soc+'%';$('dSocBar').style.background=D.soc<15?'var(--bad)':D.soc<35?'var(--warn)':'var(--ok)';
    $('dCap').textContent=D.capR+' / '+D.capN+' '+t('ah');
    $('dCmin').textContent=D.cMin+' '+t('uV')+' (#'+D.cMinI+')';$('dCmax').textContent=D.cMax+' '+t('uV')+' (#'+D.cMaxI+')';
    $('dDelta').textContent=Math.round(D.cDelta*1000)+' '+t('mv');$('dDelta').style.color=D.cDelta>0.05?'var(--warn)':'';
    $('dAvg').textContent=D.cAvg+' '+t('uV');
    $('dTm').textContent=D.tMos+' °C';$('dT1').textContent=D.t1+' °C';$('dT2').textContent=D.t2+' °C';
    $('dChg').innerHTML=D.chg?`<span style="color:var(--ok)">${t('on')}</span>`:`<span style="color:var(--bad)">${t('off')}</span>`;
    $('dDsg').innerHTML=D.dsg?`<span style="color:var(--ok)">${t('on')}</span>`:`<span style="color:var(--bad)">${t('off')}</span>`;
    $('dBal').textContent=D.balI+' '+t('uA');$('dCyc').textContent=D.cyc+' ('+D.cycCap+' '+t('ah')+')';
    pill('mosPill',(D.chg?'CHG ':'')+(D.dsg?'DSG':'')||t('mosOff'),D.chg&&D.dsg?'ok':'warn');
    $('balPill').classList.toggle('hidden',!D.bal);
    $('errBox').classList.toggle('show',!!D.err);$('errBox').textContent=D.err?t('alarms')+errText(D)+' (0x'+D.err.toString(16).toUpperCase().padStart(D.err>0xFFFF?8:4,'0')+')':'';
    const c=D.cells||[];$('cellsN').textContent=c.length;
    $('cells').innerHTML=c.map((v,k)=>`<div class="cell ${k+1==D.cMinI?'min':''} ${k+1==D.cMaxI?'max':''}"><div class="n">#${k+1}</div><div class="v">${(+v).toFixed(3)}</div></div>`).join('');
  }
  const In=S.info;
  $('infoKv').innerHTML=In?[[t('iModel'),In.model],['HW',In.hw],['SW',In.sw],[t('iName'),In.name],[t('iSerial'),In.serial],[t('iMfg'),In.mfg],[t('iPwrOn'),In.pwrOn]].map(x=>`<div><span class="mut">${x[0]}</span><b>${esc(x[1])}</b></div>`).join(''):'<span class="mut">—</span>';

  // --- Сканування ---
  const sc=S.scan||{};
  $('btnScan').disabled=sc.running||isL;
  $('btnScan').textContent=sc.running?t('scanning',sc.leftS):t('scan');
  $('scInfo').textContent=isL?t('scanBusy'):sc.agoS>=0?t('scanLast',fmtDur(sc.agoS)):'';
  renderDevices();

  // --- Діагностика ---
  $('diagKv').innerHTML=[[t('dFw'),d.fw],[t('dUp'),fmtUp(d.up||0)],['Heap',Math.round(d.heap/1024)+' '+t('kb')+' ('+t('heapMin')+' '+Math.round(d.heapMin/1024)+')'],['PSRAM',Math.round(d.psram/1024)+' '+t('kb')],[t('dReset'),srv(d.reset)],['IP',d.ip],[t('dClients'),d.clients+' ('+t('dActive')+' '+d.active+')'],[t('dCrc'),tg?tg.crc:0]].map(x=>`<div><span class="mut">${x[0]}</span><b>${esc(x[1])}</b></div>`).join('');
  const lg=$('log'),atBottom=lg.scrollTop+lg.clientHeight>=lg.scrollHeight-5;
  for(const e of S.log||[]){
    if(e.id<=lastLogId)continue;lastLogId=e.id;
    const tm=e.ts?new Date(e.ts*1000).toLocaleTimeString(lang==='uk'?'uk-UA':'en-GB'):'+'+fmtUp(Math.floor(e.ms/1000),true);
    const div=document.createElement('div');div.className='l'+e.l;div.textContent=tm+'  '+e.m;lg.appendChild(div);
  }
  while(lg.children.length>200)lg.removeChild(lg.firstChild);
  if(atBottom)lg.scrollTop=lg.scrollHeight;
}

function renderDevices(){
  const sc=S.scan||{},all=$('showAll').checked,isL=linked(S.st);
  let list=(sc.devices||[]).filter(d=>all||d.jk||/^JK/i.test(d.name)||aliasOf(d.mac));
  list.sort((a,b)=>b.rssi-a.rssi);
  $('devEmpty').classList.toggle('hidden',list.length>0);
  const pr=S.protos||{};
  $('devs').innerHTML=list.map(d=>{
    const al=aliasOf(d.mac),cur=S.target&&S.target.mac===d.mac&&isL,p=pr[d.mac]||0;
    return `<tr><td><b>${esc(al||d.name||'—')}</b>${al&&d.name?'<div class="mut sm">'+esc(d.name)+'</div>':''} <button class="sm" style="padding:2px 7px" onclick="editAlias('${d.mac}')" title="${t('aliasT')}">✎</button></td>
    <td class="mac hide-m">${d.mac}</td><td class="sm">${sig(d.rssi)}</td>
    <td class="hide-m"><select class="sm" onchange="setProto('${d.mac}',this.value)"><option value="0"${p==0?' selected':''}>${t('auto')}</option><option value="24"${p==24?' selected':''}>JK02_24S</option><option value="32"${p==32?' selected':''}>JK02_32S</option></select></td>
    <td>${cur?`<span class="pill ok">${t('connected')}</span>`:`<button onclick="connectTo('${d.mac}')"${sc.running?' disabled':''}>${t('connect')}</button>`}</td></tr>`}).join('');
}

function post(e,url,data,okMsg){
  if(e)e.preventDefault();
  fetch(url,{method:'POST',body:new URLSearchParams(data)}).then(r=>r.text().then(x=>{alert(r.ok?okMsg:t('err')+x)})).catch(x=>alert(t('err')+x));
  return false;
}
function upload(e){
  e.preventDefault();const f=$('fw').files[0];if(!f)return false;
  if(!confirm(t('fwQ',f.name)))return false;
  const fd=new FormData();fd.append('firmware',f,f.name);
  const x=new XMLHttpRequest();x.open('POST','/update');
  x.upload.onprogress=ev=>{$('fwSt').textContent=Math.round(ev.loaded/ev.total*100)+' %'};
  x.onload=()=>{$('fwSt').textContent=x.status==200?t('fwDone'):t('err')+x.responseText};
  x.send(fd);return false;
}
applyLang();
connectWs();
</script></body></html>)HTML_RAW";

static const char SETUP_HTML[] = R"HTML_RAW(<!doctype html>
<html lang="uk"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>JK-BMS Web · Wi-Fi</title>
<style>body{font:16px system-ui,sans-serif;background:#0f1216;color:#e6e9ee;display:flex;justify-content:center;padding:40px 16px}
form{background:#171b21;border:1px solid #262c35;border-radius:10px;padding:20px;display:grid;gap:12px;width:100%;max-width:340px}
input,button{font:inherit;padding:10px;border-radius:7px;border:1px solid #262c35;background:#20262e;color:#e6e9ee}button{background:#4ea1ff;color:#04121f;font-weight:600;border:0}</style></head>
<body><form method="post" action="/setup"><h2 style="margin:0" data-i="0">Налаштування Wi-Fi</h2>
<input name="ssid" placeholder="Назва мережі (SSID)" data-ip="1" required><input name="pass" type="password" placeholder="Пароль" data-ip="2">
<button data-i="3">Зберегти і перезапустити</button>
<small style="color:#8a93a1" data-i="4">Після перезапуску відкрийте http://jkbms.local (логін admin / admin).</small></form>
<script>
const EN=['Wi-Fi setup','Network name (SSID)','Password','Save and restart','After the restart open http://jkbms.local (login admin / admin).'];
if(!(navigator.language||'').toLowerCase().startsWith('uk')){document.documentElement.lang='en';
document.querySelectorAll('[data-i]').forEach(e=>e.textContent=EN[e.dataset.i]);
document.querySelectorAll('[data-ip]').forEach(e=>e.placeholder=EN[e.dataset.ip])}
</script></body></html>)HTML_RAW";
