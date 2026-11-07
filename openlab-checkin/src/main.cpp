// OpenLab Augsburg – Mitglieder & Check-in
// Windows-Oberfläche (reines Win32 + Common Controls, keine Fremdbibliotheken)
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "model.h"
#include "resource.h"

// ------------------------------------------------------------------ IDs

enum : int {
    IDC_TAB = 100,
    // Check-in
    IDC_CI_LABEL, IDC_CI_EDIT, IDC_CI_BTN, IDC_CI_HINT,
    IDC_GUEST_LABEL, IDC_GUEST_EDIT, IDC_GUEST_BTN,
    IDC_PRESENT_LABEL, IDC_PRESENT_LIST, IDC_CHECKOUT_SEL, IDC_CHECKOUT_ALL,
    // Mitglieder
    IDC_M_SEARCH_LABEL, IDC_M_SEARCH, IDC_M_INACTIVE, IDC_M_EXPORT, IDC_M_NEW, IDC_M_LIST,
    IDC_F_TITLE,
    IDC_F_FIRST_L, IDC_F_FIRST, IDC_F_LAST_L, IDC_F_LAST,
    IDC_F_NUMBER_L, IDC_F_NUMBER, IDC_F_JOIN_L, IDC_F_JOIN,
    IDC_F_TYPE_L, IDC_F_TYPE, IDC_F_EMAIL_L, IDC_F_EMAIL, IDC_F_PHONE_L, IDC_F_PHONE,
    IDC_F_CARD_L, IDC_F_CARD, IDC_F_NOTES_L, IDC_F_NOTES, IDC_F_ACTIVE,
    IDC_F_SAVE, IDC_F_CANCEL, IDC_F_DELETE, IDC_F_TOGGLE,
    // Verlauf
    IDC_H_FROM_L, IDC_H_FROM, IDC_H_TO_L, IDC_H_TO, IDC_H_TODAY, IDC_H_MONTH, IDC_H_YEAR,
    IDC_H_SEARCH_L, IDC_H_SEARCH, IDC_H_EXPORT, IDC_H_STATS, IDC_H_LIST,
    // Admin (sichtbar auf den geschützten Reitern)
    IDC_ADMIN_CHANGE, IDC_ADMIN_LOGOUT,
};

enum StatusKind { ST_INFO, ST_SUCCESS, ST_GOODBYE, ST_ERROR };

// ------------------------------------------------------------------ Globale Zustände

static HINSTANCE gInst;
static HWND gMain, gTab;
static Store gStore;
static double gScale = 1.0;
static HFONT gFont, gFontBig, gFontTitle, gFontBold, gFontSmall, gFontStat;
static HBRUSH gWhiteBrush;

static std::vector<HWND> gPage[3];
static int gCurrentPage = 0;

static std::wstring gStatus = L"Bereit. Karte scannen oder Name / Mitgliedsnummer eingeben.";
static StatusKind gStatusKind = ST_INFO;

// Mitglieder-Formular
static int64_t gSelectedMemberId = 0;
static bool gIsNewMember = false;
static bool gEditing = false;
static bool gSuppressSelection = false;

// Admin-Bereich (Reiter "Mitglieder" und "Verlauf & Statistik")
static bool gAdminUnlocked = false;
static const DWORD kAdminIdleMs = 10 * 60 * 1000;  // automatisch sperren nach 10 Minuten ohne Eingabe

static const COLORREF kHeader = RGB(0x1F, 0x2A, 0x36);
static const COLORREF kAccent = RGB(0xF2, 0x8C, 0x28);
static const COLORREF kPage = RGB(0xF3, 0xF5, 0xF7);
static const COLORREF kMuted = RGB(0x6B, 0x77, 0x85);
static const COLORREF kHeaderSub = RGB(0xAF, 0xC0, 0xCF);

static int S(int v) { return (int)(v * gScale + 0.5); }
static int headerHeight() { return S(68); }
static int statusHeight() { return S(46); }

static HWND ctl(int id) { return GetDlgItem(gMain, id); }

static std::wstring getText(HWND h) {
    int n = GetWindowTextLengthW(h);
    std::wstring s(n, L'\0');
    if (n) GetWindowTextW(h, &s[0], n + 1);
    return s;
}
static std::wstring getText(int id) { return getText(ctl(id)); }
static void setText(int id, const std::wstring& s) { SetWindowTextW(ctl(id), s.c_str()); }

// ------------------------------------------------------------------ Status & Kopfzeile

static void invalidateHeader() {
    RECT rc;
    GetClientRect(gMain, &rc);
    rc.bottom = headerHeight();
    InvalidateRect(gMain, &rc, FALSE);
}

static void setStatus(const std::wstring& msg, StatusKind kind) {
    gStatus = msg;
    gStatusKind = kind;
    RECT rc;
    GetClientRect(gMain, &rc);
    rc.top = rc.bottom - statusHeight();
    InvalidateRect(gMain, &rc, FALSE);
}

static int presentCount() {
    int n = 0;
    for (auto& v : gStore.visits) n += v.present();
    return n;
}

static void paintWindow(HDC dc, const RECT& rc) {
    HBRUSH page = CreateSolidBrush(kPage);
    FillRect(dc, &rc, page);
    DeleteObject(page);
    SetBkMode(dc, TRANSPARENT);

    // Kopfzeile
    RECT head = {0, 0, rc.right, headerHeight()};
    HBRUSH hb = CreateSolidBrush(kHeader);
    FillRect(dc, &head, hb);
    DeleteObject(hb);

    RECT logo = {S(22), S(14), S(62), S(54)};
    HBRUSH ab = CreateSolidBrush(kAccent);
    HPEN nullPen = (HPEN)GetStockObject(NULL_PEN);
    HGDIOBJ oldPen = SelectObject(dc, nullPen);
    HGDIOBJ oldBrush = SelectObject(dc, ab);
    RoundRect(dc, logo.left, logo.top, logo.right, logo.bottom, S(10), S(10));
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(ab);

    SetTextColor(dc, RGB(255, 255, 255));
    SelectObject(dc, gFontBold);
    DrawTextW(dc, L"OL", -1, &logo, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RECT title = {S(76), S(12), rc.right / 2, S(40)};
    SelectObject(dc, gFontTitle);
    DrawTextW(dc, L"OpenLab Augsburg", -1, &title, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
    RECT sub = {S(76), S(40), rc.right / 2, S(60)};
    SelectObject(dc, gFontSmall);
    SetTextColor(dc, kHeaderSub);
    DrawTextW(dc, L"Mitglieder & Check-in", -1, &sub, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

    time_t now = time(nullptr);
    int active = 0, todayVisits = 0;
    time_t t0 = util::today();
    for (auto& m : gStore.members) active += m.active;
    for (auto& v : gStore.visits) todayVisits += v.checkIn >= t0;

    RECT clock = {rc.right / 2, S(14), rc.right - S(22), S(38)};
    SelectObject(dc, gFont);
    SetTextColor(dc, RGB(255, 255, 255));
    DrawTextW(dc, util::fmtClock(now).c_str(), -1, &clock, DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX);
    RECT counts = {rc.right / 2, S(38), rc.right - S(22), S(60)};
    std::wstring c = std::to_wstring(presentCount()) + L" anwesend  \x00B7  " + std::to_wstring(todayVisits) +
                     L" Besuche heute  \x00B7  " + std::to_wstring(active) + L" aktive Mitglieder";
    SelectObject(dc, gFontSmall);
    SetTextColor(dc, kHeaderSub);
    DrawTextW(dc, c.c_str(), -1, &counts, DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX);

    // Statusleiste
    COLORREF sc = gStatusKind == ST_SUCCESS   ? RGB(0x2E, 0x9E, 0x5B)
                  : gStatusKind == ST_GOODBYE ? RGB(0x2D, 0x6F, 0xB5)
                  : gStatusKind == ST_ERROR   ? RGB(0xC0, 0x39, 0x2B)
                                              : RGB(0x3A, 0x4A, 0x5A);
    RECT st = {0, rc.bottom - statusHeight(), rc.right, rc.bottom};
    HBRUSH sb = CreateSolidBrush(sc);
    FillRect(dc, &st, sb);
    DeleteObject(sb);
    st.left += S(22);
    st.right -= S(22);
    SetTextColor(dc, RGB(255, 255, 255));
    SelectObject(dc, gFontBold);
    DrawTextW(dc, gStatus.c_str(), -1, &st, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
}

// ------------------------------------------------------------------ ListView-Helfer

static void lvAddColumn(HWND lv, int idx, const wchar_t* title, int width) {
    LVCOLUMNW c{};
    c.mask = LVCF_TEXT | LVCF_WIDTH;
    c.pszText = const_cast<LPWSTR>(title);
    c.cx = S(width);
    ListView_InsertColumn(lv, idx, &c);
}

static int lvAddRow(HWND lv, LPARAM param, const std::vector<std::wstring>& cols) {
    LVITEMW it{};
    it.mask = LVIF_TEXT | LVIF_PARAM;
    it.iItem = ListView_GetItemCount(lv);
    it.lParam = param;
    it.pszText = const_cast<LPWSTR>(cols[0].c_str());
    int row = ListView_InsertItem(lv, &it);
    for (size_t c = 1; c < cols.size(); ++c)
        ListView_SetItemText(lv, row, (int)c, const_cast<LPWSTR>(cols[c].c_str()));
    return row;
}

static LPARAM lvParam(HWND lv, int row) {
    LVITEMW it{};
    it.mask = LVIF_PARAM;
    it.iItem = row;
    ListView_GetItem(lv, &it);
    return it.lParam;
}

// Spalte "stretchCol" füllt die verbleibende Breite
static void lvFit(HWND lv, int stretchCol) {
    HWND header = ListView_GetHeader(lv);
    int cols = Header_GetItemCount(header);
    RECT rc;
    GetClientRect(lv, &rc);
    int used = 0;
    for (int i = 0; i < cols; ++i)
        if (i != stretchCol) used += ListView_GetColumnWidth(lv, i);
    ListView_SetColumnWidth(lv, stretchCol, std::max(S(120), (int)rc.right - used - 2));
}

// ------------------------------------------------------------------ Datum <-> SYSTEMTIME

static SYSTEMTIME toSystemTime(time_t t) {
    tm r{};
    localtime_s(&r, &t);
    SYSTEMTIME st{};
    st.wYear = (WORD)(r.tm_year + 1900);
    st.wMonth = (WORD)(r.tm_mon + 1);
    st.wDay = (WORD)r.tm_mday;
    return st;
}

static time_t fromSystemTime(const SYSTEMTIME& st) {
    tm r{};
    r.tm_year = st.wYear - 1900;
    r.tm_mon = st.wMonth - 1;
    r.tm_mday = st.wDay;
    r.tm_isdst = -1;
    return mktime(&r);
}

static time_t getDate(int id) {
    SYSTEMTIME st{};
    DateTime_GetSystemtime(ctl(id), &st);
    return fromSystemTime(st);
}

static void setDate(int id, time_t t) {
    SYSTEMTIME st = toSystemTime(t);
    DateTime_SetSystemtime(ctl(id), GDT_VALID, &st);
}

static time_t addDays(time_t day, int n) {
    tm r{};
    localtime_s(&r, &day);
    r.tm_mday += n;
    r.tm_isdst = -1;
    return mktime(&r);
}

// ------------------------------------------------------------------ Listen aktualisieren

static void refreshPresent() {
    HWND lv = ctl(IDC_PRESENT_LIST);
    SendMessageW(lv, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(lv);

    std::vector<const Visit*> list;
    for (auto& v : gStore.visits)
        if (v.present()) list.push_back(&v);
    std::sort(list.begin(), list.end(), [](auto a, auto b) { return a->checkIn < b->checkIn; });

    time_t now = time(nullptr);
    for (auto v : list)
        lvAddRow(lv, (LPARAM)v->id,
                 {v->name, v->guest ? L"Gast" : L"Mitglied", util::fmtTime(v->checkIn),
                  util::fmtDuration(v->seconds(now))});

    SendMessageW(lv, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(lv, nullptr, TRUE);

    std::wstring label = L"Gerade im OpenLab (" + std::to_wstring(list.size()) + L")";
    if (list.empty()) label += L"  \x2013  gerade ist niemand eingecheckt.";
    setText(IDC_PRESENT_LABEL, label);
    EnableWindow(ctl(IDC_CHECKOUT_ALL), !list.empty());
}

static void updatePresentDurations() {
    HWND lv = ctl(IDC_PRESENT_LIST);
    time_t now = time(nullptr);
    int n = ListView_GetItemCount(lv);
    for (int i = 0; i < n; ++i) {
        if (Visit* v = gStore.findVisit((int64_t)lvParam(lv, i))) {
            std::wstring d = util::fmtDuration(v->seconds(now));
            wchar_t cur[32] = {};
            ListView_GetItemText(lv, i, 3, cur, 32);
            if (d != cur) ListView_SetItemText(lv, i, 3, const_cast<LPWSTR>(d.c_str()));
        }
    }
}

static std::vector<const Member*> filteredMembers() {
    std::wstring q = util::trim(getText(IDC_M_SEARCH));
    bool showInactive = Button_GetCheck(ctl(IDC_M_INACTIVE)) == BST_CHECKED;
    std::vector<const Member*> list;
    for (auto& m : gStore.members) {
        if (!showInactive && !m.active) continue;
        if (!q.empty() && !util::containsCI(m.fullName(), q) && !util::containsCI(m.number, q) &&
            !util::containsCI(m.email, q) && !util::containsCI(m.card, q))
            continue;
        list.push_back(&m);
    }
    std::sort(list.begin(), list.end(), [](auto a, auto b) {
        std::wstring la = util::lower(a->last), lb = util::lower(b->last);
        if (la != lb) return la < lb;
        return util::lower(a->first) < util::lower(b->first);
    });
    return list;
}

static void refreshMembers() {
    HWND lv = ctl(IDC_M_LIST);
    gSuppressSelection = true;
    SendMessageW(lv, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(lv);
    for (auto m : filteredMembers()) {
        int row = lvAddRow(lv, (LPARAM)m->id,
                           {m->number, m->last, m->first, m->type, m->email, m->active ? L"ja" : L"nein"});
        if (m->id == gSelectedMemberId) {
            ListView_SetItemState(lv, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(lv, row, FALSE);
        }
    }
    SendMessageW(lv, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(lv, nullptr, TRUE);
    gSuppressSelection = false;
}

static std::vector<const Visit*> filteredHistory() {
    time_t from = util::startOfDay(getDate(IDC_H_FROM));
    time_t to = addDays(util::startOfDay(getDate(IDC_H_TO)), 1);
    std::wstring q = util::trim(getText(IDC_H_SEARCH));
    std::vector<const Visit*> list;
    for (auto& v : gStore.visits)
        if (v.checkIn >= from && v.checkIn < to && util::containsCI(v.name, q)) list.push_back(&v);
    std::sort(list.begin(), list.end(), [](auto a, auto b) { return a->checkIn > b->checkIn; });
    return list;
}

static void refreshHistory() {
    HWND lv = ctl(IDC_H_LIST);
    SendMessageW(lv, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(lv);

    auto list = filteredHistory();
    time_t now = time(nullptr);
    std::set<std::wstring> people;
    int guests = 0;
    double hours = 0;
    for (auto v : list) {
        lvAddRow(lv, (LPARAM)v->id,
                 {util::fmtDateWeekday(v->checkIn), v->name, v->guest ? L"Gast" : L"Mitglied",
                  util::fmtTime(v->checkIn), v->present() ? L"anwesend" : util::fmtTime(v->checkOut),
                  util::fmtDuration(v->seconds(now)), v->autoOut ? L"automatisch ausgecheckt" : L""});
        people.insert(v->guest ? L"g:" + util::lower(v->name) : L"m:" + std::to_wstring(v->memberId));
        guests += v->guest;
        hours += v->seconds(now) / 3600.0;
    }
    SendMessageW(lv, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(lv, nullptr, TRUE);

    std::wstring stats = std::to_wstring(list.size()) + L" Besuche     \x00B7     " +
                         std::to_wstring(people.size()) + L" verschiedene Personen     \x00B7     " +
                         std::to_wstring(guests) + L" Gastbesuche     \x00B7     " + util::fmtDecimal(hours, 1) +
                         L" Stunden im OpenLab";
    setText(IDC_H_STATS, stats);
}

static void refreshAll() {
    refreshPresent();
    refreshMembers();
    refreshHistory();
    invalidateHeader();
}

static void persist() {
    std::wstring err;
    if (!gStore.save(err)) setStatus(L"Speichern fehlgeschlagen: " + err, ST_ERROR);
    refreshAll();
}

// ------------------------------------------------------------------ Check-in-Logik

static void checkOut(Visit& v) {
    if (!v.present()) return;
    v.checkOut = time(nullptr);
    setStatus(L"Tschüss, " + v.name + L"! Ausgecheckt nach " + util::fmtDuration(v.seconds(v.checkOut)) + L".",
              ST_GOODBYE);
    persist();
}

static void toggleMember(const Member& m) {
    if (Visit* open = gStore.openVisitFor(m.id)) {
        checkOut(*open);
        return;
    }
    if (!m.active) {
        setStatus(m.fullName() + L" ist als inaktiv markiert \x2013 bitte beim Vorstand melden.", ST_ERROR);
        return;
    }
    Visit v;
    v.id = gStore.newId();
    v.memberId = m.id;
    v.name = m.fullName();
    v.checkIn = time(nullptr);
    gStore.visits.push_back(v);
    setStatus(L"Willkommen im OpenLab, " + m.first + L"! Eingecheckt um " + util::fmtTime(v.checkIn) + L" Uhr.",
              ST_SUCCESS);
    persist();
}

static void doCheckIn() {
    std::wstring input = util::trim(getText(IDC_CI_EDIT));
    setText(IDC_CI_EDIT, L"");
    SetFocus(ctl(IDC_CI_EDIT));
    if (input.empty()) return;

    // 1. exakt: Kartennummer oder Mitgliedsnummer
    std::vector<const Member*> matches;
    for (auto& m : gStore.members)
        if ((!m.card.empty() && util::equalsCI(m.card, input)) || util::equalsCI(m.number, input))
            matches.push_back(&m);
    // 2. Namenssuche
    if (matches.empty())
        for (auto& m : gStore.members)
            if (util::containsCI(m.fullName(), input)) matches.push_back(&m);
    if (matches.size() > 1) {
        std::vector<const Member*> active;
        for (auto m : matches)
            if (m->active) active.push_back(m);
        if (active.size() == 1) matches = active;
    }

    if (matches.empty()) {
        setStatus(L"Kein Mitglied gefunden für \x201E" + input + L"\x201C. Gäste bitte rechts als Gast einchecken.",
                  ST_ERROR);
    } else if (matches.size() > 1) {
        std::wstring names;
        for (size_t i = 0; i < matches.size() && i < 5; ++i)
            names += (i ? L", " : L"") + matches[i]->fullName() + L" (#" + matches[i]->number + L")";
        if (matches.size() > 5) names += L" \x2026";
        setStatus(L"Mehrere Treffer: " + names + L" \x2013 bitte genauer eingeben.", ST_ERROR);
    } else {
        Member copy = *matches[0];
        toggleMember(copy);
    }
}

static void doGuestCheckIn() {
    std::wstring name = util::trim(getText(IDC_GUEST_EDIT));
    if (name.empty()) return;
    Visit v;
    v.id = gStore.newId();
    v.name = name;
    v.guest = true;
    v.checkIn = time(nullptr);
    gStore.visits.push_back(v);
    setText(IDC_GUEST_EDIT, L"");
    setStatus(L"Gast " + name + L" eingecheckt um " + util::fmtTime(v.checkIn) + L" Uhr. Viel Spaß!", ST_SUCCESS);
    persist();
}

static void doCheckOutSelected() {
    HWND lv = ctl(IDC_PRESENT_LIST);
    int sel = ListView_GetNextItem(lv, -1, LVNI_SELECTED);
    if (sel < 0) {
        setStatus(L"Bitte zuerst eine Person in der Liste auswählen (oder doppelklicken).", ST_INFO);
        return;
    }
    if (Visit* v = gStore.findVisit((int64_t)lvParam(lv, sel))) checkOut(*v);
}

static void doCheckOutAll() {
    int n = presentCount();
    if (n == 0) return;
    std::wstring q = L"Alle " + std::to_wstring(n) + L" anwesenden Personen auschecken?\n(z. B. beim Abschließen)";
    if (MessageBoxW(gMain, q.c_str(), L"Alle auschecken", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    time_t now = time(nullptr);
    for (auto& v : gStore.visits)
        if (v.present()) v.checkOut = now;
    setStatus(std::to_wstring(n) + L" Personen ausgecheckt. Gute Nacht, OpenLab!", ST_GOODBYE);
    persist();
}

// ------------------------------------------------------------------ Mitglieder-Formular

static const int kFormInputs[] = {IDC_F_FIRST, IDC_F_LAST, IDC_F_NUMBER, IDC_F_JOIN, IDC_F_TYPE, IDC_F_EMAIL,
                                  IDC_F_PHONE, IDC_F_CARD, IDC_F_NOTES, IDC_F_ACTIVE, IDC_F_SAVE, IDC_F_CANCEL};

static void updateFormButtons() {
    bool existing = gEditing && !gIsNewMember && gSelectedMemberId != 0;
    EnableWindow(ctl(IDC_F_DELETE), existing);
    EnableWindow(ctl(IDC_F_TOGGLE), existing);
    setText(IDC_F_TITLE, !gEditing      ? L"Mitglied auswählen oder \x201E+ Neues Mitglied\x201C"
                         : gIsNewMember ? L"Neues Mitglied"
                                        : L"Mitglied bearbeiten");
}

static void setFormEnabled(bool on) {
    gEditing = on;
    for (int id : kFormInputs) EnableWindow(ctl(id), on);
    updateFormButtons();
}

static void selectType(const std::wstring& type) {
    HWND cb = ctl(IDC_F_TYPE);
    int idx = (int)SendMessageW(cb, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)type.c_str());
    SendMessageW(cb, CB_SETCURSEL, idx < 0 ? 0 : idx, 0);
}

static void loadForm(const Member& m) {
    setText(IDC_F_FIRST, m.first);
    setText(IDC_F_LAST, m.last);
    setText(IDC_F_NUMBER, m.number);
    setDate(IDC_F_JOIN, m.joinDate ? m.joinDate : util::today());
    selectType(m.type);
    setText(IDC_F_EMAIL, m.email);
    setText(IDC_F_PHONE, m.phone);
    setText(IDC_F_CARD, m.card);
    setText(IDC_F_NOTES, m.notes);
    Button_SetCheck(ctl(IDC_F_ACTIVE), m.active ? BST_CHECKED : BST_UNCHECKED);
}

static void clearForm() {
    Member empty;
    empty.type = kMembershipTypes[0];
    loadForm(empty);
}

static void readForm(Member& m) {
    m.first = util::trim(getText(IDC_F_FIRST));
    m.last = util::trim(getText(IDC_F_LAST));
    m.number = util::trim(getText(IDC_F_NUMBER));
    m.joinDate = util::startOfDay(getDate(IDC_F_JOIN));
    m.type = getText(IDC_F_TYPE);
    m.email = util::trim(getText(IDC_F_EMAIL));
    m.phone = util::trim(getText(IDC_F_PHONE));
    m.card = util::trim(getText(IDC_F_CARD));
    m.notes = getText(IDC_F_NOTES);
    m.active = Button_GetCheck(ctl(IDC_F_ACTIVE)) == BST_CHECKED;
}

static std::wstring nextMemberNumber() {
    long long mx = 0;
    for (auto& m : gStore.members) {
        wchar_t* end = nullptr;
        long long n = std::wcstoll(m.number.c_str(), &end, 10);
        if (end && *end == 0 && !m.number.empty()) mx = std::max(mx, n);
    }
    wchar_t b[32];
    swprintf(b, 32, L"%04lld", mx + 1);
    return b;
}

static void onMemberSelected(int64_t id) {
    const Member* m = gStore.findMember(id);
    if (!m) return;
    gSelectedMemberId = id;
    gIsNewMember = false;
    loadForm(*m);
    setFormEnabled(true);
}

static void doNewMember() {
    gSelectedMemberId = 0;
    gIsNewMember = true;
    gSuppressSelection = true;
    ListView_SetItemState(ctl(IDC_M_LIST), -1, 0, LVIS_SELECTED);
    gSuppressSelection = false;
    clearForm();
    setText(IDC_F_NUMBER, nextMemberNumber());
    setFormEnabled(true);
    SetFocus(ctl(IDC_F_FIRST));
}

static void doSaveMember() {
    if (!gEditing) return;
    Member e;
    readForm(e);
    int64_t selfId = gIsNewMember ? 0 : gSelectedMemberId;

    std::wstring error;
    if (e.first.empty() || e.last.empty())
        error = L"Vor- und Nachname sind Pflichtfelder.";
    else if (e.number.empty())
        error = L"Bitte eine Mitgliedsnummer angeben.";
    else
        for (auto& m : gStore.members) {
            if (m.id == selfId) continue;
            if (util::equalsCI(m.number, e.number)) {
                error = L"Die Mitgliedsnummer " + e.number + L" ist bereits an " + m.fullName() + L" vergeben.";
                break;
            }
            if (!e.card.empty() && util::equalsCI(m.card, e.card)) {
                error = L"Diese Kartennummer ist bereits " + m.fullName() + L" zugeordnet.";
                break;
            }
        }
    if (!error.empty()) {
        MessageBoxW(gMain, error.c_str(), L"Bitte prüfen", MB_OK | MB_ICONWARNING);
        return;
    }

    if (gIsNewMember) {
        e.id = gStore.newId();
        gStore.members.push_back(e);
        gSelectedMemberId = e.id;
        gIsNewMember = false;
        setStatus(e.fullName() + L" wurde als Mitglied angelegt.", ST_SUCCESS);
    } else if (Member* m = gStore.findMember(gSelectedMemberId)) {
        e.id = m->id;
        *m = e;
        setStatus(L"Änderungen an " + e.fullName() + L" gespeichert.", ST_INFO);
    }
    persist();
    updateFormButtons();
}

static void doCancelEdit() {
    if (gIsNewMember || gSelectedMemberId == 0) {
        gIsNewMember = false;
        clearForm();
        setFormEnabled(false);
    } else if (const Member* m = gStore.findMember(gSelectedMemberId)) {
        loadForm(*m);
    }
}

static void doDeleteMember() {
    Member* m = gStore.findMember(gSelectedMemberId);
    if (!m || gIsNewMember) return;
    int visits = 0;
    for (auto& v : gStore.visits) visits += v.memberId == m->id && !v.guest;
    std::wstring q = m->fullName() + L" (#" + m->number + L") wirklich löschen?";
    if (visits)
        q += L"\n\nDie " + std::to_wstring(visits) +
             L" Besuche bleiben im Verlauf erhalten.\nTipp: Statt zu löschen kann das Mitglied auch auf "
             L"\x201Einaktiv\x201C gesetzt werden.";
    if (MessageBoxW(gMain, q.c_str(), L"Mitglied löschen", MB_YESNO | MB_ICONWARNING) != IDYES) return;

    std::wstring name = m->fullName();
    int64_t id = m->id;
    time_t now = time(nullptr);
    for (auto& v : gStore.visits)
        if (v.memberId == id && !v.guest && v.present()) v.checkOut = now;
    gStore.members.erase(std::remove_if(gStore.members.begin(), gStore.members.end(),
                                        [id](const Member& x) { return x.id == id; }),
                         gStore.members.end());
    gSelectedMemberId = 0;
    clearForm();
    setFormEnabled(false);
    setStatus(name + L" wurde gelöscht.", ST_INFO);
    persist();
}

static void doToggleSelectedMember() {
    if (gIsNewMember) return;
    if (const Member* m = gStore.findMember(gSelectedMemberId)) {
        Member copy = *m;
        toggleMember(copy);
    }
}

// ------------------------------------------------------------------ Export

static bool askSavePath(const wchar_t* title, const std::wstring& suggested, std::wstring& path) {
    wchar_t buf[MAX_PATH];
    lstrcpynW(buf, suggested.c_str(), MAX_PATH);
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = gMain;
    ofn.lpstrFilter = L"CSV-Datei (*.csv)\0*.csv\0\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.lpstrDefExt = L"csv";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return false;
    path = buf;
    return true;
}

static std::wstring isoDate(time_t t) {
    std::wstring d = util::fmtDate(t);  // TT.MM.JJJJ
    return d.substr(6, 4) + L"-" + d.substr(3, 2) + L"-" + d.substr(0, 2);
}

static void finishExport(bool ok, const std::wstring& path) {
    if (ok)
        setStatus(L"Export gespeichert: " + path, ST_INFO);
    else
        MessageBoxW(gMain, (L"Export fehlgeschlagen:\n" + path).c_str(), L"Fehler", MB_OK | MB_ICONERROR);
}

static void doExportHistory() {
    std::wstring path;
    if (!askSavePath(L"Besuche exportieren",
                     L"OpenLab-Besuche_" + isoDate(getDate(IDC_H_FROM)) + L"_bis_" + isoDate(getDate(IDC_H_TO)) +
                         L".csv",
                     path))
        return;
    time_t now = time(nullptr);
    std::vector<std::vector<std::wstring>> rows;
    for (auto v : filteredHistory()) {
        const Member* m = v->guest ? nullptr : gStore.findMember(v->memberId);
        rows.push_back({util::fmtDate(v->checkIn), v->name, m ? m->number : L"", v->guest ? L"Gast" : L"Mitglied",
                        util::fmtTime(v->checkIn), v->present() ? L"anwesend" : util::fmtTime(v->checkOut),
                        util::fmtDecimal(v->seconds(now) / 3600.0, 2),
                        v->autoOut ? L"automatisch ausgecheckt" : L""});
    }
    finishExport(util::writeCsv(path, {L"Datum", L"Name", L"Mitgliedsnr.", L"Art", L"Check-in", L"Check-out",
                                       L"Dauer (h)", L"Hinweis"},
                                rows),
                 path);
}

static void doExportMembers() {
    std::wstring path;
    if (!askSavePath(L"Mitgliederliste exportieren", L"OpenLab-Mitglieder_" + isoDate(time(nullptr)) + L".csv", path))
        return;
    std::vector<std::vector<std::wstring>> rows;
    for (auto m : filteredMembers())
        rows.push_back({m->number, m->last, m->first, m->email, m->phone, m->type, util::fmtDate(m->joinDate),
                        m->active ? L"ja" : L"nein", m->card, m->notes});
    finishExport(util::writeCsv(path, {L"Mitgliedsnr.", L"Nachname", L"Vorname", L"E-Mail", L"Telefon",
                                       L"Mitgliedsart", L"Eintritt", L"Aktiv", L"Kartennr.", L"Notizen"},
                                rows),
                 path);
}

static void setHistoryRange(time_t from, time_t to) {
    setDate(IDC_H_FROM, from);
    setDate(IDC_H_TO, to);
    refreshHistory();
}

// ------------------------------------------------------------------ Admin-Anmeldung

static void dlgError(HWND dlg, const wchar_t* msg, int focusId) {
    SetDlgItemTextW(dlg, IDC_D_ERROR, msg);
    HWND f = GetDlgItem(dlg, focusId);
    SetFocus(f);
    SendMessageW(f, EM_SETSEL, 0, -1);
}

static std::wstring dlgText(HWND dlg, int id) {
    HWND h = GetDlgItem(dlg, id);
    return h ? getText(h) : std::wstring();
}

// Ein Dialog-Proc für alle drei Admin-Dialoge (IDD_LOGIN, IDD_SETUP, IDD_CHANGE); lParam = Dialog-ID
static INT_PTR CALLBACK AdminDlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_INITDIALOG:
            SetWindowLongPtrW(dlg, DWLP_USER, lp);
            for (int id : {IDC_D_USER, IDC_D_CURRENT, IDC_D_PASS, IDC_D_PASS2})
                if (HWND e = GetDlgItem(dlg, id)) SendMessageW(e, EM_LIMITTEXT, 128, 0);
            if (lp == IDD_CHANGE) SetDlgItemTextW(dlg, IDC_D_USER, gStore.admin.user.c_str());
            SetFocus(GetDlgItem(dlg, lp == IDD_CHANGE ? IDC_D_CURRENT : IDC_D_USER));
            return FALSE;  // Fokus selbst gesetzt

        case WM_CTLCOLORSTATIC:
            if (GetDlgCtrlID((HWND)lp) == IDC_D_ERROR) {
                SetTextColor((HDC)wp, RGB(0xC0, 0x39, 0x2B));
                SetBkMode((HDC)wp, TRANSPARENT);
                return (INT_PTR)GetSysColorBrush(COLOR_BTNFACE);
            }
            return FALSE;

        case WM_COMMAND:
            if (LOWORD(wp) == IDCANCEL) {
                EndDialog(dlg, IDCANCEL);
                return TRUE;
            }
            if (LOWORD(wp) == IDOK) {
                INT_PTR mode = GetWindowLongPtrW(dlg, DWLP_USER);
                std::wstring user = util::trim(dlgText(dlg, IDC_D_USER));
                std::wstring pass = dlgText(dlg, IDC_D_PASS);
                std::wstring pass2 = dlgText(dlg, IDC_D_PASS2);
                std::wstring current = dlgText(dlg, IDC_D_CURRENT);
                HCURSOR oldCursor = SetCursor(LoadCursorW(nullptr, IDC_WAIT));

                if (mode == IDD_LOGIN) {
                    bool ok = gStore.checkAdmin(user, pass);
                    SetCursor(oldCursor);
                    if (!ok) {
                        SetDlgItemTextW(dlg, IDC_D_PASS, L"");
                        dlgError(dlg, L"Benutzername oder Passwort ist falsch.", user.empty() ? IDC_D_USER : IDC_D_PASS);
                        return TRUE;
                    }
                    EndDialog(dlg, IDOK);
                    return TRUE;
                }

                // IDD_SETUP / IDD_CHANGE
                if (mode == IDD_CHANGE && !gStore.checkAdmin(gStore.admin.user, current)) {
                    SetCursor(oldCursor);
                    SetDlgItemTextW(dlg, IDC_D_CURRENT, L"");
                    dlgError(dlg, L"Das aktuelle Passwort ist falsch.", IDC_D_CURRENT);
                    return TRUE;
                }
                SetCursor(oldCursor);
                if (user.empty()) {
                    dlgError(dlg, L"Bitte einen Benutzernamen angeben.", IDC_D_USER);
                } else if (pass.size() < 6) {
                    dlgError(dlg, L"Das Passwort muss mindestens 6 Zeichen lang sein.", IDC_D_PASS);
                } else if (pass != pass2) {
                    SetDlgItemTextW(dlg, IDC_D_PASS2, L"");
                    dlgError(dlg, L"Die beiden Passwörter stimmen nicht überein.", IDC_D_PASS2);
                } else if (!gStore.setAdmin(user, pass)) {
                    dlgError(dlg, L"Das Passwort konnte nicht verschlüsselt werden.", IDC_D_PASS);
                } else {
                    EndDialog(dlg, IDOK);
                }
                return TRUE;
            }
            return FALSE;
    }
    return FALSE;
}

static bool runAdminDialog(int id) { return DialogBoxParamW(gInst, MAKEINTRESOURCEW(id), gMain, AdminDlgProc, id) == IDOK; }

// Fragt bei Bedarf nach dem Admin-Zugang (bzw. lässt ihn beim ersten Mal einrichten).
static bool requireAdmin() {
    if (gAdminUnlocked) return true;
    if (!gStore.admin.configured()) {
        if (!runAdminDialog(IDD_SETUP)) return false;
        gAdminUnlocked = true;
        setStatus(L"Admin-Zugang \x201E" + gStore.admin.user + L"\x201C eingerichtet und angemeldet.", ST_SUCCESS);
        persist();
        return true;
    }
    if (!runAdminDialog(IDD_LOGIN)) return false;
    gAdminUnlocked = true;
    setStatus(L"Als Admin angemeldet. Bitte nach der Arbeit \x201E" L"Abmelden\x201C klicken.", ST_INFO);
    return true;
}

static void showPage(int page);

static void lockAdmin(const std::wstring& message) {
    gAdminUnlocked = false;
    // offenes Formular verwerfen, damit nach dem Sperren nichts sichtbar bleibt
    gSelectedMemberId = 0;
    gIsNewMember = false;
    clearForm();
    setFormEnabled(false);
    setText(IDC_M_SEARCH, L"");
    setText(IDC_H_SEARCH, L"");
    refreshMembers();
    TabCtrl_SetCurSel(gTab, 0);
    showPage(0);
    setStatus(message, ST_INFO);
}

static void doChangeAdmin() {
    if (!gAdminUnlocked) return;
    if (runAdminDialog(IDD_CHANGE)) {
        setStatus(L"Admin-Zugang ge\x00E4ndert (Benutzer \x201E" + gStore.admin.user + L"\x201C).", ST_SUCCESS);
        persist();
    }
}

static void checkAdminIdle() {
    if (!gAdminUnlocked || !IsWindowEnabled(gMain)) return;  // nicht während ein Dialog offen ist
    LASTINPUTINFO li{};
    li.cbSize = sizeof(li);
    if (GetLastInputInfo(&li) && GetTickCount() - li.dwTime > kAdminIdleMs)
        lockAdmin(L"Admin-Bereich wurde nach 10 Minuten ohne Eingabe automatisch gesperrt.");
}

// ------------------------------------------------------------------ Steuerelemente anlegen

static LRESULT CALLBACK EnterSubclass(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR target) {
    if (msg == WM_KEYDOWN && wp == VK_RETURN) {
        if (target) PostMessageW(gMain, WM_COMMAND, MAKEWPARAM((int)target, BN_CLICKED), 0);
        return 0;
    }
    if (msg == WM_CHAR && wp == L'\r') return 0;  // kein Piepton
    return DefSubclassProc(h, msg, wp, lp);
}

static HWND make(int page, const wchar_t* cls, const wchar_t* text, DWORD style, int id, HFONT font = nullptr,
                 DWORD exStyle = 0) {
    HWND h = CreateWindowExW(exStyle, cls, text, WS_CHILD | WS_CLIPSIBLINGS | style, 0, 0, 10, 10, gMain,
                             (HMENU)(INT_PTR)id, gInst, nullptr);
    SendMessageW(h, WM_SETFONT, (WPARAM)(font ? font : gFont), TRUE);
    if (page >= 0) gPage[page].push_back(h);
    return h;
}

static HWND label(int page, const wchar_t* text, int id, HFONT font = nullptr) {
    return make(page, L"STATIC", text, SS_LEFT | SS_NOPREFIX, id, font);
}
static HWND edit(int page, int id, DWORD extra = 0, HFONT font = nullptr) {
    return make(page, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL | extra, id, font, WS_EX_CLIENTEDGE);
}
static HWND button(int page, const wchar_t* text, int id, HFONT font = nullptr) {
    return make(page, L"BUTTON", text, WS_TABSTOP | BS_PUSHBUTTON, id, font);
}
static HWND listView(int page, int id) {
    HWND lv = make(page, WC_LISTVIEWW, L"", WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL, id,
                   nullptr, WS_EX_CLIENTEDGE);
    ListView_SetExtendedListViewStyle(lv, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    return lv;
}
static HWND datePicker(int page, int id) {
    HWND h = make(page, DATETIMEPICK_CLASSW, L"", WS_TABSTOP | DTS_SHORTDATEFORMAT, id);
    DateTime_SetFormat(h, L"dd.MM.yyyy");
    return h;
}

static void createControls() {
    gTab = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP, 0, 0, 10,
                           10, gMain, (HMENU)IDC_TAB, gInst, nullptr);
    SendMessageW(gTab, WM_SETFONT, (WPARAM)gFontBold, TRUE);
    const wchar_t* tabs[] = {L"  Check-in  ", L"  Mitglieder  ", L"  Verlauf && Statistik  "};
    for (int i = 0; i < 3; ++i) {
        TCITEMW ti{};
        ti.mask = TCIF_TEXT;
        ti.pszText = const_cast<LPWSTR>(tabs[i]);
        TabCtrl_InsertItem(gTab, i, &ti);
    }

    // --- Seite 0: Check-in
    label(0, L"Karte scannen oder Name / Mitgliedsnummer eingeben:", IDC_CI_LABEL, gFontBold);
    edit(0, IDC_CI_EDIT, 0, gFontBig);
    button(0, L"Ein- / Auschecken", IDC_CI_BTN, gFontBold);
    label(0,
          L"Wer schon eingecheckt ist, wird beim erneuten Scannen automatisch ausgecheckt. "
          L"USB-RFID-Leser (Tastatur-Modus) funktionieren direkt.",
          IDC_CI_HINT, gFontSmall);
    label(0, L"Gast einchecken (Name):", IDC_GUEST_LABEL, gFontBold);
    edit(0, IDC_GUEST_EDIT);
    button(0, L"Als Gast einchecken", IDC_GUEST_BTN);
    label(0, L"Gerade im OpenLab", IDC_PRESENT_LABEL, gFontBold);
    button(0, L"Ausgewählte auschecken", IDC_CHECKOUT_SEL);
    button(0, L"Alle auschecken (Abschließen)", IDC_CHECKOUT_ALL);
    HWND pl = listView(0, IDC_PRESENT_LIST);
    SendMessageW(pl, WM_SETFONT, (WPARAM)gFontBold, TRUE);
    lvAddColumn(pl, 0, L"Name", 300);
    lvAddColumn(pl, 1, L"Art", 120);
    lvAddColumn(pl, 2, L"Seit", 100);
    lvAddColumn(pl, 3, L"Dauer", 110);
    SetWindowSubclass(ctl(IDC_CI_EDIT), EnterSubclass, 1, IDC_CI_BTN);
    SetWindowSubclass(ctl(IDC_GUEST_EDIT), EnterSubclass, 1, IDC_GUEST_BTN);

    // --- Seite 1: Mitglieder
    label(1, L"Suche:", IDC_M_SEARCH_LABEL);
    edit(1, IDC_M_SEARCH);
    make(1, L"BUTTON", L"Inaktive anzeigen", WS_TABSTOP | BS_AUTOCHECKBOX, IDC_M_INACTIVE);
    Button_SetCheck(ctl(IDC_M_INACTIVE), BST_CHECKED);
    button(1, L"CSV-Export", IDC_M_EXPORT);
    button(1, L"+ Neues Mitglied", IDC_M_NEW, gFontBold);
    HWND ml = listView(1, IDC_M_LIST);
    lvAddColumn(ml, 0, L"Nr.", 70);
    lvAddColumn(ml, 1, L"Nachname", 150);
    lvAddColumn(ml, 2, L"Vorname", 130);
    lvAddColumn(ml, 3, L"Mitgliedsart", 120);
    lvAddColumn(ml, 4, L"E-Mail", 200);
    lvAddColumn(ml, 5, L"Aktiv", 55);

    label(1, L"", IDC_F_TITLE, gFontBold);
    label(1, L"Vorname *", IDC_F_FIRST_L);
    edit(1, IDC_F_FIRST);
    label(1, L"Nachname *", IDC_F_LAST_L);
    edit(1, IDC_F_LAST);
    label(1, L"Mitgliedsnummer *", IDC_F_NUMBER_L);
    edit(1, IDC_F_NUMBER);
    label(1, L"Eintrittsdatum", IDC_F_JOIN_L);
    datePicker(1, IDC_F_JOIN);
    label(1, L"Mitgliedsart", IDC_F_TYPE_L);
    HWND cb = make(1, WC_COMBOBOXW, L"", WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, IDC_F_TYPE);
    for (int i = 0; i < kMembershipTypeCount; ++i) SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)kMembershipTypes[i]);
    label(1, L"E-Mail", IDC_F_EMAIL_L);
    edit(1, IDC_F_EMAIL);
    label(1, L"Telefon", IDC_F_PHONE_L);
    edit(1, IDC_F_PHONE);
    label(1, L"Kartennummer (RFID) \x2013 Feld anklicken und Karte scannen", IDC_F_CARD_L);
    edit(1, IDC_F_CARD);
    label(1, L"Notizen (z. B. Einweisungen: Laser, 3D-Drucker \x2026)", IDC_F_NOTES_L);
    edit(1, IDC_F_NOTES, ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL | WS_VSCROLL);
    make(1, L"BUTTON", L"Mitgliedschaft aktiv", WS_TABSTOP | BS_AUTOCHECKBOX, IDC_F_ACTIVE);
    button(1, L"Speichern", IDC_F_SAVE, gFontBold);
    button(1, L"Verwerfen", IDC_F_CANCEL);
    button(1, L"Löschen", IDC_F_DELETE);
    button(1, L"Dieses Mitglied ein-/auschecken", IDC_F_TOGGLE);
    // Kartennummer: Enter vom Kartenleser nicht als Zeilenumbruch / Piepton behandeln
    SetWindowSubclass(ctl(IDC_F_CARD), EnterSubclass, 1, 0);

    // --- Seite 2: Verlauf
    label(2, L"Von", IDC_H_FROM_L);
    datePicker(2, IDC_H_FROM);
    label(2, L"bis", IDC_H_TO_L);
    datePicker(2, IDC_H_TO);
    button(2, L"Heute", IDC_H_TODAY);
    button(2, L"Dieser Monat", IDC_H_MONTH);
    button(2, L"Dieses Jahr", IDC_H_YEAR);
    label(2, L"Name:", IDC_H_SEARCH_L);
    edit(2, IDC_H_SEARCH);
    button(2, L"CSV-Export", IDC_H_EXPORT, gFontBold);
    label(2, L"", IDC_H_STATS, gFontStat);
    HWND hl = listView(2, IDC_H_LIST);
    lvAddColumn(hl, 0, L"Datum", 130);
    lvAddColumn(hl, 1, L"Name", 220);
    lvAddColumn(hl, 2, L"Art", 90);
    lvAddColumn(hl, 3, L"Check-in", 80);
    lvAddColumn(hl, 4, L"Check-out", 90);
    lvAddColumn(hl, 5, L"Dauer", 80);
    lvAddColumn(hl, 6, L"Hinweis", 200);

    // Zeitraum: aktueller Monat
    tm r{};
    time_t now = time(nullptr);
    localtime_s(&r, &now);
    r.tm_mday = 1;
    r.tm_hour = r.tm_min = r.tm_sec = 0;
    r.tm_isdst = -1;
    setDate(IDC_H_FROM, mktime(&r));
    setDate(IDC_H_TO, util::today());

    // Admin-Knöpfe rechts neben den Reitern (nur auf den geschützten Seiten sichtbar)
    button(-1, L"Zugang \x00E4ndern", IDC_ADMIN_CHANGE);
    button(-1, L"Abmelden", IDC_ADMIN_LOGOUT, gFontBold);

    // Reiter-Steuerelement ganz nach hinten, damit es die Seiten nicht übermalt
    SetWindowPos(gTab, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    clearForm();
    setFormEnabled(false);
}

// ------------------------------------------------------------------ Layout

static void place(int id, int x, int y, int w, int h) { MoveWindow(ctl(id), x, y, w, h, FALSE); }

static void layout() {
    RECT rc;
    GetClientRect(gMain, &rc);
    RECT tr = {S(14), headerHeight() + S(10), rc.right - S(14), rc.bottom - statusHeight() - S(10)};
    MoveWindow(gTab, tr.left, tr.top, tr.right - tr.left, tr.bottom - tr.top, FALSE);

    RECT c = {0, 0, tr.right - tr.left, tr.bottom - tr.top};
    TabCtrl_AdjustRect(gTab, FALSE, &c);
    int x = tr.left + c.left + S(12), y = tr.top + c.top + S(12);

    // Admin-Knöpfe in der Reiterleiste rechts
    {
        int bh = std::max(S(22), (int)c.top - S(6)), by = tr.top + std::max(0, ((int)c.top - S(2) - bh) / 2);
        place(IDC_ADMIN_LOGOUT, tr.right - S(4) - S(110), by, S(110), bh);
        place(IDC_ADMIN_CHANGE, tr.right - S(4) - S(110) - S(6) - S(140), by, S(140), bh);
    }
    int w = (c.right - c.left) - S(24), h = (c.bottom - c.top) - S(24);

    // --- Check-in
    {
        int gw = S(320), lw = w - gw - S(24);
        place(IDC_CI_LABEL, x, y, lw, S(22));
        place(IDC_CI_EDIT, x, y + S(26), lw - S(190), S(42));
        place(IDC_CI_BTN, x + lw - S(180), y + S(26), S(180), S(42));
        place(IDC_CI_HINT, x, y + S(74), lw, S(36));
        int gx = x + lw + S(24);
        place(IDC_GUEST_LABEL, gx, y, gw, S(22));
        place(IDC_GUEST_EDIT, gx, y + S(26), gw, S(30));
        place(IDC_GUEST_BTN, gx, y + S(62), S(200), S(32));

        int ly = y + S(122);
        place(IDC_PRESENT_LABEL, x, ly + S(6), w - S(440), S(22));
        place(IDC_CHECKOUT_SEL, x + w - S(430), ly, S(190), S(32));
        place(IDC_CHECKOUT_ALL, x + w - S(230), ly, S(230), S(32));
        place(IDC_PRESENT_LIST, x, ly + S(40), w, h - (ly + S(40) - y));
        lvFit(ctl(IDC_PRESENT_LIST), 0);
    }

    // --- Mitglieder
    {
        int fw = S(390), lw = w - fw - S(20);
        place(IDC_M_SEARCH_LABEL, x, y + S(6), S(50), S(22));
        place(IDC_M_INACTIVE, x + lw - S(460), y + S(4), S(150), S(24));
        place(IDC_M_SEARCH, x + S(54), y + S(2), lw - S(54) - S(470), S(28));
        place(IDC_M_EXPORT, x + lw - S(300), y, S(120), S(32));
        place(IDC_M_NEW, x + lw - S(170), y, S(170), S(32));
        place(IDC_M_LIST, x, y + S(42), lw, h - S(42));
        lvFit(ctl(IDC_M_LIST), 4);

        int fx = x + lw + S(20), half = (fw - S(10)) / 2, fy = y;
        place(IDC_F_TITLE, fx, fy + S(4), fw, S(24));
        fy += S(34);
        auto row2 = [&](int l1, int c1, int l2, int c2) {
            place(l1, fx, fy, half, S(20));
            place(c1, fx, fy + S(20), half, S(28));
            place(l2, fx + half + S(10), fy, half, S(20));
            place(c2, fx + half + S(10), fy + S(20), half, S(28));
            fy += S(56);
        };
        auto row1 = [&](int l, int c, int height = 28) {
            place(l, fx, fy, fw, S(20));
            place(c, fx, fy + S(20), fw, S(height));
            fy += S(28 + height);
        };
        row2(IDC_F_FIRST_L, IDC_F_FIRST, IDC_F_LAST_L, IDC_F_LAST);
        row2(IDC_F_NUMBER_L, IDC_F_NUMBER, IDC_F_JOIN_L, IDC_F_JOIN);
        place(IDC_F_TYPE_L, fx, fy, fw, S(20));
        place(IDC_F_TYPE, fx, fy + S(20), fw, S(220));  // Höhe inkl. Aufklappliste
        fy += S(56);
        row1(IDC_F_EMAIL_L, IDC_F_EMAIL);
        row1(IDC_F_PHONE_L, IDC_F_PHONE);
        row1(IDC_F_CARD_L, IDC_F_CARD);
        int notesH = std::max(40, std::min(110, (int)((y + h - fy - S(130)) / gScale)));
        row1(IDC_F_NOTES_L, IDC_F_NOTES, notesH);
        place(IDC_F_ACTIVE, fx, fy, fw, S(24));
        fy += S(34);
        int bw = (fw - S(16)) / 3;
        place(IDC_F_SAVE, fx, fy, bw, S(32));
        place(IDC_F_CANCEL, fx + bw + S(8), fy, bw, S(32));
        place(IDC_F_DELETE, fx + 2 * (bw + S(8)), fy, bw, S(32));
        fy += S(40);
        place(IDC_F_TOGGLE, fx, fy, fw, S(32));
    }

    // --- Verlauf
    {
        int cx = x;
        place(IDC_H_FROM_L, cx, y + S(6), S(30), S(22));
        cx += S(32);
        place(IDC_H_FROM, cx, y + S(2), S(120), S(28));
        cx += S(130);
        place(IDC_H_TO_L, cx, y + S(6), S(26), S(22));
        cx += S(28);
        place(IDC_H_TO, cx, y + S(2), S(120), S(28));
        cx += S(136);
        place(IDC_H_TODAY, cx, y, S(70), S(32));
        cx += S(76);
        place(IDC_H_MONTH, cx, y, S(110), S(32));
        cx += S(116);
        place(IDC_H_YEAR, cx, y, S(100), S(32));
        cx += S(116);
        place(IDC_H_SEARCH_L, cx, y + S(6), S(48), S(22));
        cx += S(50);
        int sw = std::max(S(100), x + w - S(130) - cx);
        place(IDC_H_SEARCH, cx, y + S(2), sw, S(28));
        place(IDC_H_EXPORT, x + w - S(120), y, S(120), S(32));
        place(IDC_H_STATS, x, y + S(48), w, S(28));
        place(IDC_H_LIST, x, y + S(86), w, h - S(86));
        lvFit(ctl(IDC_H_LIST), 1);
    }

    InvalidateRect(gMain, nullptr, FALSE);
    RedrawWindow(gMain, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_ERASE);
}

static void showPage(int page) {
    gCurrentPage = page;
    for (int p = 0; p < 3; ++p)
        for (HWND h : gPage[p]) ShowWindow(h, p == page ? SW_SHOW : SW_HIDE);
    for (int id : {IDC_ADMIN_CHANGE, IDC_ADMIN_LOGOUT}) ShowWindow(ctl(id), page != 0 ? SW_SHOW : SW_HIDE);
    if (page == 0) SetFocus(ctl(IDC_CI_EDIT));
    if (page == 2) refreshHistory();
}

// ------------------------------------------------------------------ Start

static HFONT makeFont(int px, int weight) {
    return CreateFontW(-S(px), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
}

static void loadData() {
    std::wstring err;
    if (!gStore.load(err)) {
        MessageBoxW(gMain,
                    (L"Die Daten konnten nicht geladen werden:\n" + err + L"\n\nDatei: " + gStore.file +
                     L"\nSicherungen liegen im Unterordner \x201E" L"backups\x201C.")
                        .c_str(),
                    L"Fehler beim Laden", MB_OK | MB_ICONERROR);
    }

    // Vergessen auszuchecken? Offene Besuche früherer Tage werden um 23:59 geschlossen.
    time_t t0 = util::today();
    int stale = 0;
    for (auto& v : gStore.visits) {
        if (v.present() && v.checkIn < t0) {
            v.checkOut = addDays(util::startOfDay(v.checkIn), 1) - 60;
            v.autoOut = true;
            ++stale;
        }
    }
    if (stale) {
        setStatus(std::to_wstring(stale) + L" offene Besuche vom Vortag wurden automatisch ausgecheckt.", ST_INFO);
        persist();
    } else {
        refreshAll();
    }
}

static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            gMain = h;
            createControls();
            SetTimer(h, 1, 1000, nullptr);
            return 0;

        case WM_SIZE:
            if (gTab) layout();
            return 0;

        case WM_GETMINMAXINFO: {
            auto mmi = (MINMAXINFO*)lp;
            mmi->ptMinTrackSize.x = S(1060);
            mmi->ptMinTrackSize.y = S(760);
            return 0;
        }

        case WM_TIMER:
            invalidateHeader();
            if (gCurrentPage == 0) updatePresentDurations();
            checkAdminIdle();
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT rc;
            GetClientRect(h, &rc);
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
            HGDIOBJ old = SelectObject(mem, bmp);
            paintWindow(mem, rc);
            BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(h, &ps);
            return 0;
        }

        case WM_CTLCOLORSTATIC: {
            HDC dc = (HDC)wp;
            int id = GetDlgCtrlID((HWND)lp);
            SetBkColor(dc, RGB(255, 255, 255));
            SetTextColor(dc, id == IDC_CI_HINT ? kMuted : id == IDC_H_STATS ? RGB(0x13, 0x65, 0x71) : RGB(0x1F, 0x2A, 0x36));
            return (LRESULT)gWhiteBrush;
        }

        case WM_COMMAND: {
            int id = LOWORD(wp), code = HIWORD(wp);
            switch (id) {
                case IDC_CI_BTN: doCheckIn(); break;
                case IDC_GUEST_BTN: doGuestCheckIn(); break;
                case IDC_CHECKOUT_SEL: doCheckOutSelected(); break;
                case IDC_CHECKOUT_ALL: doCheckOutAll(); break;
                case IDC_M_NEW: doNewMember(); break;
                case IDC_M_EXPORT: doExportMembers(); break;
                case IDC_M_INACTIVE: refreshMembers(); break;
                case IDC_M_SEARCH: if (code == EN_CHANGE) refreshMembers(); break;
                case IDC_F_SAVE: doSaveMember(); break;
                case IDC_F_CANCEL: doCancelEdit(); break;
                case IDC_F_DELETE: doDeleteMember(); break;
                case IDC_F_TOGGLE: doToggleSelectedMember(); break;
                case IDC_H_SEARCH: if (code == EN_CHANGE) refreshHistory(); break;
                case IDC_H_EXPORT: doExportHistory(); break;
                case IDC_ADMIN_CHANGE: doChangeAdmin(); break;
                case IDC_ADMIN_LOGOUT: lockAdmin(L"Abgemeldet. Der Admin-Bereich ist gesperrt."); break;
                case IDC_H_TODAY: setHistoryRange(util::today(), util::today()); break;
                case IDC_H_MONTH: {
                    tm r{};
                    time_t now = time(nullptr);
                    localtime_s(&r, &now);
                    r.tm_mday = 1;
                    r.tm_hour = r.tm_min = r.tm_sec = 0;
                    r.tm_isdst = -1;
                    time_t first = mktime(&r);
                    r.tm_mon += 1;
                    r.tm_isdst = -1;
                    setHistoryRange(first, addDays(mktime(&r), -1));
                    break;
                }
                case IDC_H_YEAR: {
                    tm r{};
                    time_t now = time(nullptr);
                    localtime_s(&r, &now);
                    tm a{}, b{};
                    a.tm_year = b.tm_year = r.tm_year;
                    a.tm_mday = 1;
                    b.tm_mon = 11;
                    b.tm_mday = 31;
                    a.tm_isdst = b.tm_isdst = -1;
                    setHistoryRange(mktime(&a), mktime(&b));
                    break;
                }
            }
            return 0;
        }

        case WM_NOTIFY: {
            auto nm = (NMHDR*)lp;
            if (nm->idFrom == IDC_TAB && nm->code == TCN_SELCHANGE) {
                int page = TabCtrl_GetCurSel(gTab);
                if (page != 0 && !requireAdmin()) {
                    TabCtrl_SetCurSel(gTab, gCurrentPage);  // Anmeldung abgebrochen: zurück
                    if (gCurrentPage == 0) SetFocus(ctl(IDC_CI_EDIT));
                    return 0;
                }
                showPage(page);
            } else if (nm->idFrom == IDC_M_LIST && nm->code == LVN_ITEMCHANGED && !gSuppressSelection) {
                auto lv = (NMLISTVIEW*)lp;
                if ((lv->uNewState & LVIS_SELECTED) && !(lv->uOldState & LVIS_SELECTED))
                    onMemberSelected((int64_t)lv->lParam);
            } else if (nm->idFrom == IDC_PRESENT_LIST && nm->code == NM_DBLCLK) {
                auto ia = (NMITEMACTIVATE*)lp;
                if (ia->iItem >= 0)
                    if (Visit* v = gStore.findVisit((int64_t)lvParam(ctl(IDC_PRESENT_LIST), ia->iItem))) checkOut(*v);
            } else if (nm->idFrom == IDC_M_LIST && nm->code == NM_DBLCLK) {
                SetFocus(ctl(IDC_F_FIRST));
            } else if ((nm->idFrom == IDC_H_FROM || nm->idFrom == IDC_H_TO) && nm->code == DTN_DATETIMECHANGE) {
                refreshHistory();
            }
            return 0;
        }

        case WM_DESTROY:
            KillTimer(h, 1);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int show) {
    gInst = inst;

    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES | ICC_DATE_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    HDC screen = GetDC(nullptr);
    gScale = GetDeviceCaps(screen, LOGPIXELSY) / 96.0;
    ReleaseDC(nullptr, screen);

    gFont = makeFont(15, FW_NORMAL);
    gFontSmall = makeFont(13, FW_NORMAL);
    gFontBold = makeFont(15, FW_SEMIBOLD);
    gFontBig = makeFont(26, FW_NORMAL);
    gFontTitle = makeFont(22, FW_SEMIBOLD);
    gFontStat = makeFont(17, FW_BOLD);
    gWhiteBrush = CreateSolidBrush(RGB(255, 255, 255));

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.hIconSm = wc.hIcon;
    wc.lpszClassName = L"OpenLabCheckInWindow";
    RegisterClassExW(&wc);

    HWND h = CreateWindowExW(0, wc.lpszClassName, L"OpenLab Augsburg \x2013 Check-in",
                             WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, S(1260), S(840),
                             nullptr, nullptr, inst, nullptr);
    if (!h) return 1;

    layout();
    showPage(0);
    loadData();
    ShowWindow(h, show == SW_SHOWDEFAULT || show == SW_SHOWNORMAL ? SW_SHOWMAXIMIZED : show);
    UpdateWindow(h);
    SetFocus(ctl(IDC_CI_EDIT));

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    return (int)m.wParam;
}
