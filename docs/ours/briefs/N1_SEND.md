# N1: Send tab (connect menu, hotspot in two steps, ready / receiving, starting, failed)

Repo: `/Users/daniilhurevych/Wren/firmware`, branch `ours`. The tree has uncommitted Wren setup/library work
(i18n yamls, `WrenChrome`, `ActivityManager.cpp`, `src/wren/setup/`, ...): it is ours, keep it, build on it.
C++ (gnu++2a), PlatformIO + Arduino, ESP32-C3 baseline (~380 KB RAM, keep >50 KB free, no leaks).

Design (source of truth for geometry and words), 528x792 portrait, 1-bit (#151515 ink, #F3F1EA paper), in
`docs/ours/design/send/`: `TransferMode.html` (connect menu), `HotspotJoin.html`, `HotspotOpen.html`,
`SendReady.html`, `SendReceiving.html`, `SendStarting.html`, `SendFailed.html`. Ignore fonts and the tab colors in the
HTML; use the Wren fonts (`src/wren/ui/WrenFonts.h`) and draw the tab header with the existing
`wren::ui::drawTabHeader` exactly as home/setup do. Size mapping: design 56 px headline → Golos 40, 40 → Golos 40,
32 → Golos 32, 24 → Golos 24, 18 → Golos 18, mono 16 labels → `drawLabel`.

Patterns to copy: `src/wren/setup/WrenSetupActivity.{h,cpp}` (tab header, big rows, side buttons switching tabs,
bottom tabs, landscape scrolling), `src/wren/ui/WrenChrome.{h,cpp}` (`drawHeader`, `drawRow`, `drawLabel`,
`drawTextFit`, `drawTextWrapped`, `drawProgressBar`, `drawButtonTabs`, `drawEdgeTabs`), `src/util/QrUtils.h`.

Engine to mirror: `src/activities/network/CrossPointWebServerActivity.{h,cpp}` (upstream, 590 lines). Read all of it.
The new activity keeps its whole lifecycle, behaviour for behaviour: SD-font cache release in `onEnter` and before the
server alloc, `onExit` (DNS stop, `MDNS.end()`, `pluginevents::refreshSubscriptions()`, Wi-Fi off, `silentRestart()`),
the heap-defrag reboot before Join Network (`silentRestartToJoinNetwork()`, `startInJoinNetwork` flag), mDNS hostname
`crosspoint` (do not change it), captive DNS in AP mode, `setUploadCancelCheck`, the `handleClient` loop with watchdog
resets and input pumping, the STA health check with `WIFI_ABANDON_MS`, `pluginevents::drain` in STA, `skipLoopDelay`,
`preventAutoSleep`. Put a comment at the top: "Mirrors CrossPointWebServerActivity @ 622e5de9; keep in sync on upstream
merges." Only rendering, the menu and the states below change.

## Constraints (hard)

No flashing, no `erase_flash`/`write_flash`, no commits, no push. Do not edit `src/network/` (read it, never change it),
`freeink-sdk/`, `lib/hal/`, `docs/webserver-endpoints.md`. Do not edit `CrossPointWebServerActivity` either (it stays
as the upstream fallback). Generated files: only `lib/I18n/I18nKeys.h` via `scripts/gen_i18n.py`. New code in
`src/wren/send/`. Upstream edits: only the routing in `ActivityManager.cpp` below. HTTP API, ports, discovery, mDNS name
and `firmwareFamily` must not change. Do not run builds or tests and report them; I run them. No heap allocation in
render or per-loop paths; justify every allocation in a comment.

## Files

- `src/wren/send/WrenSendActivity.{h,cpp}`: the activity.
- `src/wren/send/SendState.h`: pure logic, no Arduino headers (host-tested): state enum, hotspot step machine,
  received list, SSID formatting, percent and redraw decisions, row list for the menu, landscape scroll window.
- `test/wren_layout/SendStateTest.cpp`, registered in `test/wren_layout/CMakeLists.txt`.

## Routing (`src/activities/ActivityManager.cpp`)

`goToFileTransfer()` → `WrenSendActivity(renderer, mappedInput)`. `goToJoinNetwork()` →
`WrenSendActivity(renderer, mappedInput, /*startInJoinNetwork=*/true)`. Grep for other `CrossPointWebServerActivity`
constructions and list them in your report; do not change them.

## States

`CHOOSE` → (`JOIN_PICK` child) | `AP_STARTING` → `RUNNING` | `FAILED`. Plus children for Calibre and USB.

### 1. CHOOSE: connect menu (`TransferMode.html`)

`drawMenuStatusBar`, `drawTabHeader(renderer, y, 1)` (send active), headline "connect how?" (Golos 40, 20 px sides,
28 px top / 24 px bottom, 3 px ink rule under it), then rows of 104 px: Golos 32 label, chevron right, 3 px divider,
selected row inverted. Rows: join a network · create hotspot · calibre wireless · usb drive (this row only
`#if FREEINK_CAP_USB_MSC`). Label "2.4 ghz networks only" (mono) bottom-left above the bottom tabs. Bottom tabs
back · select · ▲ · ▼, `drawEdgeTabs(true, true)`.

Input: ▲▼ move (wrap, hold repeats), select opens, back → `activityManager.goHome()`. Side buttons switch tabs in the
order library · send · setup, exactly mirroring how `WrenSetupActivity` and `WrenHomeActivity` map them (setup is to
the right of send, library to the left). OPDS is not in this menu (it lives elsewhere); do not add it.

- join a network: as upstream `onNetworkModeSelected(JOIN_NETWORK)` (reboot first unless `startInJoinNetwork`), then
  `WiFi.mode(WIFI_STA)` and upstream `WifiSelectionActivity` as a child (restyled in a later brief). Cancel → CHOOSE.
  Connected → mDNS + server → RUNNING (STA).
- create hotspot: go to AP_STARTING (below).
- calibre wireless: upstream `CalibreConnectActivity` child; on return → CHOOSE.
- usb drive: `activityManager.goToUsbDrive()`.

### 2. AP_STARTING (`SendStarting.html`)

`drawHeader(renderer, 28, tr(STR_WREN_TAB_SEND), "hotspot")`, headline "one moment", Golos 18 text "Turning on the
hotspot. It takes a few seconds." Bottom tab: stop (slot 0 only). Make this frame actually reach the panel before
the blocking AP start: set the state, `requestUpdate()`, and call `startAccessPoint()` from the next `loop()` pass,
not inline.

AP: SSID `wren-xxxx`, xxxx = last two bytes of the soft-AP MAC as lowercase hex (`wren-3f2a`); format it in
`SendState.h` from 6 MAC bytes. Open network (no password), channel 1, max 4 clients, as upstream. Failure of
`softAP` or of the server → FAILED (reason hotspot), never `onGoHome()`.

### 3. RUNNING (hotspot): step JOIN (`HotspotJoin.html`) and step OPEN (`HotspotOpen.html`)

Header `send` / `hotspot`. One QR at a time, 198x198 at x 20, fields in a column to its right (label + Golos 32 value,
18 px apart), vertically centred on the QR.

- JOIN: headline "join wren", text "Point your phone camera at the code.", QR payload
  `WIFI:T:nopass;S:<ssid>;;`, fields network = ssid, password = none. Under it a box (3 px ink border, 14/18 px
  padding): label "next", Golos 18 "Once your phone joins, the address to open shows up here."
- OPEN: headline "phone joined", text "Scan again to open Wren in the browser.", QR payload `http://<apIP>/`,
  fields address = `<apIP>` (no `http://`, no `crosspoint.local`), network = ssid. Then the received section (5).

Step machine (in `SendState.h`): poll `WiFi.softAPgetStationNum()` once a second from `loop()`. JOIN → OPEN as soon
as the count is > 0, or as soon as any upload starts. OPEN → JOIN only after the count reads 0 on two polls in a row
and nothing has been received this session. Redraw only on a step change.

### 4. RUNNING (STA): ready (`SendReady.html`) / receiving (`SendReceiving.html`)

Header `send` / `wi-fi`. Headline "ready", text "On your phone, scan the code or type the address.", QR payload
`http://<ip>/`, fields: address = IP; network = SSID in Golos 24 (fit to the column) with 4 signal bars under it
(5 px wide, heights 6/10/14/18, 3 px gap, filled up to the bar count; reuse upstream `barsForRssi` with its
hysteresis). While Wi-Fi is down (the upstream disconnect counter > 0) the network value reads "reconnecting" and no
bars. After `WIFI_ABANDON_MS` → FAILED (reason wifi lost) instead of `onGoHome()`. Then the received section (5).

Both running screens: bottom tab stop (slot 0, back). No side-button tab switching here: leaving turns Wi-Fi off and
reboots, so only back does it. Back/home handling stays exactly as upstream (including during uploads).

### 5. Received section (both modes)

Label "received" left, the session count right (mono), 3 px ink rule, then rows of 64 px: title Golos 24 fitted, meta
mono (not uppercased) `<ext> · <size> mb` (size one decimal, e.g. `epub · 1.1 mb`), right side either a progress bar
(140 px, `drawProgressBar`, plus `48%`) for the file coming in now, or "done". The incoming file is the top row, then
completed files newest first. Keep at most 3 completed names in fixed `char[96]` buffers in `SendState.h` (no heap);
the count keeps going. Empty: Golos 18 "Nothing yet. Send to X4 and CrossPoint Sync work too."

Data source (read only, do not edit `src/network/`): `CrossPointWebServer::getWsUploadStatus()` (WebSocket uploads:
`inProgress`, `received`, `total`, `filename`, `lastCompleteName/Size/At`) and the public `upload` member for HTTP
multipart uploads (`fileName`, `size`, `success`; read `handleUpload` / `handleUploadPost`,
`src/network/CrossPointWebServer.cpp` ~1004-1240, to find a reliable "finished" signal). HTTP uploads have no total:
show "receiving" instead of the bar and percent. If you cannot find a reliable HTTP completion signal without editing
`src/network/`, track WebSocket uploads only and say so in the report. Do not list font or plugin uploads.

Uploads hold `loop()` inside `handleClient()`; the upload cancel-check callback runs once per chunk, so update
progress there. Before calling `requestUpdate()` from it, read `Activity.h` / the render task to confirm it is safe.
Redraw policy (decide in `SendState.h`): on upload start, on upload end, when progress crosses 25 / 50 / 75 %, on
signal-bar change, on Wi-Fi lost/recovered; never more often than once per 2 s while an upload is running.

### 6. FAILED (`SendFailed.html`)

Header `send` / `hotspot` or `wi-fi`. Headline "didn't start". Text by reason: hotspot or server did not start →
"The hotspot didn't turn on. Try again. If it keeps failing, restart the reader." (STA server failure: "The transfer
didn't start. Try again."); wifi lost → "No network for 5 minutes. Join again or use the hotspot." 32 px gap, 3 px rule,
then two 88 px rows (Golos 32, chevron): try again (selected by default) · other ways to send. Try again: hotspot →
AP_STARTING; STA → join a network flow. Other ways → CHOOSE. Back → CHOOSE. Bottom tabs back · select · ▲ · ▼.
Tear down the server, DNS and AP/STA before retrying so nothing leaks.

## Landscape (792x528) and all four orientations

Set the UI orientation the same way the setup activity does. Landscape layouts: menu and FAILED rows scroll by rows
so the selected row stays visible. Running screens: headline + text across the top, then QR at the left, and to the
right of the QR the fields and under them the received section (as many rows as fit; never overlap the bottom tabs).
Text is cut with an ellipsis, never overlapped.

## Strings

Add `STR_WREN_SEND_*` keys to `english.yaml`, `russian.yaml`, `ukrainian.yaml`, regenerate `I18nKeys.h`. Reuse
`STR_WREN_TAB_SEND` for the crumb and the existing tab keys. Exact strings (en / ru / uk):

| en | ru | uk |
|---|---|---|
| connect how? | как подключить? | як під'єднати? |
| join a network | подключиться к сети | під'єднатися до мережі |
| create hotspot | раздать сеть | роздати мережу |
| calibre wireless | calibre по wi-fi | calibre через wi-fi |
| usb drive | usb-накопитель | usb-накопичувач |
| 2.4 ghz networks only | только сети 2,4 ггц | лише мережі 2,4 ггц |
| hotspot | точка доступа | точка доступу |
| wi-fi | wi-fi | wi-fi |
| one moment | секунду | секунду |
| Turning on the hotspot. It takes a few seconds. | Включаю точку доступа. Это займёт несколько секунд. | Вмикаю точку доступу. Це займе кілька секунд. |
| join wren | подключите телефон | під'єднайте телефон |
| Point your phone camera at the code. | Наведите камеру телефона на код. | Наведіть камеру телефона на код. |
| network | сеть | мережа |
| password | пароль | пароль |
| none | нет | немає |
| next | дальше | далі |
| Once your phone joins, the address to open shows up here. | Когда телефон подключится, здесь появится адрес. | Коли телефон під'єднається, тут з'явиться адреса. |
| phone joined | телефон подключён | телефон під'єднано |
| Scan again to open Wren in the browser. | Отсканируйте ещё раз, чтобы открыть Wren в браузере. | Скануйте ще раз, щоб відкрити Wren у браузері. |
| address | адрес | адреса |
| ready | готово | готово |
| On your phone, scan the code or type the address. | На телефоне отсканируйте код или введите адрес. | На телефоні скануйте код або введіть адресу. |
| reconnecting | переподключение | перепідключення |
| received | получено | отримано |
| Nothing yet. Send to X4 and CrossPoint Sync work too. | Пока пусто. Send to X4 и CrossPoint Sync тоже работают. | Поки порожньо. Send to X4 і CrossPoint Sync теж працюють. |
| receiving | приём | приймання |
| done | есть | є |
| didn't start | не вышло | не вийшло |
| The hotspot didn't turn on. Try again. If it keeps failing, restart the reader. | Точка доступа не включилась. Попробуйте ещё раз. Если не выходит, перезагрузите ридер. | Точка доступу не ввімкнулася. Спробуйте ще раз. Якщо не виходить, перезавантажте рідер. |
| The transfer didn't start. Try again. | Передача не запустилась. Попробуйте ещё раз. | Передача не запустилася. Спробуйте ще раз. |
| No network for 5 minutes. Join again or use the hotspot. | Сети не было 5 минут. Подключитесь снова или раздайте сеть. | Мережі не було 5 хвилин. Під'єднайтеся знову або роздайте мережу. |
| try again | ещё раз | ще раз |
| other ways to send | другой способ | інший спосіб |
| stop | стоп | стоп |

## Host test (`SendStateTest.cpp`)

SSID from MAC; step machine (join → open on count, open → join only after two zero polls and nothing received, upload
forces open); received list (cap 3, newest first, count keeps going, long names truncated safely); percent and the
25/50/75 % redraw thresholds with the 2 s limit; menu rows with and without USB; landscape scroll window.

## Done

The files exist and compile in your judgement. Report: every file changed, the HTTP completion signal you used (or
that you track WebSocket only), where `requestUpdate()` is called from during uploads and why it is safe, other
`CrossPointWebServerActivity` call sites, and anything you could not do.
