
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
#include <cwctype>
#include <cwchar>
#include <cmath>
#include <string>

#include <vector>
#include <algorithm>

static const double PI = 3.14159265358979323846;

static HINSTANCE hInst;
static HWND hMain, hTitle, hIn, hOut, hModeLbl;
static HWND hExtra[25];
static HWND hTabs[3];
static HFONT g_btnFont, g_dispFont, g_titleFont, g_smallFont;
static HFONT g_scaledBtnFont, g_scaledDispFont, g_scaledTitleFont, g_scaledSmallFont;
static HBRUSH hSurface, hWhite;
static const COLORREF COLOR_SURFACE = RGB(232, 235, 238);
static const COLORREF COLOR_TEXT = RGB(42, 49, 57);
static const COLORREF COLOR_MUTED = RGB(91, 101, 108);
static const COLORREF COLOR_BUTTON = RGB(246, 247, 248);
static const COLORREF COLOR_BUTTON_DOWN = RGB(214, 235, 223);
static const COLORREF COLOR_BORDER = RGB(190, 197, 201);
static const COLORREF COLOR_ACTIVE = RGB(41, 115, 82);
static const COLORREF COLOR_ACTION = RGB(34, 139, 91);
static double g_ans = 0;
static WNDPROC oldEditProc;
static int g_activePage;

struct MainChildLayout { HWND window; RECT bounds; HFONT font; };
static std::vector<MainChildLayout> g_mainChildren;
static std::vector<HWND> g_scientificControls, g_infoControls, g_unitControls, g_basicControls;

enum {
    ID_EXTRA = 1000, ID_KEY = 2000, ID_EQ = 2100,
    ID_TAB_SCI = 2200, ID_TAB_INFO, ID_TAB_UNIT, ID_INFO = 2300
};

struct Err { std::wstring m; };

static std::wstring F(double v) {
    if (v == 0) v = 0.0;
    wchar_t b[64];
    if (std::isfinite(v) && std::fabs(v) < 1e15 && v == std::floor(v))
        _snwprintf(b, 64, L"%.0f", v);
    else
        _snwprintf(b, 64, L"%.10g", v);
    b[63] = 0;
    return b;
}

static std::wstring getText(HWND h) {
    int n = GetWindowTextLengthW(h);
    std::wstring s(n + 1, L'\0');
    GetWindowTextW(h, &s[0], n + 1);
    s.resize(n);
    return s;
}

static BOOL CALLBACK collectMainChild(HWND child, LPARAM) {
    RECT bounds;
    if (!GetWindowRect(child, &bounds)) return TRUE;
    MapWindowPoints(HWND_DESKTOP, hMain, (POINT*)&bounds, 2);
    g_mainChildren.push_back({ child, bounds, (HFONT)SendMessageW(child, WM_GETFONT, 0, 0) });
    return TRUE;
}

static HFONT makeScaledFont(int height, int weight, const wchar_t* face, double scale) {
    return CreateFontW((int)-std::lround(height * scale), 0, 0, 0, weight,
                       0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, face);
}

static void layoutMain(int width, int height) {
    if (width <= 0 || height <= 0 || g_mainChildren.empty()) return;
    const double sx = width / 780.0;
    const double sy = height / 620.0;
    const double fontScale = std::min(sx, sy);

    HFONT newBtn = makeScaledFont(17, FW_BOLD, L"Segoe UI", fontScale);
    HFONT newDisp = makeScaledFont(26, FW_NORMAL, L"Consolas", fontScale);
    HFONT newTitle = makeScaledFont(20, FW_BOLD, L"Segoe UI", fontScale);
    HFONT newSmall = makeScaledFont(15, FW_NORMAL, L"Segoe UI", fontScale);

    for (const MainChildLayout& child : g_mainChildren) {
        const RECT& r = child.bounds;
        MoveWindow(child.window,
                   (int)std::lround(r.left * sx), (int)std::lround(r.top * sy),
                   (int)std::lround((r.right - r.left) * sx),
                   (int)std::lround((r.bottom - r.top) * sy), TRUE);
        HFONT font = child.font == g_btnFont ? newBtn
                   : child.font == g_dispFont ? newDisp
                   : child.font == g_titleFont ? newTitle
                   : child.font == g_smallFont ? newSmall : NULL;
        if (font) SendMessageW(child.window, WM_SETFONT, (WPARAM)font, TRUE);
    }

    HFONT oldBtn = g_scaledBtnFont, oldDisp = g_scaledDispFont;
    HFONT oldTitle = g_scaledTitleFont, oldSmall = g_scaledSmallFont;
    g_scaledBtnFont = newBtn;
    g_scaledDispFont = newDisp;
    g_scaledTitleFont = newTitle;
    g_scaledSmallFont = newSmall;
    if (oldBtn) DeleteObject(oldBtn);
    if (oldDisp) DeleteObject(oldDisp);
    if (oldTitle) DeleteObject(oldTitle);
    if (oldSmall) DeleteObject(oldSmall);
}

struct Parser {
    std::wstring s; size_t p = 0;
    explicit Parser(std::wstring t) : s(std::move(t)) {}

    void ws() { while (p < s.size() && iswspace(s[p])) p++; }
    bool eat(wchar_t c) { ws(); if (p < s.size() && s[p] == c) { p++; return true; } return false; }

    double parse() {
        double v = expr(); ws();
        if (p < s.size()) throw Err{ std::wstring(L"Unexpected '") + s[p] + L"'" };
        if (!std::isfinite(v)) throw Err{ L"Math error" };
        return v;
    }
    double expr() {
        double v = term();
        for (;;) {
            if (eat(L'+')) v += term();
            else if (eat(L'-')) v -= term();
            else return v;
        }
    }
    double term() {
        double v = unary();
        for (;;) {
            if (eat(L'*')) v *= unary();
            else if (eat(L'/')) {
                double d = unary();
                if (d == 0) throw Err{ L"Divide by zero" };
                v /= d;
            } else return v;
        }
    }
    double unary() {
        if (eat(L'-')) return -unary();
        if (eat(L'+')) return unary();
        return power();
    }
    double power() {
        double b = postfix();
        if (eat(L'^')) return std::pow(b, unary());
        return b;
    }
    double postfix() {
        double v = primary();
        while (eat(L'!')) v = fact(v);
        return v;
    }
    static double fact(double a) {
        if (a < 0 || a != std::floor(a) || a > 170) throw Err{ L"n! needs integer 0..170" };
        double r = 1; for (int i = 2; i <= (int)a; i++) r *= i;
        return r;
    }
    double primary() {
        ws();
        if (p >= s.size()) throw Err{ L"Unexpected end" };
        wchar_t c = s[p];
        if (c == L'(') {
            p++; double v = expr();
            if (!eat(L')')) throw Err{ L"Missing )" };
            return v;
        }
        if (iswdigit(c) || c == L'.') {
            const wchar_t* b = s.c_str() + p; wchar_t* e;
            double v = wcstod(b, &e);
            if (e == b) throw Err{ L"Bad number" };
            p += (size_t)(e - b);
            return v;
        }
        if (iswalpha(c)) {
            size_t st = p;
            while (p < s.size() && iswalnum(s[p])) p++;
            std::wstring n = s.substr(st, p - st);
            for (auto& ch : n) ch = (wchar_t)towlower(ch);
            if (n == L"pi") return PI;
            if (n == L"e") return std::exp(1.0);
            if (n == L"ans") return g_ans;
            if (!eat(L'(')) throw Err{ L"Unknown name: " + n };
            double a = expr();
            if (!eat(L')')) throw Err{ L"Missing )" };
            return func(n, a);
        }
        throw Err{ std::wstring(L"Unexpected '") + c + L"'" };
    }
    static double func(const std::wstring& n, double a) {
        const double k = PI / 180.0;
        if (n == L"sin") return std::sin(a * k);
        if (n == L"cos") return std::cos(a * k);
        if (n == L"tan") return std::tan(a * k);
        if (n == L"asin") return std::asin(a) / k;
        if (n == L"acos") return std::acos(a) / k;
        if (n == L"atan") return std::atan(a) / k;
        if (n == L"sinh") return std::sinh(a);
        if (n == L"cosh") return std::cosh(a);
        if (n == L"tanh") return std::tanh(a);
        if (n == L"ln") { if (a <= 0) throw Err{ L"ln needs x > 0" }; return std::log(a); }
        if (n == L"log" || n == L"log10") { if (a <= 0) throw Err{ L"log needs x > 0" }; return std::log10(a); }
        if (n == L"log2") { if (a <= 0) throw Err{ L"log2 needs x > 0" }; return std::log2(a); }
        if (n == L"sqrt") { if (a < 0) throw Err{ L"sqrt of negative" }; return std::sqrt(a); }
        if (n == L"cbrt") return std::cbrt(a);
        if (n == L"exp") return std::exp(a);
        if (n == L"abs") return std::fabs(a);
        if (n == L"ceil") return std::ceil(a);
        if (n == L"inv") { if (a == 0) throw Err{ L"1/x needs x != 0" }; return 1.0 / a; }
        if (n == L"fact") return fact(a);
        throw Err{ L"Unknown function: " + n };
    }
};

static std::vector<double> parseList(const std::wstring& t) {
    std::vector<double> r; int d = 0; std::wstring cur;
    auto flush = [&]() {
        if (cur.find_first_not_of(L" \t") != std::wstring::npos) {
            Parser P(cur); r.push_back(P.parse());
        }
        cur.clear();
    };
    for (wchar_t c : t) {
        if (c == L'(') d++;
        if (c == L')') d--;
        if (c == L',' && d == 0) flush(); else cur += c;
    }
    flush();
    return r;
}

static double entropyNats(const std::vector<double>& v, bool& normalised) {
    if (v.empty()) throw Err{ L"Enter values like 0.5, 0.25, 0.25" };
    double sum = 0;
    for (double x : v) { if (x < 0) throw Err{ L"Values cannot be negative" }; sum += x; }
    if (sum <= 0) throw Err{ L"Sum must be > 0" };
    normalised = std::fabs(sum - 1.0) > 1e-9;
    double H = 0;
    for (double x : v) { double p = x / sum; if (p > 0) H -= p * std::log(p); }
    return H;
}

static void baseInfo(const std::wstring& s, double& lnb, const wchar_t*& name) {
    if (s == L"2")  { lnb = std::log(2.0);  name = L"bits"; }
    else if (s == L"e") { lnb = 1.0;        name = L"nats"; }
    else            { lnb = std::log(10.0); name = L"hartleys"; }
}

static void setOut(const std::wstring& s) { SetWindowTextW(hOut, s.c_str()); }

static void doInfo(const std::wstring& code) {
    std::wstring t = getText(hIn);
    try {
        const double LN2 = std::log(2.0);
        if (code[0] == L'H') {
            double lnb; const wchar_t* nm; baseInfo(code.substr(1), lnb, nm);
            bool norm; double H = entropyNats(parseList(t), norm);
            setOut(L"H = " + F(H / lnb) + L" " + nm + (norm ? L"  (counts normalised)" : L""));
            return;
        }
        if (code[0] == L'I') {
            double lnb; const wchar_t* nm; baseInfo(code.substr(1), lnb, nm);
            auto v = parseList(t);
            if (v.size() != 1) throw Err{ L"Enter one probability p" };
            if (v[0] <= 0 || v[0] > 1) throw Err{ L"p must be in (0, 1]" };
            setOut(L"I(p) = " + F(-std::log(v[0]) / lnb) + L" " + nm);
            return;
        }
        bool norm; auto v = parseList(t);
        double Hb = entropyNats(v, norm) / LN2;
        if (v.size() < 2) throw Err{ L"Need at least 2 outcomes" };
        double eff = Hb / std::log2((double)v.size());
        if (code == L"eff") setOut(L"Efficiency = " + F(eff) + L"  (" + F(eff * 100) + L"%)");
        else                setOut(L"Redundancy = " + F(1 - eff) + L"  (" + F((1 - eff) * 100) + L"%)");
    } catch (Err& e) { setOut(L"Error: " + e.m); }
    catch (...) { setOut(L"Error"); }
}

struct Ex { const wchar_t* label; const wchar_t* ins; };

static const Ex EXTRAS[] = {
    {L"sin", L"sin("}, {L"cos", L"cos("}, {L"tan", L"tan("},
    {L"ln", L"ln("}, {L"log10", L"log10("}, {L"log2", L"log2("},
    {L"\u221A", L"sqrt("}, {L"x\u00B2", L"^2"},
    {L"e\u02E3", L"exp("}, {L"n!", L"!"}, {L"1/x", L"inv("},
    {L"ceil", L"ceil("},
    {L"\u03C0", L"pi"}, {L"e", L"e"}
};
static const int EXTRA_COUNT = (int)(sizeof(EXTRAS) / sizeof(Ex));
static const Ex INFO_TOOLS[] = {
    {L"H bits", L"H2"}, {L"H nats", L"He"}, {L"H hartleys", L"H10"},
    {L"I bits", L"I2"}, {L"I nats", L"Ie"}, {L"I hartleys", L"I10"},
    {L"Efficiency", L"eff"}, {L"Redundancy", L"red"}
};
static const Ex KEYS[21] = {
    {L"C", L"#C"}, {L"(", L"("}, {L")", L")"}, {L"DEL", L"#DEL"},
    {L"7", L"7"}, {L"8", L"8"}, {L"9", L"9"}, {L"\u00F7", L"/"},
    {L"4", L"4"}, {L"5", L"5"}, {L"6", L"6"}, {L"\u00D7", L"*"},
    {L"1", L"1"}, {L"2", L"2"}, {L"3", L"3"}, {L"-", L"-"},
    {L"0", L"0"}, {L".", L"."}, {L"^", L"^"}, {L"+", L"+"}, {L",", L","}
};

static void updateStatus() {
    std::wstring s = L"Select SCI, INFO or UNIT";
    SetWindowTextW(hModeLbl, s.c_str());
}

static void doEquals() {
    std::wstring t = getText(hIn);
    if (t.empty()) return;
    try {
        Parser P(t); double v = P.parse();
        g_ans = v; setOut(F(v));
    } catch (Err& e) { setOut(L"Error: " + e.m); }
    catch (...) { setOut(L"Error"); }
}

static void doExtra(int i) {
    if (i < 0 || i >= EXTRA_COUNT) return;
    const Ex& e = EXTRAS[i];
    std::wstring ins = e.ins;
    if (ins.empty()) return;
    if (ins[0] == L'#') { doInfo(ins.substr(1)); return; }
    SendMessageW(hIn, EM_REPLACESEL, TRUE, (LPARAM)ins.c_str());
}

static void doKey(int i) {
    std::wstring ins = KEYS[i].ins;
    if (ins == L"#C")   { SetWindowTextW(hIn, L""); SetWindowTextW(hOut, L""); return; }
    if (ins == L"#DEL") { SendMessageW(hIn, WM_CHAR, VK_BACK, 0); return; }
    SendMessageW(hIn, EM_REPLACESEL, TRUE, (LPARAM)ins.c_str());
}

enum { U_CAT = 3001, U_FROM, U_TO, U_VAL, U_RES, U_CONV = 3100, U_SWAP };

static void DrawBtn(DRAWITEMSTRUCT* d, bool active) {
    HDC dc = d->hDC; RECT r = d->rcItem;
    bool sel = (d->itemState & ODS_SELECTED) != 0;
    bool primary = d->CtlID == ID_EQ || d->CtlID == U_CONV;
    COLORREF buttonColor = sel ? COLOR_BUTTON_DOWN
        : primary ? COLOR_ACTION : active ? COLOR_ACTIVE : COLOR_BUTTON;
    HBRUSH b = CreateSolidBrush(buttonColor);
    FillRect(dc, &r, b); DeleteObject(b);
    HBRUSH fb = CreateSolidBrush(active || primary ? buttonColor : COLOR_BORDER);
    FrameRect(dc, &r, fb);
    if (active) { InflateRect(&r, -1, -1); FrameRect(dc, &r, fb); InflateRect(&r, 1, 1); }
    DeleteObject(fb);
    wchar_t t[64]; GetWindowTextW(d->hwndItem, t, 64);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, active || primary ? RGB(255, 255, 255) : COLOR_TEXT);
    HFONT old = (HFONT)SelectObject(dc, g_btnFont);
    DrawTextW(dc, t, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, old);
}

static HWND MkBtn(HWND parent, const wchar_t* t, int id, int x, int y, int w, int h) {
    HWND b = CreateWindowW(L"BUTTON", t, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                           x, y, w, h, parent, (HMENU)(INT_PTR)id, hInst, 0);
    SendMessageW(b, WM_SETFONT, (WPARAM)g_btnFont, TRUE);
    return b;
}

static LRESULT CALLBACK EditProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_CHAR && w == VK_RETURN) { PostMessageW(GetParent(h), WM_COMMAND, ID_EQ, 0); return 0; }
    if (m == WM_CHAR && w == VK_ESCAPE) { PostMessageW(GetParent(h), WM_COMMAND, ID_KEY, 0); return 0; }
    return CallWindowProcW(oldEditProc, h, m, w, l);
}

struct U { const wchar_t* n; double f; };

static const U LEN[] = {
    {L"millimetre (mm)", 0.001}, {L"centimetre (cm)", 0.01}, {L"metre (m)", 1}, {L"kilometre (km)", 1000},
    {L"inch (in)", 0.0254}, {L"foot (ft)", 0.3048}, {L"yard (yd)", 0.9144},
    {L"mile (mi)", 1609.344}, {L"nautical mile", 1852} };
static const U MASS[] = {
    {L"milligram (mg)", 1e-6}, {L"gram (g)", 0.001}, {L"kilogram (kg)", 1}, {L"tonne (t)", 1000},
    {L"ounce (oz)", 0.028349523125}, {L"pound (lb)", 0.45359237}, {L"stone (st)", 6.35029318} };
static const U TEMP[] = { {L"Celsius (\u00B0C)", 0}, {L"Fahrenheit (\u00B0F)", 0}, {L"Kelvin (K)", 0} };
static const U TIME[] = {
    {L"millisecond (ms)", 0.001}, {L"second (s)", 1}, {L"minute (min)", 60}, {L"hour (h)", 3600},
    {L"day", 86400}, {L"week", 604800}, {L"year (365.25 d)", 31557600} };
static const U SPEED[] = {
    {L"metre/second (m/s)", 1}, {L"kilometre/hour (km/h)", 1.0 / 3.6}, {L"mile/hour (mph)", 0.44704},
    {L"knot (kn)", 1852.0 / 3600.0}, {L"foot/second (ft/s)", 0.3048}, {L"Mach (sea level)", 340.29} };
static const U AREA[] = {
    {L"mm\u00B2", 1e-6}, {L"cm\u00B2", 1e-4}, {L"m\u00B2", 1}, {L"hectare (ha)", 1e4}, {L"km\u00B2", 1e6},
    {L"in\u00B2", 6.4516e-4}, {L"ft\u00B2", 0.09290304}, {L"yd\u00B2", 0.83612736},
    {L"acre", 4046.8564224}, {L"mi\u00B2", 2589988.110336} };
static const U VOL[] = {
    {L"millilitre (mL)", 0.001}, {L"litre (L)", 1}, {L"cubic metre (m\u00B3)", 1000},
    {L"teaspoon (US)", 0.00492892159375}, {L"tablespoon (US)", 0.01478676478125},
    {L"fluid ounce (US)", 0.0295735295625}, {L"cup (US)", 0.2365882365},
    {L"pint (US)", 0.473176473}, {L"quart (US)", 0.946352946}, {L"gallon (US)", 3.785411784} };
static const U DATA[] = {
    {L"bit", 0.125}, {L"byte (B)", 1}, {L"kilobyte (KB, 1000)", 1e3}, {L"megabyte (MB)", 1e6},
    {L"gigabyte (GB)", 1e9}, {L"terabyte (TB)", 1e12}, {L"kibibyte (KiB, 1024)", 1024.0},
    {L"mebibyte (MiB)", 1048576.0}, {L"gibibyte (GiB)", 1073741824.0},
    {L"tebibyte (TiB)", 1099511627776.0} };

struct Cat { const wchar_t* name; const U* u; int n; bool temp; };
#define CNT(a) ((int)(sizeof(a) / sizeof(U)))
static const Cat CATS[] = {
    {L"Length", LEN, CNT(LEN), false}, {L"Mass", MASS, CNT(MASS), false},
    {L"Temperature", TEMP, CNT(TEMP), true}, {L"Time", TIME, CNT(TIME), false},
    {L"Speed", SPEED, CNT(SPEED), false}, {L"Area", AREA, CNT(AREA), false},
    {L"Volume", VOL, CNT(VOL), false}, {L"Data", DATA, CNT(DATA), false} };
static const int NCATS = (int)(sizeof(CATS) / sizeof(Cat));

static HWND uCat, uFrom, uTo, uVal, uRes;

static void uFill() {
    int c = (int)SendMessageW(uCat, CB_GETCURSEL, 0, 0); if (c < 0) c = 0;
    SendMessageW(uFrom, CB_RESETCONTENT, 0, 0);
    SendMessageW(uTo, CB_RESETCONTENT, 0, 0);
    for (int i = 0; i < CATS[c].n; i++) {
        SendMessageW(uFrom, CB_ADDSTRING, 0, (LPARAM)CATS[c].u[i].n);
        SendMessageW(uTo, CB_ADDSTRING, 0, (LPARAM)CATS[c].u[i].n);
    }
    SendMessageW(uFrom, CB_SETCURSEL, 0, 0);
    SendMessageW(uTo, CB_SETCURSEL, CATS[c].n > 1 ? 1 : 0, 0);
}

static void uConvert() {
    if (!uRes || !uVal || !uCat || !uFrom || !uTo) return;
    int c = (int)SendMessageW(uCat, CB_GETCURSEL, 0, 0);
    int a = (int)SendMessageW(uFrom, CB_GETCURSEL, 0, 0);
    int b = (int)SendMessageW(uTo, CB_GETCURSEL, 0, 0);
    if (c < 0 || a < 0 || b < 0) return;
    std::wstring t = getText(uVal);
    if (t.empty()) { SetWindowTextW(uRes, L""); return; }
    try {
        Parser P(t); double x = P.parse(), r;
        const Cat& ct = CATS[c];
        if (ct.temp) {
            double C = (a == 0) ? x : (a == 1) ? (x - 32.0) * 5.0 / 9.0 : x - 273.15;
            if (C < -273.15 - 1e-9) throw Err{ L"Below absolute zero" };
            r = (b == 0) ? C : (b == 1) ? C * 9.0 / 5.0 + 32.0 : C + 273.15;
        } else {
            r = x * ct.u[a].f / ct.u[b].f;
        }
        SetWindowTextW(uRes, F(r).c_str());
    } catch (Err& e) { SetWindowTextW(uRes, (L"Error: " + e.m).c_str()); }
    catch (...) { SetWindowTextW(uRes, L"Error"); }
}

static HWND MkLabel(HWND p, const wchar_t* t, int x, int y, int w, int h) {
    HWND s = CreateWindowW(L"STATIC", t, WS_CHILD | WS_VISIBLE, x, y, w, h, p, 0, hInst, 0);
    SendMessageW(s, WM_SETFONT, (WPARAM)g_smallFont, TRUE);
    return s;
}
static HWND MkCombo(HWND p, int id, int x, int y, int w) {
    HWND c = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                           x, y, w, 220, p, (HMENU)(INT_PTR)id, hInst, 0);
    SendMessageW(c, WM_SETFONT, (WPARAM)g_smallFont, TRUE);
    return c;
}

static void createUnitControls(HWND parent) {
    g_unitControls.push_back(MkLabel(parent, L"Category", 150, 216, 90, 26));
    uCat = MkCombo(parent, U_CAT, 250, 210, 490);
    g_unitControls.push_back(uCat);
    g_unitControls.push_back(MkLabel(parent, L"From", 150, 270, 90, 26));
    uFrom = MkCombo(parent, U_FROM, 250, 264, 490);
    g_unitControls.push_back(uFrom);
    g_unitControls.push_back(MkLabel(parent, L"To", 150, 324, 90, 26));
    uTo = MkCombo(parent, U_TO, 250, 318, 490);
    g_unitControls.push_back(uTo);
    g_unitControls.push_back(MkLabel(parent, L"Value", 150, 382, 90, 30));
    uVal = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                           250, 374, 490, 38, parent, (HMENU)(INT_PTR)U_VAL, hInst, 0);
    SendMessageW(uVal, WM_SETFONT, (WPARAM)g_dispFont, TRUE);
    g_unitControls.push_back(uVal);
    g_unitControls.push_back(MkLabel(parent, L"Result", 150, 442, 90, 30));
    uRes = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY,
                           250, 434, 490, 38, parent, (HMENU)(INT_PTR)U_RES, hInst, 0);
    SendMessageW(uRes, WM_SETFONT, (WPARAM)g_dispFont, TRUE);
    g_unitControls.push_back(uRes);
    g_unitControls.push_back(MkBtn(parent, L"CONVERT", U_CONV, 250, 500, 235, 42));
    g_unitControls.push_back(MkBtn(parent, L"SWAP", U_SWAP, 505, 500, 235, 42));
    for (int i = 0; i < NCATS; i++) SendMessageW(uCat, CB_ADDSTRING, 0, (LPARAM)CATS[i].name);
    SendMessageW(uCat, CB_SETCURSEL, 0, 0);
    uFill();
    SetWindowTextW(uVal, L"1");
    uConvert();
}

static void setActivePage(int page) {
    if (page < 0 || page >= 3) return;
    g_activePage = page;
    const std::vector<HWND>* pages[] = {
        &g_scientificControls, &g_infoControls, &g_unitControls
    };
    for (int i = 0; i < 3; i++)
        for (HWND control : *pages[i]) ShowWindow(control, i == page ? SW_SHOW : SW_HIDE);
    for (HWND control : g_basicControls)
        ShowWindow(control, page == 0 || page == 1 ? SW_SHOW : SW_HIDE);
    for (HWND tab : hTabs) InvalidateRect(tab, NULL, TRUE);
}

static LRESULT CALLBACK MainProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE: {
        hMain = h;
        hTitle = CreateWindowW(L"STATIC", L"CALCII - RAGHAV VERMA", WS_CHILD | WS_VISIBLE | SS_CENTER,
                       16, 8, 748, 24, h, 0, hInst, 0);
        SendMessageW(hTitle, WM_SETFONT, (WPARAM)g_titleFont, TRUE);

        hIn = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                              WS_CHILD | WS_VISIBLE | ES_RIGHT | ES_AUTOHSCROLL | ES_NOHIDESEL,
                              16, 40, 748, 38, h, (HMENU)(INT_PTR)100, hInst, 0);
        SendMessageW(hIn, WM_SETFONT, (WPARAM)g_dispFont, TRUE);
        oldEditProc = (WNDPROC)SetWindowLongPtrW(hIn, GWLP_WNDPROC, (LONG_PTR)EditProc);

        hOut = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                               WS_CHILD | WS_VISIBLE | ES_RIGHT | ES_AUTOHSCROLL | ES_READONLY,
                               16, 84, 748, 36, h, (HMENU)(INT_PTR)101, hInst, 0);
        SendMessageW(hOut, WM_SETFONT, (WPARAM)g_dispFont, TRUE);

        hModeLbl = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 16, 132, 748, 20, h, 0, hInst, 0);
        SendMessageW(hModeLbl, WM_SETFONT, (WPARAM)g_smallFont, TRUE);

        hTabs[0] = MkBtn(h, L"SCI", ID_TAB_SCI, 16, 178, 104, 42);
        hTabs[1] = MkBtn(h, L"INFO", ID_TAB_INFO, 16, 230, 104, 42);
        hTabs[2] = MkBtn(h, L"UNIT", ID_TAB_UNIT, 16, 282, 104, 42);

        g_scientificControls.push_back(MkLabel(h, L"SCIENTIFIC", 140, 170, 292, 24));
        g_basicControls.push_back(MkLabel(h, L"BASIC KEYPAD", 450, 170, 300, 24));
        for (int i = 0; i < EXTRA_COUNT; i++) {
            hExtra[i] = MkBtn(h, EXTRAS[i].label, ID_EXTRA + i,
                              140 + (i % 5) * 58, 204 + (i / 5) * 42, 54, 36);
            g_scientificControls.push_back(hExtra[i]);
        }
        for (int i = 0; i < (int)(sizeof(KEYS) / sizeof(Ex)); i++)
            g_basicControls.push_back(MkBtn(h, KEYS[i].label, ID_KEY + i,
                                             450 + (i % 4) * 72, 204 + (i / 4) * 42, 66, 36));
        g_basicControls.push_back(MkBtn(h, L"=", ID_EQ, 450, 456, 300, 42));

        g_infoControls.push_back(MkLabel(h, L"INFORMATION TOOLS", 140, 170, 292, 24));
        g_infoControls.push_back(MkLabel(h, L"Enter probabilities or counts separated by commas for H, efficiency or redundancy; enter one probability for I.",
                                         140, 202, 292, 58));
        for (int i = 0; i < (int)(sizeof(INFO_TOOLS) / sizeof(Ex)); i++)
            g_infoControls.push_back(MkBtn(h, INFO_TOOLS[i].label, ID_INFO + i,
                                           140 + (i % 2) * 152, 270 + (i / 2) * 52, 142, 42));

        g_unitControls.push_back(MkLabel(h, L"UNIT CONVERTER", 140, 170, 610, 24));
        createUnitControls(h);

        updateStatus();
        setActivePage(0);
        EnumChildWindows(h, collectMainChild, 0);
        RECT client;
        GetClientRect(h, &client);
        layoutMain(client.right, client.bottom);
        SetFocus(hIn);
        return 0;
    }
    case WM_SIZE:
        layoutMain(LOWORD(l), HIWORD(l));
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(w);
        if (id == ID_TAB_SCI) setActivePage(0);
        else if (id == ID_TAB_INFO) setActivePage(1);
        else if (id == ID_TAB_UNIT) setActivePage(2);
        else if (id >= ID_EXTRA && id < ID_EXTRA + EXTRA_COUNT) doExtra(id - ID_EXTRA);
        else if (id >= ID_INFO && id < ID_INFO + (int)(sizeof(INFO_TOOLS) / sizeof(Ex)))
            doInfo(INFO_TOOLS[id - ID_INFO].ins);
        else if (id >= ID_KEY && id < ID_KEY + (int)(sizeof(KEYS) / sizeof(Ex))) doKey(id - ID_KEY);
        else if (id == ID_EQ) doEquals();
        else if (id == U_CAT && HIWORD(w) == CBN_SELCHANGE) { uFill(); uConvert(); }
        else if ((id == U_FROM || id == U_TO) && HIWORD(w) == CBN_SELCHANGE) uConvert();
        else if (id == U_VAL && HIWORD(w) == EN_CHANGE) uConvert();
        else if (id == U_CONV) uConvert();
        else if (id == U_SWAP) {
            int a = (int)SendMessageW(uFrom, CB_GETCURSEL, 0, 0);
            int b = (int)SendMessageW(uTo, CB_GETCURSEL, 0, 0);
            SendMessageW(uFrom, CB_SETCURSEL, b, 0);
            SendMessageW(uTo, CB_SETCURSEL, a, 0);
            uConvert();
        }
        if ((id >= ID_EXTRA && id < ID_EXTRA + EXTRA_COUNT) ||
            (id >= ID_INFO && id < ID_INFO + (int)(sizeof(INFO_TOOLS) / sizeof(Ex))) ||
            (id >= ID_KEY && id < ID_KEY + (int)(sizeof(KEYS) / sizeof(Ex))) || id == ID_EQ)
            SetFocus(hIn);
        return 0;
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* d = (DRAWITEMSTRUCT*)l;
        bool active = d->CtlID >= ID_TAB_SCI && d->CtlID <= ID_TAB_UNIT &&
             d->CtlID == ID_TAB_SCI + g_activePage;
        DrawBtn(d, active);
        return TRUE;
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)w; SetTextColor(dc, COLOR_TEXT); SetBkColor(dc, RGB(255, 255, 255));
        return (LRESULT)hWhite;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)w; HWND c = (HWND)l;
        if (c == hOut) { SetTextColor(dc, COLOR_TEXT); SetBkColor(dc, RGB(255, 255, 255)); return (LRESULT)hWhite; }
        SetBkColor(dc, COLOR_SURFACE);
        SetTextColor(dc, c == hTitle ? COLOR_TEXT : COLOR_MUTED);
        return (LRESULT)hSurface;
    }
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

int WINAPI WinMain(HINSTANCE hi, HINSTANCE, LPSTR, int) {
    hInst = hi;
    hSurface = CreateSolidBrush(COLOR_SURFACE);
    hWhite = CreateSolidBrush(RGB(255, 255, 255));
    g_btnFont   = CreateFontW(-17, 0, 0, 0, FW_BOLD,   0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_dispFont  = CreateFontW(-26, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Consolas");
    g_titleFont = CreateFontW(-20, 0, 0, 0, FW_BOLD,   0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_smallFont = CreateFontW(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");

    WNDCLASSW wc = {};
    wc.hInstance = hi; wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.hbrBackground = hSurface;
    wc.lpfnWndProc = MainProc;  wc.lpszClassName = L"CalcIIMain";
    RegisterClassW(&wc);

    RECT r = { 0, 0, 780, 620 };
    DWORD st = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX |
               WS_THICKFRAME | WS_MAXIMIZEBOX;
    AdjustWindowRect(&r, st, FALSE);
    HWND w = CreateWindowW(L"CalcIIMain", L"CALCII \u2014 RAGHAV VERMA", st,
                           CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
                           0, 0, hi, 0);
    ShowWindow(w, SW_SHOWNORMAL);
    UpdateWindow(w);

    MSG msg;
    while (GetMessageW(&msg, 0, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}