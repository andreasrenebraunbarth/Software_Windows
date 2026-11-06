# build-mingw.sh – Windows-Build mit MinGW-w64

`build-mingw.sh` baut die OpenLab-Check-in-App als **eine einzelne, statisch gelinkte `OpenLabCheckIn.exe`** (64 Bit, Windows 10/11).
Es funktioniert unter **Linux** (Cross-Compiling) und unter **Windows mit MSYS2**.

## Kurzfassung

```bash
cd openlab-checkin
```

```bash
./build-mingw.sh
```

Ergebnis: `openlab-checkin/build/OpenLabCheckIn.exe` (ca. 360 KB). Die Datei kann ohne Installation direkt auf einen Windows-Rechner kopiert und gestartet werden.

## Voraussetzungen

### Linux (Debian / Ubuntu)

```bash
sudo apt-get install mingw-w64
```

Damit sind Compiler (`x86_64-w64-mingw32-g++`) und Ressourcen-Compiler (`x86_64-w64-mingw32-windres`) installiert.

### Windows mit MSYS2

1. MSYS2 installieren: https://www.msys2.org
2. Die Shell **„MSYS2 MINGW64“** öffnen (nicht „MSYS2 MSYS“).
3. Toolchain installieren:

```bash
pacman -S --needed mingw-w64-x86_64-gcc
```

4. Falls die Programme mit dem Präfix `x86_64-w64-mingw32-` nicht gefunden werden, die Namen beim Aufruf angeben (siehe [Eigene Compiler-Namen](#eigene-compiler-namen)):

```bash
CXX=g++ WINDRES=windres ./build-mingw.sh
```

## Was das Skript macht

| Schritt | Befehl | Ergebnis |
| --- | --- | --- |
| 1 | wechselt in den Ordner, in dem das Skript liegt | Aufruf von überall möglich, z. B. `openlab-checkin/build-mingw.sh` |
| 2 | wählt den Compiler (siehe unten) | – |
| 3 | legt `build/` an | – |
| 4 | `windres` übersetzt `src/app.rc` (Icon, Manifest, Versionsinfo) | `build/app.res` |
| 5 | `g++` kompiliert `src/main.cpp` und `src/model.cpp` und linkt alles | `build/OpenLabCheckIn.exe` |

Das Skript bricht beim ersten Fehler ab (`set -euo pipefail`). Am Ende steht `Fertig: build/OpenLabCheckIn.exe` nur, wenn alles geklappt hat.

### Compiler-Optionen

| Option | Bedeutung |
| --- | --- |
| `-std=c++17 -O2` | C++17, optimiert |
| `-Wall -Wextra` | alle üblichen Warnungen – **der Build muss warnungsfrei bleiben** |
| `-DUNICODE -D_UNICODE` | Win32-Funktionen in der Unicode-Variante (Umlaute in Namen und Pfaden) |
| `-DNOMINMAX` | verhindert, dass `windows.h` `min`/`max` als Makro definiert |
| `-municode` | Einstiegspunkt `wWinMain` (Unicode) |
| `-mwindows` | GUI-Programm, es öffnet sich kein Konsolenfenster |
| `-static` | C++-Laufzeit wird eingebaut, es werden keine MinGW-DLLs benötigt |
| `-s` | Debug-Symbole entfernen (kleinere Datei) |
| `-lcomctl32 -lcomdlg32` | Common Controls (Listen, Reiter, Datumsfelder) und Speichern-Dialog |

## Eigene Compiler-Namen

Über Umgebungsvariablen lassen sich andere Programme verwenden:

| Variable | Standard |
| --- | --- |
| `CXX` | `x86_64-w64-mingw32-g++-posix`, ersatzweise `x86_64-w64-mingw32-g++` |
| `WINDRES` | `x86_64-w64-mingw32-windres` |

Beispiel:

```bash
CXX=x86_64-w64-mingw32-g++-win32 ./build-mingw.sh
```

> **Achtung:** Gibt es den Compiler aus `CXX` nicht, nimmt das Skript **ohne Meldung** `x86_64-w64-mingw32-g++`. Ein Tippfehler in `CXX` fällt also nicht auf, wenn dieser Compiler installiert ist. Für `WINDRES` gibt es keinen solchen Ersatz: Ein falscher Name führt zu `command not found` und zum Abbruch.

## Ausgabe

```
openlab-checkin/build/
├── app.res              Zwischendatei (Ressourcen), kann gelöscht werden
└── OpenLabCheckIn.exe   die fertige App
```

`build/` steht in `.gitignore` und wird nicht eingecheckt. Ein neuer Aufruf überschreibt die alten Dateien. Für einen sauberen Neubau:

```bash
rm -rf build && ./build-mingw.sh
```

## Testen ohne Windows

Unter Linux lässt sich die `.exe` mit Wine starten, ohne Monitor z. B. mit Xvfb:

```bash
sudo apt-get install wine64 xvfb
```

```bash
xvfb-run wine build/OpenLabCheckIn.exe
```

Tipp: Eine leere Datei `portable.txt` neben die `.exe` legen. Dann landen die Testdaten in `build/data/` und nicht im Wine-Profil.

## Häufige Fehler

| Meldung | Ursache / Lösung |
| --- | --- |
| `x86_64-w64-mingw32-windres: command not found` | MinGW-w64 ist nicht installiert (`apt-get install mingw-w64`) oder heißt anders → `WINDRES=windres` setzen |
| `undefined reference to 'WinMain'` | `-municode` fehlt – nur möglich, wenn das Skript verändert wurde |
| `cannot find -lcomctl32` | unvollständige MinGW-Installation, Paket neu installieren |
| `Permission denied` beim Start von `./build-mingw.sh` | Ausführrecht fehlt → `chmod +x build-mingw.sh` oder `bash build-mingw.sh` |
| Fehler beim Kopieren / Überschreiben der `.exe` unter Windows | Die App läuft noch – vorher schließen |

## Alternative: Visual Studio

Wer unter Windows lieber Visual Studio 2022 nutzt, braucht das Skript nicht. Der Build läuft dann über `CMakeLists.txt` (siehe [README](../README.md#bauen)).
