# N2: Wren Wi-Fi network picker

Repo: `/Users/daniilhurevych/Wren/firmware`, branch `ours`. Lots of uncommitted Wren work in the tree (setup,
library, send): ours, keep it. C++ (gnu++2a), PlatformIO + Arduino, ESP32-C3 baseline (~380 KB RAM, >50 KB free).
Another Codex task writes `src/wren/ui/WrenKeyboardActivity.*` and `src/wren/ui/KeyboardState.h` at the same time:
do not touch those files; code against the API below.

Design (geometry and words), 528x792 portrait, 1-bit, in `docs/ours/design/send/`: `WifiNetworks.html` (list),
`WifiScanning.html`, `WifiConnecting.html`, `WifiSave.html`, `WifiFailed.html`, `WifiForget.html`, `WifiNone.html`.
Ignore HTML fonts; Wren fonts: headline Golos 40, list title Golos 24, list subtitle Golos 18, labels Mono 16, action
rows Golos 32. Patterns: `src/wren/send/WrenSendActivity.cpp` (same headers, headline, action rows 88 px with
chevron, bottom tabs, landscape scrolling, `wren::setup::status`/`clipContent`/`footer`), `src/wren/ui/WrenChrome`.

Engine to mirror: `src/activities/network/WifiSelectionActivity.{h,cpp}` (upstream, 1112 lines; read all). Keep every
behaviour: credential store load under `RenderLock`, auto-connect (last SSID first, then saved networks seen in a scan,
by signal; select interrupts it and shows the list), scan with `WIFI_ALL_CHANNEL_SCAN`, persistent(false), hostname,
timeouts (15 s / 7 s auto), NTP clock sync on first join, `trustedtime::startSync()`, last-connected SSID,
`pluginevents::drain`, save prompt only for a newly typed password, forget for saved networks, hidden network
(SSID entry then password), `onExit` deletes the scan and leaves Wi-Fi up, result `WifiResult{ssid, ip}` exactly as
upstream. Comment at the top: "Mirrors WifiSelectionActivity @ 622e5de9; keep in sync on upstream merges." Only the
drawing, words and input mapping change. Drop the MAC address line.

## Constraints (hard)

No flashing, no `erase_flash`/`write_flash`, no commits, no push. Do not edit `src/network/`, `freeink-sdk/`,
`lib/hal/`, `src/activities/network/*` (upstream stays as is). Generated: only `lib/I18n/I18nKeys.h` via
`scripts/gen_i18n.py`. New code: `src/wren/send/WrenWifiActivity.{h,cpp}`, pure logic in
`src/wren/send/WifiState.h`, host test `test/wren_layout/WifiStateTest.cpp` (do not edit
`test/wren_layout/CMakeLists.txt`; I register it). One upstream-style edit allowed:
`src/wren/send/WrenSendActivity.cpp` `joinNetwork()` starts `WrenWifiActivity` instead of `WifiSelectionActivity`.
Do not run builds or tests; I run them. Networks list: keep upstream's `std::vector` (scan size is bounded by the
driver); no other heap in render or per-loop paths.

## Keyboard API (built by the other task)

```cpp
#include "wren/ui/WrenKeyboardActivity.h"
WrenKeyboardActivity(GfxRenderer&, MappedInputManager&, std::string crumb, std::string title, std::string fieldLabel,
                     std::string initialText = "", size_t maxLength = 0, InputType inputType = InputType::Text);
```

Result `KeyboardResult{text}` or cancelled. Password: crumb `wi-fi`, title = SSID, label "password",
`InputType::Password`, max 64. Hidden network name: crumb `wi-fi`, title "hidden network", label "network name",
`InputType::Text`, max 32.

## States and screens

Every screen: status bar, `drawHeader` with crumb/title as in the design.

| state | design | content |
|---|---|---|
| SCANNING | `WifiScanning` | crumb send / title wi-fi; headline "searching", text "Looking for networks nearby. Takes a few seconds."; tabs: back |
| AUTO_CONNECTING | `WifiConnecting` | crumb wi-fi / title SSID; "connecting", "Joining <ssid>, the network you used last. Select to pick another."; tabs back · select |
| CONNECTING | same board | text "Joining <ssid>."; tabs back |
| NETWORK_LIST | `WifiNetworks` | crumb send / title wi-fi. Rows 72 px, 20 px sides, 2 px dividers: title Golos 24 (SSID fitted), subtitle Golos 18: "saved · connect" for saved, "secured", "open"; signal: 4 bars right (5 px wide, heights 6/10/14/18, 3 px gap, filled up to the count from RSSI, same thresholds as upstream's indicator). Then row "scan again" with the refresh icon if one exists in `WrenIcons` (else text only) and row "hidden network" with a chevron. Selected row inverted. ▲▼ (front) move with wrap and hold-repeat, select joins, hold select on a saved network → FORGET. Rows scroll by row when they do not fit. Tabs back · select · ▲ · ▼, `drawEdgeTabs(true, true)`. |
| (none found) | `WifiNone` | when a scan finds no networks: "no networks", "Only 2.4 GHz networks show up. Move closer to the router, or go back and use the hotspot.", rows scan again (default) · hidden network |
| SAVE_PROMPT | `WifiSave` | "connected", "Save the password? Next time Wren joins on its own.", rows save (default) · don't save |
| CONNECTION_FAILED | `WifiFailed` | "didn't connect"; text by cause: wrong password / general failure "Check the password and try again.", `WL_NO_SSID_AVAIL` "The network is out of reach. Move closer and try again.", timeout "The network didn't answer. Try again."; rows try again (default: reopens the password keyboard for a secured network, reconnects for an open one) · other network (back to the list) |
| FORGET_PROMPT | `WifiForget` | "forget network?", "Wren will ask for the <ssid> password again.", rows keep (default) · forget. Forget removes the credential (as upstream) and returns to the list. |

Back: list → cancel (parent shows the send menu); prompts → list; scanning → cancel. Landscape 792x528: same stacks,
lists scroll, text cut with an ellipsis, nothing overlaps the bottom tabs. All four orientations.

## Strings

Add `STR_WREN_WIFI_*` (en / ru / uk) and regenerate `I18nKeys.h`:

| en | ru | uk |
|---|---|---|
| searching | поиск | пошук |
| Looking for networks nearby. Takes a few seconds. | Ищу сети рядом. Это займёт несколько секунд. | Шукаю мережі поруч. Це займе кілька секунд. |
| connecting | подключение | під'єднання |
| Joining %s, the network you used last. Select to pick another. | Подключаюсь к %s, последней сети. Нажмите «выбрать», чтобы взять другую. | Під'єднуюся до %s, останньої мережі. Натисніть «вибрати», щоб узяти іншу. |
| Joining %s. | Подключаюсь к %s. | Під'єднуюся до %s. |
| saved · connect | сохранена · подключить | збережена · під'єднати |
| secured | с паролем | з паролем |
| open | открытая | відкрита |
| scan again | искать снова | шукати знову |
| hidden network | скрытая сеть | прихована мережа |
| network name | имя сети | назва мережі |
| no networks | сетей нет | мереж немає |
| Only 2.4 GHz networks show up. Move closer to the router, or go back and use the hotspot. | Видны только сети 2,4 ГГц. Подойдите ближе к роутеру или вернитесь и раздайте сеть. | Видно лише мережі 2,4 ГГц. Підійдіть ближче до роутера або поверніться й роздайте мережу. |
| connected | подключено | під'єднано |
| Save the password? Next time Wren joins on its own. | Сохранить пароль? В следующий раз Wren подключится сам. | Зберегти пароль? Наступного разу Wren під'єднається сам. |
| save | сохранить | зберегти |
| don't save | не сохранять | не зберігати |
| didn't connect | не подключилось | не під'єдналося |
| Check the password and try again. | Проверьте пароль и попробуйте ещё раз. | Перевірте пароль і спробуйте ще раз. |
| The network is out of reach. Move closer and try again. | Сеть не отвечает. Подойдите ближе и попробуйте ещё раз. | Мережа не відповідає. Підійдіть ближче й спробуйте ще раз. |
| The network didn't answer. Try again. | Сеть не ответила. Попробуйте ещё раз. | Мережа не відповіла. Спробуйте ще раз. |
| other network | другая сеть | інша мережа |
| forget network? | забыть сеть? | забути мережу? |
| Wren will ask for the %s password again. | Wren снова спросит пароль от %s. | Wren знову запитає пароль від %s. |
| keep | оставить | залишити |
| forget | забыть | забути |

Also add the keyboard task's keys (it does not edit yaml): `STR_WREN_KB_SHOW_PASSWORD` show password / показать
пароль / показати пароль; `STR_WREN_KB_DEL` del / стер / стер; `STR_WREN_KB_SHIFT` shift / шифт / шифт;
`STR_WREN_KB_SPACE` space / пробел / пробіл; `STR_WREN_KB_OK` ok / ок / ок; `STR_WREN_KB_ABC` abc / абв / абв.

Reuse `STR_WREN_SEND_TRY_AGAIN`, `STR_WREN_TAB_SEND`, `STR_WREN_SEND_WIFI`, `STR_WREN_SEND_PASSWORD`. Format strings
use `snprintf` into a fixed buffer.

## Host test (`WifiStateTest.cpp`)

Signal bars from RSSI, subtitle choice (saved / secured / open), list row model (networks + scan again + hidden, with
and without networks → none-found screen), selection wrap and scroll window, failure text by cause, default row of
each prompt (keep / save / try again / scan again).

## Done

Files exist and compile in your judgement. Report every file changed, how hold-select is detected, and anything not done.
