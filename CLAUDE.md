# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository overview

Collection of Windows software. Each project lives in its own top-level folder.

| Folder | What it is |
| --- | --- |
| `openlab-checkin/` | OpenLab Augsburg – member management & check-in (C++, pure Win32, no third-party libraries). UI text is German. |

## openlab-checkin

### Commands

Build on Linux / MSYS2 with MinGW-w64 (statically linked single `.exe`):

```bash
cd openlab-checkin && ./build-mingw.sh
```

Output: `openlab-checkin/build/OpenLabCheckIn.exe`.

Build with Visual Studio 2022 / CMake (Windows):

```bash
cmake -S openlab-checkin -B openlab-checkin/build -G "Visual Studio 17 2022" -A x64
```

```bash
cmake --build openlab-checkin/build --config Release
```

There is no automated test suite or linter. The build must stay free of warnings (`-Wall -Wextra` / `/W4`). The app can be smoke-tested on Linux under Wine + Xvfb.

### Architecture

- `src/model.h`, `src/model.cpp` – data model (`Member`, `Visit`), `Store` (load/save), and `util` helpers (UTF-8 conversion, German date/time formatting, CSV export). File I/O uses the Win32 API (`CreateFileW`) so Unicode paths work under MinGW.
- `src/main.cpp` – the whole UI: one main window that paints its own header and status bar (`WM_PAINT`, double-buffered), plus a tab control with three pages (Check-in, Mitglieder, Verlauf & Statistik). Controls are children of the main window; each page's controls are listed in `gPage[n]` and shown or hidden in `showPage()`. All positions are set in `layout()` and DPI-scaled through `S()`.
- `src/app.rc` with `app.manifest` and `app.ico` – Common Controls v6, system DPI awareness, UTF-8 code page, version info. On MSVC the linker manifest is disabled (`/MANIFEST:NO`) because the manifest comes from the `.rc` file.

Data storage: `%APPDATA%\OpenLabCheckIn\data.tsv`, a UTF-8 tab-separated file with `M` (member) and `V` (visit) lines and backslash escaping. Saves are atomic (tmp file + `MoveFileExW`), and a daily backup goes to `backups\`. If a `portable.txt` file sits next to the `.exe`, data is stored in `data\` beside it instead. Times are stored as Unix `time_t`, and `checkOut == 0` means the person is still present.
