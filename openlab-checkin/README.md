# OpenLab Augsburg – Mitglieder & Check-in (C++)

Windows-App in C++ (reines Win32 + Common Controls, keine Fremdbibliotheken).
Ergebnis ist **eine einzelne, kleine `.exe`** ohne Installation und ohne Laufzeit-Abhängigkeiten (Windows 10/11).

## Funktionen

**Check-in**
- Karte scannen (USB-RFID-Leser im Tastatur-Modus) oder Name / Mitgliedsnummer eintippen + Enter
- Erneutes Scannen checkt automatisch aus
- Gäste per Name einchecken
- Live-Liste „Gerade im OpenLab“ mit Uhrzeit und Dauer; Auschecken per Doppelklick oder Button
- „Alle auschecken“ beim Abschließen
- Vergessene Check-outs vom Vortag werden beim nächsten Start automatisch geschlossen (Hinweis im Verlauf)
- Inaktive Mitglieder werden abgewiesen

**Admin-Zugang (Benutzername & Passwort)**
- Der Reiter „Check-in“ ist frei nutzbar (Kiosk-Betrieb)
- „Mitglieder“ und „Verlauf & Statistik“ sind erst nach Anmeldung erreichbar
- Beim ersten Öffnen eines geschützten Reiters wird der Admin-Zugang eingerichtet (Benutzername + Passwort, mind. 6 Zeichen)
- „Abmelden“ sperrt den Bereich sofort, nach 10 Minuten ohne Eingabe wird automatisch gesperrt
- „Zugang ändern“ setzt Benutzername / Passwort neu (aktuelles Passwort erforderlich)
- Gespeichert wird nur ein Hash (PBKDF2-HMAC-SHA256, 200 000 Runden, zufälliges Salt), nie das Passwort selbst

**Mitglieder**
- Anlegen, bearbeiten, verwerfen, löschen, Suche, Filter für inaktive Mitglieder
- Felder: Mitgliedsnr. (automatisch vorgeschlagen), Vor-/Nachname, E-Mail, Telefon, Mitgliedsart,
  Eintrittsdatum, RFID-Kartennummer, Notizen (z. B. Einweisungen), aktiv/inaktiv
- Prüfung auf doppelte Mitglieds- und Kartennummern
- Mitglied direkt aus dem Formular ein-/auschecken
- CSV-Export

**Verlauf & Statistik**
- Zeitraum frei wählbar (Schnellwahl Heute / Monat / Jahr), Namenssuche
- Besuche, verschiedene Personen, Gastbesuche, Gesamtstunden
- CSV-Export (Semikolon, UTF-8 mit BOM – öffnet direkt korrekt in deutschem Excel)

## Daten

- Datei: `%APPDATA%\OpenLabCheckIn\data.tsv` (UTF-8, tabulatorgetrennt, mit jedem Texteditor lesbar)
- Einmal täglich automatische Sicherung nach `%APPDATA%\OpenLabCheckIn\backups\`
- Speichern erfolgt atomar (erst temporäre Datei, dann ersetzen) – kein Datenverlust bei Absturz
- **Passwort vergessen?** In `data.tsv` die Zeile, die mit `A` beginnt, löschen. Beim nächsten Öffnen von
  „Mitglieder“ wird der Admin-Zugang neu eingerichtet.
- Hinweis: Das Passwort schützt die Oberfläche. Die Datendatei selbst ist nicht verschlüsselt – wer Zugriff auf das
  Windows-Benutzerkonto hat, kann sie lesen. Für den Kiosk-PC daher ein eigenes Windows-Konto verwenden.
- **Portabler Modus:** leere Datei `portable.txt` neben die `.exe` legen → Daten liegen in `data\` neben der `.exe`

## Bauen

### Visual Studio 2022 (Windows)
„Ordner öffnen“ → Projektordner wählen → CMake wird erkannt → Ziel `OpenLabCheckIn.exe` bauen.
Oder auf der Kommandozeile (Developer PowerShell):

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
```

```bash
cmake --build build --config Release
```

### MinGW-w64 (Linux oder MSYS2)

```bash
./build-mingw.sh
```

Ergebnis: `build/OpenLabCheckIn.exe` (statisch gelinkt).

## Projektstruktur

```
src/model.h / model.cpp   Datenmodell (Member, Visit), Speicherung, Datums-/Text-Helfer, CSV
src/main.cpp              Oberfläche: Fenster, Reiter, Listen, Formular, Check-in-Logik
src/app.rc                Icon, Manifest (moderne Steuerelemente, DPI), Versionsinfo, Admin-Dialoge
src/resource.h            IDs der Dialoge
src/app.manifest
src/app.ico
CMakeLists.txt            Build für Visual Studio / CMake
build-mingw.sh            Build mit MinGW-w64
```
