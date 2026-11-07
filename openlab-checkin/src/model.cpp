#ifndef UNICODE
#define UNICODE
#endif
#include "model.h"

#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <cwchar>

const wchar_t* const kMembershipTypes[] = {
    L"Vollmitglied", L"Ermäßigt", L"Familie", L"Fördermitglied", L"Kind / Jugend", L"Ehrenmitglied"};
const int kMembershipTypeCount = sizeof(kMembershipTypes) / sizeof(kMembershipTypes[0]);

std::wstring Member::fullName() const {
    if (first.empty()) return last;
    if (last.empty()) return first;
    return first + L" " + last;
}

// ------------------------------------------------------------------ Datei-Helfer (Win32, Unicode-Pfade)

static bool writeFileBytes(const std::wstring& path, const std::string& data) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    BOOL ok = WriteFile(h, data.data(), (DWORD)data.size(), &written, nullptr) && written == data.size();
    ok = FlushFileBuffers(h) && ok;
    CloseHandle(h);
    return ok;
}

static bool readFileBytes(const std::wstring& path, std::string& data) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    GetFileSizeEx(h, &size);
    data.assign((size_t)size.QuadPart, '\0');
    DWORD read = 0;
    BOOL ok = data.empty() || (ReadFile(h, &data[0], (DWORD)data.size(), &read, nullptr) && read == data.size());
    CloseHandle(h);
    return ok;
}

// ------------------------------------------------------------------ util

namespace util {

std::string toUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

std::wstring fromUtf8(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

std::wstring trim(const std::wstring& s) {
    const wchar_t* ws = L" \t\r\n";
    auto b = s.find_first_not_of(ws);
    if (b == std::wstring::npos) return {};
    auto e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

std::wstring lower(const std::wstring& s) {
    std::wstring l = s;
    if (!l.empty()) CharLowerBuffW(&l[0], (DWORD)l.size());
    return l;
}

bool equalsCI(const std::wstring& a, const std::wstring& b) {
    return CompareStringOrdinal(a.c_str(), (int)a.size(), b.c_str(), (int)b.size(), TRUE) == CSTR_EQUAL;
}

bool containsCI(const std::wstring& haystack, const std::wstring& needle) {
    if (needle.empty()) return true;
    return lower(haystack).find(lower(needle)) != std::wstring::npos;
}

static tm localTm(time_t t) {
    tm r{};
    localtime_s(&r, &t);
    return r;
}

time_t startOfDay(time_t t) {
    tm r = localTm(t);
    r.tm_hour = r.tm_min = r.tm_sec = 0;
    r.tm_isdst = -1;
    return mktime(&r);
}

time_t today() { return startOfDay(time(nullptr)); }

std::wstring fmtTime(time_t t) {
    tm r = localTm(t);
    wchar_t b[16];
    swprintf(b, 16, L"%02d:%02d", r.tm_hour, r.tm_min);
    return b;
}

std::wstring fmtDate(time_t t) {
    tm r = localTm(t);
    wchar_t b[16];
    swprintf(b, 16, L"%02d.%02d.%04d", r.tm_mday, r.tm_mon + 1, r.tm_year + 1900);
    return b;
}

static const wchar_t* kDays[] = {L"Sonntag", L"Montag", L"Dienstag", L"Mittwoch", L"Donnerstag", L"Freitag", L"Samstag"};
static const wchar_t* kMonths[] = {L"Januar", L"Februar", L"März", L"April", L"Mai", L"Juni",
                                   L"Juli", L"August", L"September", L"Oktober", L"November", L"Dezember"};

std::wstring fmtDateWeekday(time_t t) {
    tm r = localTm(t);
    return std::wstring(kDays[r.tm_wday]).substr(0, 2) + L" " + fmtDate(t);
}

std::wstring fmtClock(time_t t) {
    tm r = localTm(t);
    wchar_t b[96];
    swprintf(b, 96, L"%ls, %02d. %ls %04d  \x00B7  %02d:%02d:%02d", kDays[r.tm_wday], r.tm_mday,
             kMonths[r.tm_mon], r.tm_year + 1900, r.tm_hour, r.tm_min, r.tm_sec);
    return b;
}

std::wstring fmtDuration(long long sec) {
    if (sec < 0) sec = 0;
    wchar_t b[32];
    swprintf(b, 32, L"%lld:%02lld h", sec / 3600, (sec / 60) % 60);
    return b;
}

std::wstring fmtDecimal(double v, int decimals) {
    wchar_t b[64];
    swprintf(b, 64, L"%.*f", decimals, v);
    std::wstring s = b;
    std::replace(s.begin(), s.end(), L'.', L',');
    return s;
}

static std::wstring csvEscape(const std::wstring& v) {
    if (v.find_first_of(L";\"\r\n") == std::wstring::npos) return v;
    std::wstring r = L"\"";
    for (wchar_t c : v) {
        if (c == L'"') r += L'"';
        r += c;
    }
    return r + L"\"";
}

bool writeCsv(const std::wstring& path, const std::vector<std::wstring>& header,
              const std::vector<std::vector<std::wstring>>& rows) {
    std::wstring out;
    auto line = [&](const std::vector<std::wstring>& cols) {
        for (size_t i = 0; i < cols.size(); ++i) {
            if (i) out += L';';
            out += csvEscape(cols[i]);
        }
        out += L"\r\n";
    };
    line(header);
    for (auto& r : rows) line(r);

    return writeFileBytes(path, "\xEF\xBB\xBF" + toUtf8(out));  // UTF-8 mit BOM für Excel
}

}  // namespace util

// ------------------------------------------------------------------ Store

static std::wstring envVar(const wchar_t* name) {
    wchar_t buf[MAX_PATH * 2];
    DWORD n = GetEnvironmentVariableW(name, buf, MAX_PATH * 2);
    return (n > 0 && n < MAX_PATH * 2) ? std::wstring(buf, n) : std::wstring();
}

static bool fileExists(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

Store::Store() {
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring exeDir = exe;
    exeDir = exeDir.substr(0, exeDir.find_last_of(L"\\/"));

    if (fileExists(exeDir + L"\\portable.txt"))
        dir = exeDir + L"\\data";
    else
        dir = envVar(L"APPDATA") + L"\\OpenLabCheckIn";
    CreateDirectoryW(dir.c_str(), nullptr);
    file = dir + L"\\data.tsv";
}

static std::wstring esc(const std::wstring& s) {
    std::wstring r;
    for (wchar_t c : s) {
        switch (c) {
            case L'\\': r += L"\\\\"; break;
            case L'\t': r += L"\\t"; break;
            case L'\n': r += L"\\n"; break;
            case L'\r': r += L"\\r"; break;
            default: r += c;
        }
    }
    return r;
}

static std::wstring unesc(const std::wstring& s) {
    std::wstring r;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'\\' && i + 1 < s.size()) {
            wchar_t n = s[++i];
            r += n == L't' ? L'\t' : n == L'n' ? L'\n' : n == L'r' ? L'\r' : n;
        } else {
            r += s[i];
        }
    }
    return r;
}

static std::vector<std::wstring> splitTabs(const std::wstring& line) {
    std::vector<std::wstring> out;
    size_t start = 0;
    for (;;) {
        size_t p = line.find(L'\t', start);
        out.push_back(unesc(line.substr(start, p == std::wstring::npos ? std::wstring::npos : p - start)));
        if (p == std::wstring::npos) break;
        start = p + 1;
    }
    return out;
}

static long long num(const std::wstring& s) { return std::wcstoll(s.c_str(), nullptr, 10); }

bool Store::load(std::wstring& error) {
    members.clear();
    visits.clear();
    admin = AdminAccount();
    if (!fileExists(file)) return true;

    // tägliche Sicherung
    std::wstring backupDir = dir + L"\\backups";
    CreateDirectoryW(backupDir.c_str(), nullptr);
    time_t t = time(nullptr);
    tm r{};
    localtime_s(&r, &t);
    wchar_t name[64];
    swprintf(name, 64, L"\\data-%04d-%02d-%02d.tsv", r.tm_year + 1900, r.tm_mon + 1, r.tm_mday);
    CopyFileW(file.c_str(), (backupDir + name).c_str(), TRUE);

    std::string bytes;
    if (!readFileBytes(file, bytes)) {
        error = L"Datei kann nicht gelesen werden.";
        return false;
    }
    if (bytes.size() >= 3 && bytes.compare(0, 3, "\xEF\xBB\xBF") == 0) bytes.erase(0, 3);
    std::wstring text = util::fromUtf8(bytes);

    int64_t maxId = 0;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t nl = text.find(L'\n', pos);
        std::wstring line = text.substr(pos, nl == std::wstring::npos ? std::wstring::npos : nl - pos);
        pos = nl == std::wstring::npos ? text.size() : nl + 1;
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        if (line.empty() || line[0] == L'#') continue;

        auto c = splitTabs(line);
        if (c[0] == L"M" && c.size() >= 12) {
            Member m;
            m.id = num(c[1]);
            m.number = c[2];
            m.first = c[3];
            m.last = c[4];
            m.email = c[5];
            m.phone = c[6];
            m.card = c[7];
            m.type = c[8];
            m.notes = c[9];
            m.active = c[10] == L"1";
            m.joinDate = (time_t)num(c[11]);
            maxId = std::max(maxId, m.id);
            members.push_back(m);
        } else if (c[0] == L"A" && c.size() >= 5) {
            admin.user = c[1];
            admin.salt = c[2];
            admin.iterations = (unsigned)num(c[3]);
            admin.hash = c[4];
        } else if (c[0] == L"V" && c.size() >= 8) {
            Visit v;
            v.id = num(c[1]);
            v.memberId = num(c[2]);
            v.name = c[3];
            v.guest = c[4] == L"1";
            v.checkIn = (time_t)num(c[5]);
            v.checkOut = (time_t)num(c[6]);
            v.autoOut = c[7] == L"1";
            maxId = std::max(maxId, v.id);
            visits.push_back(v);
        }
    }
    nextId_ = maxId + 1;
    return true;
}

bool Store::save(std::wstring& error) const {
    std::wstring out = L"# OpenLab Augsburg Check-in – Datendatei (UTF-8, Tab-getrennt)\n";
    auto s = [](long long v) { return std::to_wstring(v); };
    if (admin.configured())
        out += L"A\t" + esc(admin.user) + L"\t" + admin.salt + L"\t" + s(admin.iterations) + L"\t" + admin.hash + L"\n";
    for (auto& m : members) {
        out += L"M\t" + s(m.id) + L"\t" + esc(m.number) + L"\t" + esc(m.first) + L"\t" + esc(m.last) + L"\t" +
               esc(m.email) + L"\t" + esc(m.phone) + L"\t" + esc(m.card) + L"\t" + esc(m.type) + L"\t" +
               esc(m.notes) + L"\t" + (m.active ? L"1" : L"0") + L"\t" + s((long long)m.joinDate) + L"\n";
    }
    for (auto& v : visits) {
        out += L"V\t" + s(v.id) + L"\t" + s(v.memberId) + L"\t" + esc(v.name) + L"\t" + (v.guest ? L"1" : L"0") +
               L"\t" + s((long long)v.checkIn) + L"\t" + s((long long)v.checkOut) + L"\t" +
               (v.autoOut ? L"1" : L"0") + L"\n";
    }

    std::wstring tmp = file + L".tmp";
    if (!writeFileBytes(tmp, util::toUtf8(out))) {
        error = L"Datei kann nicht geschrieben werden: " + tmp;
        return false;
    }
    if (!MoveFileExW(tmp.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = L"Datei kann nicht ersetzt werden: " + file;
        return false;
    }
    return true;
}

Member* Store::findMember(int64_t id) {
    for (auto& m : members)
        if (m.id == id) return &m;
    return nullptr;
}

Visit* Store::findVisit(int64_t id) {
    for (auto& v : visits)
        if (v.id == id) return &v;
    return nullptr;
}

Visit* Store::openVisitFor(int64_t memberId) {
    for (auto& v : visits)
        if (v.memberId == memberId && !v.guest && v.present()) return &v;
    return nullptr;
}

// ------------------------------------------------------------------ Admin-Zugang

static const unsigned kPbkdf2Iterations = 200000;

static std::wstring toHex(const std::vector<unsigned char>& bytes) {
    static const wchar_t* digits = L"0123456789abcdef";
    std::wstring r;
    for (unsigned char b : bytes) {
        r += digits[b >> 4];
        r += digits[b & 15];
    }
    return r;
}

static std::vector<unsigned char> fromHex(const std::wstring& hex) {
    auto val = [](wchar_t c) { return c <= L'9' ? c - L'0' : (c | 0x20) - L'a' + 10; };
    std::vector<unsigned char> r;
    for (size_t i = 0; i + 1 < hex.size(); i += 2) r.push_back((unsigned char)(val(hex[i]) * 16 + val(hex[i + 1])));
    return r;
}

// PBKDF2-HMAC-SHA256 über das UTF-8-Passwort; leer bei Fehler
static std::wstring derive(const std::wstring& password, const std::vector<unsigned char>& salt, unsigned iterations) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) < 0)
        return {};
    std::string pw = util::toUtf8(password);
    std::vector<unsigned char> key(32);
    NTSTATUS st = BCryptDeriveKeyPBKDF2(alg, (PUCHAR)pw.data(), (ULONG)pw.size(), (PUCHAR)salt.data(),
                                        (ULONG)salt.size(), iterations, key.data(), (ULONG)key.size(), 0);
    BCryptCloseAlgorithmProvider(alg, 0);
    SecureZeroMemory(&pw[0], pw.size());
    return st < 0 ? std::wstring() : toHex(key);
}

bool Store::setAdmin(const std::wstring& user, const std::wstring& password) {
    std::vector<unsigned char> salt(16);
    if (BCryptGenRandom(nullptr, salt.data(), (ULONG)salt.size(), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return false;
    std::wstring hash = derive(password, salt, kPbkdf2Iterations);
    if (hash.empty()) return false;
    admin.user = user;
    admin.salt = toHex(salt);
    admin.iterations = kPbkdf2Iterations;
    admin.hash = hash;
    return true;
}

bool Store::checkAdmin(const std::wstring& user, const std::wstring& password) const {
    if (!admin.configured() || admin.iterations == 0) return false;
    std::wstring hash = derive(password, fromHex(admin.salt), admin.iterations);
    if (hash.empty() || hash.size() != admin.hash.size()) return false;
    unsigned diff = 0;  // Vergleich in konstanter Zeit
    for (size_t i = 0; i < hash.size(); ++i) diff |= (unsigned)(hash[i] ^ admin.hash[i]);
    return diff == 0 && util::equalsCI(user, admin.user);
}
