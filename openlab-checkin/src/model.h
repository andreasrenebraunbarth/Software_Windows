// OpenLab Augsburg – Mitglieder & Check-in
// Datenmodell, Speicherung und Hilfsfunktionen
#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

struct Member {
    int64_t id = 0;
    std::wstring number;     // Mitgliedsnummer
    std::wstring first;      // Vorname
    std::wstring last;       // Nachname
    std::wstring email;
    std::wstring phone;
    std::wstring card;       // RFID-/Kartennummer
    std::wstring type = L"Vollmitglied";
    std::wstring notes;
    bool active = true;
    time_t joinDate = 0;

    std::wstring fullName() const;
};

struct Visit {
    int64_t id = 0;
    int64_t memberId = 0;    // 0 = Gast
    std::wstring name;       // Name zum Zeitpunkt des Besuchs
    bool guest = false;
    time_t checkIn = 0;
    time_t checkOut = 0;     // 0 = noch anwesend
    bool autoOut = false;    // beim Programmstart automatisch ausgecheckt

    bool present() const { return checkOut == 0; }
    long long seconds(time_t now) const { return (long long)((present() ? now : checkOut) - checkIn); }
};

// Admin-Zugang für die Reiter "Mitglieder" und "Verlauf & Statistik".
// Gespeichert wird nur ein Hash (PBKDF2-HMAC-SHA256 mit zufälligem Salt), nie das Passwort selbst.
struct AdminAccount {
    std::wstring user;
    std::wstring salt;        // hex
    std::wstring hash;        // hex
    unsigned iterations = 0;

    bool configured() const { return !user.empty() && !hash.empty(); }
};

extern const wchar_t* const kMembershipTypes[];
extern const int kMembershipTypeCount;

// Speichert alle Daten in %APPDATA%\OpenLabCheckIn\data.tsv
// (oder im Unterordner "data" neben der .exe, wenn dort eine Datei "portable.txt" liegt).
// Beim Laden wird einmal pro Tag eine Sicherung unter backups\ angelegt.
class Store {
public:
    std::vector<Member> members;
    std::vector<Visit> visits;
    AdminAccount admin;
    std::wstring dir;
    std::wstring file;

    Store();
    bool load(std::wstring& error);
    bool save(std::wstring& error) const;

    int64_t newId() { return nextId_++; }
    Member* findMember(int64_t id);
    Visit* findVisit(int64_t id);
    Visit* openVisitFor(int64_t memberId);

    // Setzt Benutzername und Passwort neu (speichert noch nicht). false, wenn der Hash nicht erzeugt werden kann.
    bool setAdmin(const std::wstring& user, const std::wstring& password);
    // Prüft Benutzername (ohne Groß-/Kleinschreibung) und Passwort.
    bool checkAdmin(const std::wstring& user, const std::wstring& password) const;

private:
    int64_t nextId_ = 1;
};

namespace util {
std::string toUtf8(const std::wstring& w);
std::wstring fromUtf8(const std::string& s);
std::wstring trim(const std::wstring& s);
std::wstring lower(const std::wstring& s);
bool equalsCI(const std::wstring& a, const std::wstring& b);
bool containsCI(const std::wstring& haystack, const std::wstring& needle);

time_t startOfDay(time_t t);
time_t today();
std::wstring fmtTime(time_t t);           // 14:05
std::wstring fmtDate(time_t t);           // 06.11.2026
std::wstring fmtDateWeekday(time_t t);    // Fr 06.11.2026
std::wstring fmtClock(time_t t);          // Freitag, 06. November 2026 · 14:05:33
std::wstring fmtDuration(long long sec);  // 2:05 h
std::wstring fmtDecimal(double v, int decimals); // 3,50

bool writeCsv(const std::wstring& path, const std::vector<std::wstring>& header,
              const std::vector<std::vector<std::wstring>>& rows);
}
