// ============================================================================
//  DockCalendar — всплывающий календарь у кнопки панели (PowerToys Dock и др.)
//  Win32 + GDI+, без внешних зависимостей. Windows 10 / 11, x64.
//
//  Поведение:
//   * Запуск exe  -> окно появляется РЯДОМ С КУРСОРОМ (под кнопкой, на которую
//     нажали), а не сбоку, как системный календарь Windows.
//   * Повторный запуск (ещё один клик по кнопке) -> окно закрывается (toggle).
//   * Клик мимо окна / Esc -> окно скрывается. Процесс остаётся в трее,
//     поэтому повторные клики открывают календарь мгновенно.
//   * Виды: месяц -> год -> десятилетие (клик по заголовку), стрелки и колесо
//     мыши листают месяцы / годы / десятилетия.
//
//  Ключи запуска:
//   /exit          — завершить уже работающий экземпляр
//   /noautohide    — не скрывать окно при потере фокуса (для отладки)
//   /at=X,Y        — показать окно в заданной точке экрана (для отладки)
// ============================================================================
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <algorithm>


#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#define DWMWA_USE_IMMERSIVE_DARK_MODE_OLD 19

using namespace Gdiplus;

// ---------------------------------------------------------------------------
static const wchar_t* kWndClass   = L"DockCalendarPopup_7C2E91";
static const wchar_t* kMutexName  = L"Local\\DockCalendar_7C2E91_SingleInstance";
static const UINT WM_APP_TOGGLE   = WM_APP + 1;   // второй запуск exe
static const UINT WM_APP_TRAY     = WM_APP + 2;   // события иконки в трее
static const UINT IDM_SHOW        = 101;
static const UINT IDM_EXIT        = 102;

//.hit-зоны
static const int HIT_NONE  = -1;
static const int HIT_TITLE = 1000;
static const int HIT_UP    = 1001;
static const int HIT_DOWN  = 1002;
static const int HIT_TODAY = 1003;
// 0..41 — ячейки сетки

enum class View { Month, Year, Decade };

struct Date { int y, m, d; };

// ---------------------------------------------------------------------------
// Даты: перевод в/из порядкового дня (алгоритм Ховарда Хиннанта, proleptic
// Gregorian) — надёжная арифметика без переполнений SYSTEMTIME.
// ---------------------------------------------------------------------------
static long long DaysFromCivil(long long y, unsigned m, unsigned d) {
    y -= m <= 2;
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long long)doe - 719468;
}
static Date CivilFromDays(long long z) {
    z += 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned)(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const long long y = (long long)yoe + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    const unsigned d = doy - (153 * mp + 2) / 5 + 1;
    const unsigned m = mp + (mp < 10 ? 3 : -9);
    return Date{ (int)(y + (m <= 2)), (int)m, (int)d };
}
// день недели: 0 = воскресенье
static int DayOfWeekSun(const Date& dt) {
    long long days = DaysFromCivil(dt.y, dt.m, dt.d);
    return (int)(((days + 4) % 7 + 7) % 7);
}
static Date Today() {
    SYSTEMTIME st; GetLocalTime(&st);
    return Date{ st.wYear, st.wMonth, st.wDay };
}
static bool SameDate(const Date& a, const Date& b) {
    return a.y == b.y && a.m == b.m && a.d == b.d;
}

// ---------------------------------------------------------------------------
// Локализация (имена месяцев/дней берём из локали пользователя)
// ---------------------------------------------------------------------------
struct Locale {
    std::wstring month[12];       // именительный: Ноябрь
    std::wstring monthGen[12];    // родительный:  ноября
    std::wstring monthAbbr[12];   // краткие:      ноя
    std::wstring dayShort[7];     // 0 = воскресенье: Вс
    int firstDow = 0;             // 0 = понедельник (LOCALE_IFIRSTDAYOFWEEK)
};
static Locale g_loc;

static std::wstring LocaleStr(LCTYPE t) {
    wchar_t buf[256] = L"";
    GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, t, buf, 256);
    return buf;
}
static void CapFirst(std::wstring& s) {
    if (!s.empty()) CharUpperBuffW(&s[0], 1);
}
static void LoadLocale() {
    for (int i = 0; i < 12; ++i) {
        g_loc.month[i]     = LocaleStr((LCTYPE)(LOCALE_SMONTHNAME1 + i));
        g_loc.monthGen[i]  = LocaleStr((LCTYPE)((LOCALE_SMONTHNAME1 + i) | LOCALE_RETURN_GENITIVE_NAMES));
        g_loc.monthAbbr[i] = LocaleStr((LCTYPE)(LOCALE_SABBREVMONTHNAME1 + i));
        if (g_loc.monthGen[i].empty())  g_loc.monthGen[i]  = g_loc.month[i];
        if (g_loc.monthAbbr[i].empty()) g_loc.monthAbbr[i] = g_loc.month[i].substr(0, 3);
        while (!g_loc.monthAbbr[i].empty() && g_loc.monthAbbr[i].back() == L'.')
            g_loc.monthAbbr[i].pop_back();
        CapFirst(g_loc.month[i]); CapFirst(g_loc.monthAbbr[i]);
    }
    // Индексация LOCALE_S*DAYNAME* по документации: 1 = воскресенье.
    // Некоторые среды (Wine) возвращают массивы с понедельника — выравниваем
    // их, определяя реальное имя воскресенья через форматирование даты.
    int sunIdx = 0;
    {
        SYSTEMTIME st{}; st.wYear = 2026; st.wMonth = 11; st.wDay = 15; // воскресенье
        wchar_t sunName[256] = L"";
        if (GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &st, L"dddd", sunName, 256, nullptr) && sunName[0]) {
            for (int i = 0; i < 7; ++i) {
                std::wstring full = LocaleStr((LCTYPE)(LOCALE_SDAYNAME1 + i));
                if (!full.empty() && _wcsicmp(full.c_str(), sunName) == 0) { sunIdx = i; break; }
            }
        }
    }
    for (int i = 0; i < 7; ++i) {
        std::wstring s = LocaleStr((LCTYPE)(LOCALE_SSHORTESTDAYNAME1 + i));
        int wd = (i - sunIdx + 7) % 7;   // 0 = воскресенье
        if (s.empty()) {
            static const wchar_t* fb[7] = { L"Вс", L"Пн", L"Вт", L"Ср", L"Чт", L"Пт", L"Сб" };
            s = fb[wd];
        }
        CapFirst(s);
        g_loc.dayShort[wd] = s;
    }
    std::wstring fd = LocaleStr(LOCALE_IFIRSTDAYOFWEEK);
    g_loc.firstDow = fd.empty() ? 0 : _wtoi(fd.c_str());
    if (g_loc.firstDow < 0 || g_loc.firstDow > 6) g_loc.firstDow = 0;
}
// «вторник, 17 ноября» / «Tuesday, November 17»
static std::wstring LongDate(const Date& d) {
    SYSTEMTIME st{}; st.wYear = (WORD)d.y; st.wMonth = (WORD)d.m; st.wDay = (WORD)d.d;
    st.wDayOfWeek = (WORD)DayOfWeekSun(d);
    wchar_t buf[256] = L"";
    if (GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &st, L"dddd, d MMMM", buf, 256, nullptr) && buf[0])
        return buf;
    return g_loc.dayShort[DayOfWeekSun(d)] + L", " + std::to_wstring(d.d) + L" " + g_loc.monthGen[d.m - 1];
}

// ---------------------------------------------------------------------------
// Состояние приложения
// ---------------------------------------------------------------------------
static HWND    g_hwnd = nullptr;
static View    g_view = View::Month;
static int     g_viewY = 2026, g_viewM = 1;   // отображаемый месяц/год/десятилетие
static Date    g_sel{};                        // выбранный день
static bool    g_visible = false;
static bool    g_autoHide = true;
static bool    g_fixedPos = false;
static POINT   g_fixedPt{};
static int     g_hover = HIT_NONE;
static DWORD   g_lastAutoHide = 0;             // GetTickCount последнего авто-скрытия
static POINT   g_lastAutoHidePt{};             // где был курсор в тот момент
static bool    g_roundedCorners = false;       // Win11: DWM скруглил углы сам
static ULONG_PTR g_gdipToken = 0;
static NOTIFYICONDATAW g_nid{};

// DPI / метрики
static int   g_dpi = 96;
static HFONT g_fDate = nullptr, g_fTitle = nullptr, g_fCell = nullptr,
             g_fCellBold = nullptr, g_fSmall = nullptr, g_fBtn = nullptr;

static int S(int v) { return MulDiv(v, g_dpi, 96); }   // масштаб 96dpi -> текущий

// Цвета темы — переключаются вслед за системной светлой/тёмной темой
static COLORREF C_BG, C_BORDER, C_TEXT, C_TEXT2, C_DIM, C_HOVER,
                C_ACCENT, C_ONACCENT, C_SEPARATOR;
static bool g_light = false;

static void SetPalette(bool light) {
    g_light = light;
    if (light) {                       // светлая тема Windows 11
        C_BG        = RGB(0xF3, 0xF3, 0xF3);
        C_BORDER    = RGB(0xD1, 0xD1, 0xD1);
        C_TEXT      = RGB(0x1B, 0x1B, 0x1B);
        C_TEXT2     = RGB(0x5D, 0x5D, 0x5D);
        C_DIM       = RGB(0xA0, 0xA0, 0xA0);
        C_HOVER     = RGB(0xE9, 0xE9, 0xE9);
        C_ACCENT    = RGB(0x00, 0x78, 0xD4);
        C_ONACCENT  = RGB(0xFF, 0xFF, 0xFF);
        C_SEPARATOR = RGB(0xE5, 0xE5, 0xE5);
    } else {                           // тёмная тема Windows 11
        C_BG        = RGB(0x20, 0x20, 0x20);
        C_BORDER    = RGB(0x45, 0x45, 0x45);
        C_TEXT      = RGB(0xFF, 0xFF, 0xFF);
        C_TEXT2     = RGB(0xA6, 0xA6, 0xA6);
        C_DIM       = RGB(0x6E, 0x6E, 0x6E);
        C_HOVER     = RGB(0x38, 0x38, 0x38);
        C_ACCENT    = RGB(0x4C, 0xC2, 0xFF);
        C_ONACCENT  = RGB(0x1A, 0x1A, 0x1A);
        C_SEPARATOR = RGB(0x3A, 0x3A, 0x3A);
    }
}

// Светлая ли тема приложений в системе (Параметры -> Цвета -> Выбор режима)
static bool SystemIsLight() {
    bool light = true;                 // по умолчанию Windows светлая
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            0, KEY_READ, &k) == ERROR_SUCCESS) {
        DWORD val = 1, sz = sizeof(val), type = 0;
        if (RegQueryValueExW(k, L"AppsUseLightTheme", nullptr, &type,
                (LPBYTE)&val, &sz) == ERROR_SUCCESS && type == REG_DWORD)
            light = (val != 0);
        RegCloseKey(k);
    }
    return light;
}

// Применить тему к рамке DWM (Win11): режим dark/light и цвет рамки
static void ApplyDwmTheme(HWND hwnd) {
    if (!hwnd) return;
    BOOL dark = g_light ? FALSE : TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_OLD, &dark, sizeof(dark));
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    COLORREF border = C_BORDER;
    DwmSetWindowAttribute(hwnd, 34 /*BORDER_COLOR*/, &border, sizeof(border));
}

// Перечитать системную тему и при изменении перекрасить окно
static void RefreshTheme(HWND hwnd) {
    bool light = SystemIsLight();
    if (light != g_light) {
        SetPalette(light);
        ApplyDwmTheme(hwnd);
        if (hwnd) InvalidateRect(hwnd, nullptr, FALSE);
    }
}

static Color GdiColor(COLORREF c, BYTE a = 255) {
    return Color(a, GetRValue(c), GetGValue(c), GetBValue(c));
}

// ---------------------------------------------------------------------------
// Геометрия окна и hit-зоны
// ---------------------------------------------------------------------------
struct HitRegion { RECT rc; int id; };
static std::vector<HitRegion> g_regions;

static int WinW() { return S(320); }
static int WinH() { return S(384); }

struct Layout {
    int pad, topDateY, topDateH;
    int hdrY, hdrH, titleW, upX, downX, btnW;
    int gridX, gridY, gridW, gridH;
    int todayY, todayH, todayW;
};
static Layout Lay() {
    Layout L{};
    L.pad = S(12);
    L.topDateY = S(12); L.topDateH = S(26);
    L.hdrY = S(44);     L.hdrH = S(34);
    L.btnW = S(32);
    L.downX = WinW() - L.pad - L.btnW;
    L.upX   = L.downX - L.btnW - S(2);
    L.titleW = L.upX - L.pad - S(4);
    L.gridX = L.pad;    L.gridY = S(84);
    L.gridW = WinW() - 2 * L.pad;
    L.gridH = S(252);
    L.todayY = WinH() - S(44); L.todayH = S(32); L.todayW = S(96);
    return L;
}

static void AddRegion(int x, int y, int w, int h, int id) {
    g_regions.push_back({ RECT{ x, y, x + w, y + h }, id });
}
static int HitTest(int x, int y) {
    for (const auto& r : g_regions)
        if (x >= r.rc.left && x < r.rc.right && y >= r.rc.top && y < r.rc.bottom)
            return r.id;
    return HIT_NONE;
}

// ---------------------------------------------------------------------------
// Шрифты
// ---------------------------------------------------------------------------
static HFONT MakeFont(int px, int weight) {
    return CreateFontW(-S(px), 0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}
static void RebuildFonts() {
    for (HFONT f : { g_fDate, g_fTitle, g_fCell, g_fCellBold, g_fSmall, g_fBtn })
        if (f) DeleteObject(f);
    g_fDate     = MakeFont(15, FW_SEMIBOLD);
    g_fTitle    = MakeFont(14, FW_SEMIBOLD);
    g_fCell     = MakeFont(14, FW_NORMAL);
    g_fCellBold = MakeFont(14, FW_SEMIBOLD);
    g_fSmall    = MakeFont(12, FW_NORMAL);
    g_fBtn      = MakeFont(13, FW_NORMAL);
}

// ---------------------------------------------------------------------------
// Навигация
// ---------------------------------------------------------------------------
static void ClampYear(int& y) { y = std::max(1601, std::min(9999, y)); }

static void ShiftPeriod(int dir) {           // dir: -1 назад, +1 вперёд
    if (g_view == View::Month) {
        int m = g_viewM + dir, y = g_viewY;
        if (m < 1)  { m = 12; --y; }
        if (m > 12) { m = 1;  ++y; }
        ClampYear(y); g_viewY = y; g_viewM = m;
    } else if (g_view == View::Year) {
        g_viewY += dir; ClampYear(g_viewY);
    } else {
        g_viewY += dir * 10; ClampYear(g_viewY);
    }
}
static void DrillUp() {
    if (g_view == View::Month)      g_view = View::Year;
    else if (g_view == View::Year)  g_view = View::Decade;
}
static void GoToday() {
    Date t = Today();
    g_sel = t; g_viewY = t.y; g_viewM = t.m; g_view = View::Month;
}
static void SelectDay(const Date& d) {
    g_sel = d;
    if (g_view == View::Month && (d.m != g_viewM || d.y != g_viewY)) {
        g_viewM = d.m; g_viewY = d.y;
    }
}
static void MoveSelection(int days) {        // стрелки клавиатуры в виде «месяц»
    Date d = CivilFromDays(DaysFromCivil(g_sel.y, g_sel.m, g_sel.d) + days);
    if (d.y < 1601 || d.y > 9999) return;
    SelectDay(d);
}

static std::wstring Title() {
    if (g_view == View::Month)
        return g_loc.month[g_viewM - 1] + L" " + std::to_wstring(g_viewY);
    if (g_view == View::Year)
        return std::to_wstring(g_viewY);
    int base = (g_viewY / 10) * 10;
    return std::to_wstring(base) + L" – " + std::to_wstring(base + 9);
}

// ---------------------------------------------------------------------------
// Отрисовка
// ---------------------------------------------------------------------------
static void FillRoundRect(Graphics& g, const RECT& rc, REAL radius, COLORREF c) {
    GraphicsPath path;
    REAL x = (REAL)rc.left, y = (REAL)rc.top,
         w = (REAL)(rc.right - rc.left), h = (REAL)(rc.bottom - rc.top),
         d = radius * 2;
    if (d > w) d = w;
    if (d > h) d = h;
    path.AddArc(x, y, d, d, 180, 90);
    path.AddArc(x + w - d, y, d, d, 270, 90);
    path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    path.AddArc(x, y + h - d, d, d, 90, 90);
    path.CloseFigure();
    SolidBrush br(GdiColor(c));
    g.FillPath(&br, &path);
}
static void FillCircle(Graphics& g, int cx, int cy, int diam, COLORREF c) {
    SolidBrush br(GdiColor(c));
    g.FillEllipse(&br, cx - diam / 2, cy - diam / 2, diam, diam);
}
static void RingCircle(Graphics& g, int cx, int cy, int diam, COLORREF c, float width) {
    Pen pen(GdiColor(c), width);
    g.DrawEllipse(&pen, (REAL)(cx - diam / 2), (REAL)(cy - diam / 2), (REAL)diam, (REAL)diam);
}
static void Text(HDC dc, const std::wstring& s, RECT rc, HFONT font,
                 COLORREF color, UINT fmt) {
    HGDIOBJ old = SelectObject(dc, font);
    SetTextColor(dc, color);
    SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, s.c_str(), (int)s.size(), &rc, fmt | DT_NOPREFIX);
    SelectObject(dc, old);
}
static void Chevron(Graphics& g, int cx, int cy, bool up, COLORREF c) {
    int w = S(5), h = S(3);
    PointF pts[3];
    if (up) { pts[0] = PointF((REAL)(cx - w), (REAL)(cy + h)); pts[1] = PointF((REAL)(cx + w), (REAL)(cy + h)); pts[2] = PointF((REAL)cx, (REAL)(cy - h)); }
    else    { pts[0] = PointF((REAL)(cx - w), (REAL)(cy - h)); pts[1] = PointF((REAL)(cx + w), (REAL)(cy - h)); pts[2] = PointF((REAL)cx, (REAL)(cy + h)); }
    SolidBrush br(GdiColor(c));
    g.FillPolygon(&br, pts, 3);
}

struct Cell { std::wstring label; Date date; bool dim; };

static std::vector<Cell> BuildCells() {
    std::vector<Cell> cells;
    if (g_view == View::Month) {
        int firstDowSun = (g_loc.firstDow + 1) % 7;
        int dowFirst = DayOfWeekSun(Date{ g_viewY, g_viewM, 1 });
        int offset = (dowFirst - firstDowSun + 7) % 7;
        long long base = DaysFromCivil(g_viewY, g_viewM, 1) - offset;
        for (int i = 0; i < 42; ++i) {
            Date d = CivilFromDays(base + i);
            cells.push_back({ std::to_wstring(d.d), d, d.m != g_viewM || d.y != g_viewY });
        }
    } else if (g_view == View::Year) {
        for (int i = 0; i < 16; ++i) {
            int m = i % 12 + 1, y = g_viewY + i / 12;
            cells.push_back({ g_loc.monthAbbr[m - 1], Date{ y, m, 1 }, i >= 12 });
        }
    } else {
        int base = (g_viewY / 10) * 10;
        for (int i = 0; i < 16; ++i) {
            int y = base + i;
            cells.push_back({ std::to_wstring(y), Date{ y, g_viewM, 1 }, y < base || y > base + 9 });
        }
    }
    return cells;
}

static void Paint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC wndDc = BeginPaint(hwnd, &ps);
    RECT wr; GetClientRect(hwnd, &wr);
    int W = wr.right, H = wr.bottom;
    if (W <= 0 || H <= 0) { EndPaint(hwnd, &ps); return; }

    // двойная буферизация
    HDC dc = CreateCompatibleDC(wndDc);
    HBITMAP bmp = CreateCompatibleBitmap(wndDc, W, H);
    HGDIOBJ oldBmp = SelectObject(dc, bmp);

    Layout L = Lay();
    g_regions.clear();

    {
        Graphics g(dc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(PixelOffsetModeHalf);

        // фон
        SolidBrush bg(GdiColor(C_BG));
        g.FillRectangle(&bg, 0, 0, W, H);
        if (!g_roundedCorners) {                       // Win10: рамка вручную
            Pen border(GdiColor(C_BORDER), (REAL)S(1));
            g.DrawRectangle(&border, 0, 0, W - S(1), H - S(1));
        }

        // верхняя строка: полная дата выбранного дня
        Text(dc, LongDate(g_sel), RECT{ L.pad + S(4), L.topDateY, W - L.pad, L.topDateY + L.topDateH },
             g_fDate, C_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // заголовок-кнопка (месяц/год/десятилетие)
        RECT titleRc{ L.pad, L.hdrY, L.pad + L.titleW, L.hdrY + L.hdrH };
        if (g_hover == HIT_TITLE && g_view != View::Decade)
            FillRoundRect(g, titleRc, (REAL)S(6), C_HOVER);
        Text(dc, Title(), RECT{ titleRc.left + S(4), titleRc.top, titleRc.right, titleRc.bottom },
             g_fTitle, C_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        if (g_view != View::Decade) AddRegion(titleRc.left, titleRc.top, L.titleW, L.hdrH, HIT_TITLE);

        // стрелки вверх/вниз
        RECT upRc{ L.upX, L.hdrY, L.upX + L.btnW, L.hdrY + L.hdrH };
        RECT downRc{ L.downX, L.hdrY, L.downX + L.btnW, L.hdrY + L.hdrH };
        if (g_hover == HIT_UP)   FillRoundRect(g, upRc,   (REAL)S(6), C_HOVER);
        if (g_hover == HIT_DOWN) FillRoundRect(g, downRc, (REAL)S(6), C_HOVER);
        Chevron(g, (upRc.left + upRc.right) / 2,   (upRc.top + upRc.bottom) / 2,   true,  C_TEXT2);
        Chevron(g, (downRc.left + downRc.right) / 2, (downRc.top + downRc.bottom) / 2, false, C_TEXT2);
        AddRegion(upRc.left, upRc.top, L.btnW, L.hdrH, HIT_UP);
        AddRegion(downRc.left, downRc.top, L.btnW, L.hdrH, HIT_DOWN);

        Date today = Today();
        auto cells = BuildCells();

        if (g_view == View::Month) {
            // строка дней недели
            int cw = L.gridW / 7;
            int firstDowSun = (g_loc.firstDow + 1) % 7;
            for (int i = 0; i < 7; ++i) {
                int dow = (firstDowSun + i) % 7;
                RECT rc{ L.gridX + i * cw, L.gridY, L.gridX + (i + 1) * cw, L.gridY + S(24) };
                Text(dc, g_loc.dayShort[dow], rc, g_fSmall, C_TEXT2,
                     DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
            int gy = L.gridY + S(24);
            int ch = (L.gridH - S(24)) / 6;
            int diam = std::min(cw, ch) - S(6);
            for (int i = 0; i < 42; ++i) {
                int cx = L.gridX + (i % 7) * cw + cw / 2;
                int cy = gy + (i / 7) * ch + ch / 2;
                const Cell& c = cells[i];
                bool isToday = SameDate(c.date, today) && !c.dim;
                bool isSel   = SameDate(c.date, g_sel);
                if (g_hover == i && !isToday)
                    FillCircle(g, cx, cy, diam, C_HOVER);
                if (isToday)
                    FillCircle(g, cx, cy, diam, C_ACCENT);
                else if (isSel)
                    RingCircle(g, cx, cy, diam - S(2), C_ACCENT, (float)S(2) * 0.75f + 0.5f);
                COLORREF tc = isToday ? C_ONACCENT : (c.dim ? C_DIM : C_TEXT);
                if (!c.dim && (DayOfWeekSun(c.date) == 0 || DayOfWeekSun(c.date) == 6) && !isToday)
                    tc = C_TEXT2;                       // выходные чуть приглушены
                RECT rc{ cx - cw / 2, cy - ch / 2, cx + cw / 2, cy + ch / 2 };
                Text(dc, c.label, rc, isToday ? g_fCellBold : g_fCell, tc,
                     DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                AddRegion(rc.left, rc.top, cw, ch, i);
            }
        } else {
            // год / десятилетие: сетка 4x4
            int cw = L.gridW / 4, ch = L.gridH / 4;
            for (int i = 0; i < 16; ++i) {
                int col = i % 4, row = i / 4;
                RECT rc{ L.gridX + col * cw + S(3), L.gridY + row * ch + S(3),
                         L.gridX + (col + 1) * cw - S(3), L.gridY + (row + 1) * ch - S(3) };
                const Cell& c = cells[i];
                bool cur = false, sel = false;
                if (g_view == View::Year) {
                    cur = !c.dim && c.date.y == today.y && c.date.m == today.m;
                    sel = c.date.y == g_sel.y && c.date.m == g_sel.m;
                } else {
                    cur = c.date.y == today.y;
                    sel = c.date.y == g_sel.y;
                }
                if (g_hover == i && !cur)
                    FillRoundRect(g, rc, (REAL)S(8), C_HOVER);
                if (cur)
                    FillRoundRect(g, rc, (REAL)S(8), C_ACCENT);
                else if (sel) {
                    Pen pen(GdiColor(C_ACCENT), (float)S(2) * 0.75f + 0.5f);
                    GraphicsPath p; REAL d = (REAL)S(16);
                    REAL x = (REAL)rc.left, y = (REAL)rc.top,
                         w = (REAL)(rc.right - rc.left), h = (REAL)(rc.bottom - rc.top);
                    p.AddArc(x, y, d, d, 180, 90); p.AddArc(x + w - d, y, d, d, 270, 90);
                    p.AddArc(x + w - d, y + h - d, d, d, 0, 90); p.AddArc(x, y + h - d, d, d, 90, 90);
                    p.CloseFigure(); g.DrawPath(&pen, &p);
                }
                COLORREF tc = cur ? C_ONACCENT : (c.dim ? C_DIM : C_TEXT);
                Text(dc, c.label, rc, cur ? g_fCellBold : g_fCell, tc,
                     DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                AddRegion(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, i);
            }
        }

        // разделитель и кнопка «Сегодня»
        Pen sep(GdiColor(C_SEPARATOR), 1.0f);
        g.DrawLine(&sep, L.pad, L.todayY - S(4), W - L.pad, L.todayY - S(4));
        RECT todayRc{ L.pad, L.todayY, L.pad + L.todayW, L.todayY + L.todayH };
        if (g_hover == HIT_TODAY)
            FillRoundRect(g, todayRc, (REAL)S(6), C_HOVER);
        Text(dc, L"Сегодня", RECT{ todayRc.left + S(8), todayRc.top, todayRc.right, todayRc.bottom },
             g_fBtn, C_ACCENT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        AddRegion(todayRc.left, todayRc.top, L.todayW, L.todayH, HIT_TODAY);
    }

    BitBlt(wndDc, 0, 0, W, H, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(dc);
    EndPaint(hwnd, &ps);
}

// ---------------------------------------------------------------------------
// Показ / скрытие у курсора
// ---------------------------------------------------------------------------
static int DpiForPoint(POINT pt) {
    HMODULE shcore = LoadLibraryW(L"shcore.dll");
    if (shcore) {
        typedef HRESULT(WINAPI* Fn)(HMONITOR, int, UINT*, UINT*);
        Fn fn = (Fn)GetProcAddress(shcore, "GetDpiForMonitor");
        if (fn) {
            HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
            UINT dx = 96, dy = 96;
            if (SUCCEEDED(fn(mon, 0 /*MDT_EFFECTIVE_DPI*/, &dx, &dy))) {
                FreeLibrary(shcore);
                return (int)dx;
            }
        }
        FreeLibrary(shcore);
    }
    HDC dc = GetDC(nullptr);
    int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(nullptr, dc);
    return dpi;
}

static void ShowAtCursor() {
    POINT pt; GetCursorPos(&pt);
    if (g_fixedPos) pt = g_fixedPt;

    g_dpi = DpiForPoint(pt);
    RebuildFonts();
    RefreshTheme(g_hwnd);              // на случай смены темы, пока окно скрыто

    int W = WinW(), H = WinH();
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{ sizeof(mi) };
    GetMonitorInfoW(mon, &mi);
    const RECT& mr = mi.rcMonitor;
    int margin = S(8);

    int x = pt.x - W / 2;                 // по центру кнопки, под которой курсор
    int y = pt.y + S(18);                 // чуть ниже курсора (под панелью)
    if (y + H > mr.bottom - margin)       // не влезает снизу — показываем сверху
        y = pt.y - H - S(18);
    x = std::max((int)mr.left + margin, std::min(x, (int)mr.right - W - margin));
    y = std::max((int)mr.top + margin,  std::min(y, (int)mr.bottom - H - margin));

    Date t = Today();
    g_sel = t; g_viewY = t.y; g_viewM = t.m; g_view = View::Month;
    g_hover = HIT_NONE;

    SetWindowPos(g_hwnd, HWND_TOPMOST, x, y, W, H, SWP_NOACTIVATE);
    ShowWindow(g_hwnd, SW_SHOW);
    SetForegroundWindow(g_hwnd);
    SetFocus(g_hwnd);
    g_visible = true;
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

static void HidePopup(bool byDeactivate) {
    if (!g_visible) return;
    ShowWindow(g_hwnd, SW_HIDE);
    g_visible = false;
    if (byDeactivate) {
        g_lastAutoHide = GetTickCount();
        GetCursorPos(&g_lastAutoHidePt);
    }
}

static void TogglePopup() {
    if (g_visible) { HidePopup(false); return; }
    // Если окно только что само скрылось из-за клика по той же кнопке панели
    // (клик сначала уводит фокус, потом запускает exe) — считаем это «закрыть»,
    // а не «открыть заново».
    DWORD now = GetTickCount();
    POINT pt; GetCursorPos(&pt);
    int dx = pt.x - g_lastAutoHidePt.x, dy = pt.y - g_lastAutoHidePt.y;
    if (g_lastAutoHide && now - g_lastAutoHide < 700 &&
        dx * dx + dy * dy < S(64) * S(64)) {
        g_lastAutoHide = 0;
        return;
    }
    ShowAtCursor();
}

// ---------------------------------------------------------------------------
// Иконка в трее
// ---------------------------------------------------------------------------
static void TrayAdd(HWND hwnd) {
    g_nid = {};
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_APP_TRAY;
    g_nid.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1));
    wcscpy_s(g_nid.szTip, L"Dock Calendar — календарь у панели");
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}
static void TrayRemove() { Shell_NotifyIconW(NIM_DELETE, &g_nid); }

static void TrayMenu(HWND hwnd) {
    POINT pt; GetCursorPos(&pt);
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, IDM_SHOW, L"Показать календарь");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_EXIT, L"Выход");
    SetForegroundWindow(hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(menu);
}

// ---------------------------------------------------------------------------
// Оконная процедура
// ---------------------------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_NCCALCSIZE:
        if (wp) return 0;                 // клиент на всё окно (тень DWM остаётся)
        break;
    case WM_NCHITTEST:
        return HTCLIENT;                  // без resize-рамок
    case WM_NCACTIVATE:
        return TRUE;
    case WM_NCPAINT:
        return 0;                         // рамку рисуем сами (или DWM)
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        Paint(hwnd);
        return 0;
    case WM_DPICHANGED: {
        g_dpi = HIWORD(wp);
        RebuildFonts();
        const RECT* r = (const RECT*)lp;
        SetWindowPos(hwnd, nullptr, r->left, r->top, WinW(), WinH(),
                     SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        int hit = HitTest(x, y);
        if (hit != g_hover) {
            g_hover = hit;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        SetCursor(LoadCursorW(nullptr, hit != HIT_NONE ? IDC_HAND : IDC_ARROW));
        TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE:
        if (g_hover != HIT_NONE) { g_hover = HIT_NONE; InvalidateRect(hwnd, nullptr, FALSE); }
        return 0;
    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        int hit = HitTest(x, y);
        if (hit == HIT_TITLE)      { DrillUp(); }
        else if (hit == HIT_UP)    { ShiftPeriod(-1); }
        else if (hit == HIT_DOWN)  { ShiftPeriod(+1); }
        else if (hit == HIT_TODAY) { GoToday(); }
        else if (hit >= 0 && hit < 42) {
            auto cells = BuildCells();
            if (hit < (int)cells.size()) {
                const Cell& c = cells[hit];
                if (g_view == View::Month) {
                    SelectDay(c.date);
                } else if (g_view == View::Year) {
                    g_viewY = c.date.y; g_viewM = c.date.m; g_view = View::Month;
                } else {
                    g_viewY = c.date.y; g_view = View::Year;
                }
            }
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wp);
        ShiftPeriod(delta > 0 ? -1 : +1);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_KEYDOWN:
        switch (wp) {
        case VK_ESCAPE: HidePopup(false); return 0;
        case VK_PRIOR:  ShiftPeriod(-1); break;
        case VK_NEXT:   ShiftPeriod(+1); break;
        case VK_HOME:   GoToday(); break;
        case VK_RETURN: DrillUp(); break;
        case VK_LEFT:
            if (g_view == View::Month) MoveSelection(-1); else ShiftPeriod(-1);
            break;
        case VK_RIGHT:
            if (g_view == View::Month) MoveSelection(+1); else ShiftPeriod(+1);
            break;
        case VK_UP:
            if (g_view == View::Month) MoveSelection(-7); else ShiftPeriod(-1);
            break;
        case VK_DOWN:
            if (g_view == View::Month) MoveSelection(+7); else ShiftPeriod(+1);
            break;
        default: return 0;
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_SETTINGCHANGE:
        // смена светлой/тёмной темы приходит как сообщение "ImmersiveColorSet"
        if (lp && !IsBadStringPtrW((LPCWSTR)lp, 64) &&
            wcscmp((LPCWSTR)lp, L"ImmersiveColorSet") == 0)
            RefreshTheme(hwnd);
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE && g_autoHide)
            HidePopup(true);
        return 0;
    case WM_APP_TOGGLE:
        TogglePopup();
        return 0;
    case WM_APP_TRAY:
        if (lp == WM_LBUTTONUP || lp == WM_LBUTTONDBLCLK) TogglePopup();
        else if (lp == WM_RBUTTONUP) TrayMenu(hwnd);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wp) == IDM_SHOW) ShowAtCursor();
        else if (LOWORD(wp) == IDM_EXIT) DestroyWindow(hwnd);
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        TrayRemove();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------------------
static void EnableDpiAwareness() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    typedef BOOL(WINAPI* Fn)(DPI_AWARENESS_CONTEXT);
    Fn fn = (Fn)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
    if (fn && fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) return;
    SetProcessDPIAware();
}

static void ApplyFrameEffects(HWND hwnd) {
    // Win11: скруглённые углы; на Win10 вызов просто не сработает
    int pref = 2; // DWMWCP_ROUND
    g_roundedCorners = SUCCEEDED(DwmSetWindowAttribute(hwnd, 33 /*WINDOW_CORNER_PREFERENCE*/, &pref, sizeof(pref)));
    ApplyDwmTheme(hwnd);   // режим dark/light + цвет рамки по текущей теме
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    // ---- разбор аргументов ----
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool exitMode = false;
    for (int i = 1; i < argc; ++i) {
        std::wstring a = argv[i];
        if (_wcsicmp(a.c_str(), L"/exit") == 0 || _wcsicmp(a.c_str(), L"-exit") == 0)
            exitMode = true;
        else if (_wcsicmp(a.c_str(), L"/noautohide") == 0)
            g_autoHide = false;
        else if (_wcsnicmp(a.c_str(), L"/at=", 4) == 0) {
            int x = 0, y = 0;
            if (swscanf_s(a.c_str() + 4, L"%d,%d", &x, &y) == 2) {
                g_fixedPos = true; g_fixedPt = POINT{ x, y };
            }
        }
    }
    if (argv) LocalFree(argv);

    // ---- единственный экземпляр ----
    HANDLE mutex = CreateMutexW(nullptr, TRUE, kMutexName);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(kWndClass, nullptr);
        if (existing) {
            // разрешаем уже работающему экземпляру вывести окно на передний план
            AllowSetForegroundWindow(ASFW_ANY);
            if (exitMode) PostMessageW(existing, WM_CLOSE, 0, 0);
            else          PostMessageW(existing, WM_APP_TOGGLE, 0, 0);
        }
        return 0;
    }
    if (exitMode) return 0;

    EnableDpiAwareness();
    LoadLocale();
    SetPalette(SystemIsLight());       // стартовая тема — как в системе

    GdiplusStartupInput gdiInput;
    GdiplusStartup(&g_gdipToken, &gdiInput, nullptr);

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWndClass;
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(1));
    RegisterClassW(&wc);

    g_dpi = DpiForPoint(POINT{ 0, 0 });
    RebuildFonts();

    // WS_THICKFRAME нужен только ради тени DWM; рамки убираем в WM_NCCALCSIZE
    g_hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        kWndClass, L"Dock Calendar",
        WS_POPUP | WS_THICKFRAME,
        0, 0, WinW(), WinH(),
        nullptr, nullptr, hInst, nullptr);
    if (!g_hwnd) return 1;

    ApplyFrameEffects(g_hwnd);
    TrayAdd(g_hwnd);
    ShowAtCursor();

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    for (HFONT f : { g_fDate, g_fTitle, g_fCell, g_fCellBold, g_fSmall, g_fBtn })
        if (f) DeleteObject(f);
    GdiplusShutdown(g_gdipToken);
    if (mutex) { ReleaseMutex(mutex); CloseHandle(mutex); }
    return (int)msg.wParam;
}
