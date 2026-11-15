# Send tab: status

Source: design canvas "Wren design", page "Wren send" (Send · connect, Hotspot · join, Hotspot · open, Send · ready,
Send · receiving, Send · starting, Send · failed). Copies: `docs/ours/design/send/`. Brief: `briefs/N1_SEND.md`.
14 Nov 2026.

## Built

- `src/wren/send/WrenSendActivity`: replaces `CrossPointWebServerActivity` for `goToFileTransfer()` and
  `goToJoinNetwork()`. Same lifecycle (heap-defrag reboot before join, captive DNS, mDNS `crosspoint`, reboot on
  exit). The upstream activity stays in the tree, unused.
- Hotspot: open network `wren-xxxx` (soft-AP MAC), two steps, one QR at a time. Step switches when a phone joins.
- Addresses on screen are IPs. `crosspoint.local` still answers for the apps. HTTP API unchanged.
- Received list: WebSocket uploads (built-in web page) with live percent; HTTP uploads (apps) appear when finished.
- `src/wren/send/WrenWifiActivity` (N2): Wren network list, searching, connecting (auto and manual), save
  password, failed (by cause), forget (hold select on a saved network), no networks, hidden network. Mirrors
  `WifiSelectionActivity`. Codex ran out of limit mid-task; Claude fixed the build and the front-button mapping.
- `src/wren/ui/WrenKeyboardActivity` (N3): Wren keyboard for password and network name, show-password switch,
  language key, touch hit rects. Mirrors `KeyboardEntryActivity`.
- Still upstream: Calibre screen.

## Known limits

- HTTP size is the size before the last chunk (off by at most one chunk).
- A failed overwrite of an existing file may still be listed as received.
- Two WebSocket uploads finishing within one 250 ms poll: only the last is listed.
- OPDS is no longer reachable from the send menu (not in the design).

## Checks (14 Nov)

`pio run -e default|x4c|x4pro` pass, `clang-format` clean, `pio check` clean, unit tests 984/984.
RAM static 66,640 B (default). Hardware not tested.

After N2/N3: all three envs pass, `pio check` clean, unit tests 992/992, RAM static 66,656 B. Hardware not tested.
Keyboard: typing allocates (std::string, as upstream); Hebrew/Arabic layouts have no glyphs in the Wren fonts.
