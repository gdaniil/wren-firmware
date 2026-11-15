# Setup tab: status

Source: design canvas "Wren design", page "Wren setup" (boards Setup, Setup · display, Setup · system, Files,
Delete file). Copies: `docs/ours/design/setup/`. Briefs: `briefs/S1_SETUP.md`, `briefs/S2_FILES.md`. 14 Nov 2026.

## Built

- `src/wren/setup/`: `WrenSetupActivity` (reader · display · system · files), `WrenSetupDisplayActivity`,
  `WrenSetupSystemActivity`, `WrenFilesActivity`, `SetupState.h`, `FilesState.h`, `SetupUi.h`.
- `src/wren/ui/WrenDeleteDialog`: used by Files and by the library book menu.
- `WrenChrome`: `Marker::Toggle`, `drawTabHeader` (shared with home). Icons Folder, BookFile, ImageFile.
- `ActivityManager::goToSettings()` without a section opens Wren setup; with a section, upstream settings.
- `src/wren/WrenVersion.h`: `0.1.0`.

## Decisions

- Removed from the device UI (not in the design): controls, status bar, refresh frequency, theme, night mode,
  hidden files, file extensions, recents options, KOReader, OPDS, Bluetooth, clock, plugins, firmware update, about.
  Values stay stored and editable in the web settings page (API unchanged).
- sleep screen: cover · dark (the Wren sleep screen is not built). battery %: show = never hide, hide = always.
- sleep after: 5 · 10 · 30 min. language: english · русский · українська.
- Files: folders not deletable on the device.

## Checks (14 Nov)

`pio run -e default|x4c|x4pro` pass, `clang-format-fix -g` clean, `pio check` clean, unit tests 980/980.
RAM static 66,696 B. Hardware not tested.
