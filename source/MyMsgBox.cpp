// UNICODE comes from the compiler flags (/DUNICODE /D_UNICODE)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "MyMsgBox.h"

#include <windowsx.h>
#include <cwchar>
#include <string>
#include <algorithm> // std::min / std::max

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

// =========================
// Internal structures
// =========================
struct MsgData {
    std::wstring title;
    std::wstring text;
    unsigned     flags { MYMB_OK };
    int          result { 0 };
    HFONT        hFont { nullptr };
    RECT         rcBtn1 {}, rcBtn2 {};
    std::wstring btn1Label, btn2Label;
    int          hot { 0 };  // 0 = none, 1 = btn1, 2 = btn2 (hover)
    int          def { 1 };  // default button (1/2)
    int          icon { 0 }; // 0 none, 1 info, 2 warn, 3 err
    SIZE         textSize {};
};

static const wchar_t* kCls = L"MYRAWMSGBOX_CLS";

// =========================
// Drawing helpers
// =========================
static void DrawIconGlyph(HDC hdc, RECT r, int type) {
    if (type == 0) return;

    const int w  = r.right  - r.left;
    const int h  = r.bottom - r.top;
    const int d  = (std::min)(w, h) - 2;
    const int cx = (r.left + r.right) / 2;
    const int cy = (r.top  + r.bottom) / 2;

    const COLORREF color =
        (type == 1) ? RGB(0, 120, 215) :
        (type == 2) ? RGB(255, 170, 0) :
                      RGB(220, 20, 60);

    // Filled circle
    HBRUSH b  = CreateSolidBrush(color);
    HBRUSH ob = (HBRUSH)SelectObject(hdc, b);
    HPEN   p  = CreatePen(PS_NULL, 0, 0);
    HPEN   op = (HPEN)SelectObject(hdc, p);

    Ellipse(hdc, cx - d / 2, cy - d / 2, cx + d / 2, cy + d / 2);

    SelectObject(hdc, op); DeleteObject(p);
    SelectObject(hdc, ob); DeleteObject(b);

    // Center glyph: i / ! / x
    const wchar_t ch = (type == 1 ? L'i' : (type == 2 ? L'!' : L'x'));

    LOGFONTW lf {};
    lf.lfHeight = d / 2;
    lf.lfWeight = FW_BOLD;
    wcscpy_s(lf.lfFaceName, L"Segoe UI");

    HFONT f  = CreateFontIndirectW(&lf);
    HFONT of = (HFONT)SelectObject(hdc, f);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));

    SIZE s {};
    GetTextExtentPoint32W(hdc, &ch, 1, &s);
    TextOutW(hdc, cx - s.cx / 2, cy - s.cy / 2, &ch, 1);

    SelectObject(hdc, of);
    DeleteObject(f);
}

static void DrawButton(HDC hdc, RECT r, const std::wstring& label, bool hot, bool def) {
    const COLORREF bg = hot ? RGB(230, 240, 255) : RGB(245, 245, 245);

    HBRUSH br = CreateSolidBrush(bg);
    FillRect(hdc, &r, br);
    DeleteObject(br);

    // Border
    FrameRect(hdc, &r, (HBRUSH)GetStockObject(GRAY_BRUSH));

    // Default outline
    if (def) {
        HPEN   p  = CreatePen(PS_SOLID, 2, RGB(0, 120, 215));
        HPEN   op = (HPEN)SelectObject(hdc, p);
        HBRUSH nb = (HBRUSH)GetStockObject(NULL_BRUSH);
        HBRUSH ob = (HBRUSH)SelectObject(hdc, nb);

        Rectangle(hdc, r.left + 1, r.top + 1, r.right - 1, r.bottom - 1);

        SelectObject(hdc, ob);
        SelectObject(hdc, op);
        DeleteObject(p);
    }

    // Label
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));

    SIZE s {};
    GetTextExtentPoint32W(hdc, label.c_str(), (int)label.size(), &s);

    const int x = r.left + ((r.right - r.left) - s.cx) / 2;
    const int y = r.top  + ((r.bottom - r.top) - s.cy) / 2;

    TextOutW(hdc, x, y, label.c_str(), (int)label.size());
}

static void SetLabelsAndIcon(MsgData* d) {
    if (d->flags & MYMB_YESNO) {
        d->btn1Label = L"Yes";
        d->btn2Label = L"No";
        d->def       = (d->flags & MYMB_DEFAULT2) ? 2 : 1;
    } else if (d->flags & MYMB_OKCANCEL) {
        d->btn1Label = L"OK";
        d->btn2Label = L"Cancel";
        d->def       = (d->flags & MYMB_DEFAULT2) ? 2 : 1;
    } else {
        d->btn1Label = L"OK";
        d->btn2Label.clear();
        d->def = 1;
    }

    if      (d->flags & MYMB_ICON_INFO) d->icon = 1;
    else if (d->flags & MYMB_ICON_WARN) d->icon = 2;
    else if (d->flags & MYMB_ICON_ERR ) d->icon = 3;
}

static void Layout(HWND h, MsgData* d) {
    RECT wr {};
    GetClientRect(h, &wr);

    const int pad  = 16;
    const int btnW = 90;
    const int btnH = 28;
    const int gap  = 12;

    // Measure text
    HDC   hdc = GetDC(h);
    HFONT of  = (HFONT)SelectObject(hdc, d->hFont);

    RECT rcText { pad, pad, wr.right - pad, wr.bottom - pad };
    if (d->icon) rcText.left += 48 + 12; // icon width + spacing

    DrawTextW(hdc, d->text.c_str(), -1, &rcText, DT_CALCRECT | DT_WORDBREAK);
    d->textSize.cx = rcText.right  - rcText.left;
    d->textSize.cy = rcText.bottom - rcText.top;

    SelectObject(hdc, of);
    ReleaseDC(h, hdc);

    // FIX: force same type (int) on both arguments of std::max
    const int contentH = (std::max)(static_cast<int>(d->textSize.cy),
                                    d->icon ? 48 : 0);

    const int btnTop    = pad + contentH + 24;
    const bool twoBtns  = (d->flags & (MYMB_OKCANCEL | MYMB_YESNO)) != 0;
    const int totalW    = twoBtns ? (btnW * 2 + gap) : btnW;
    const int startX    = (wr.right - totalW) / 2;

    d->rcBtn1 = { startX, btnTop, startX + btnW, btnTop + btnH };
    if (twoBtns) {
        d->rcBtn2 = { startX + btnW + gap, btnTop, startX + btnW + gap + btnW, btnTop + btnH };
    } else {
        SetRectEmpty(&d->rcBtn2);
    }
}

static void CenterToOwner(HWND h, HWND owner) {
    RECT rcW {};
    GetWindowRect(h, &rcW);
    const int ww = rcW.right - rcW.left;
    const int wh = rcW.bottom - rcW.top;

    RECT rc {};
    if (owner && GetWindowRect(owner, &rc)) {
        const int cx = rc.left + (rc.right  - rc.left - ww) / 2;
        const int cy = rc.top  + (rc.bottom - rc.top  - wh) / 2;
        SetWindowPos(h, nullptr, (std::max)(cx, 0), (std::max)(cy, 0),
                     0, 0, SWP_NOZORDER | SWP_NOSIZE);
    } else {
        RECT wa {};
        SystemParametersInfo(SPI_GETWORKAREA, 0, &wa, 0);
        const int cx = wa.left + (wa.right  - wa.left - ww) / 2;
        const int cy = wa.top  + (wa.bottom - wa.top  - wh) / 2;
        SetWindowPos(h, nullptr, (std::max)(cx, 0), (std::max)(cy, 0),
                     0, 0, SWP_NOZORDER | SWP_NOSIZE);
    }
}

// =========================
// Window procedure
// =========================
static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    MsgData* d = (MsgData*)GetWindowLongPtrW(h, GWLP_USERDATA);

    switch (msg) {
        case WM_NCCREATE: {
            auto* cs = (CREATESTRUCTW*)l;
            d = (MsgData*)cs->lpCreateParams;
            SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)d);
            return DefWindowProcW(h, msg, w, l);
        }

        case WM_CREATE: {
            HDC tdc = GetDC(h);
            const int dpiY = GetDeviceCaps(tdc, LOGPIXELSY);
            ReleaseDC(h, tdc);

            LOGFONTW lf {};
            lf.lfHeight = -MulDiv(10, dpiY, 72);
            wcscpy_s(lf.lfFaceName, L"Segoe UI");

            d->hFont = CreateFontIndirectW(&lf);
            Layout(h, d);
            return 0;
        }

        case WM_SIZE:
            if (d) Layout(h, d);
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps {};
            HDC hdc = BeginPaint(h, &ps);

            RECT rc {};
            GetClientRect(h, &rc);

            // Background
            HBRUSH bg = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(hdc, &rc, bg);
            DeleteObject(bg);

            // Title
            SetBkMode(hdc, TRANSPARENT);
            HFONT of = (HFONT)SelectObject(hdc, d->hFont);
            SetTextColor(hdc, RGB(30, 30, 30));

            RECT rTitle { 12, 8, rc.right - 12, 28 };
            DrawTextW(hdc, d->title.c_str(), -1, &rTitle,
                      DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);

            // Separator
            HPEN p  = CreatePen(PS_SOLID, 1, RGB(230, 230, 230));
            HPEN op = (HPEN)SelectObject(hdc, p);
            MoveToEx(hdc, 0, 34, nullptr);
            LineTo(hdc, rc.right, 34);
            SelectObject(hdc, op);
            DeleteObject(p);

            // Icon + text
            RECT rIcon { 16, 50, 16 + 48, 50 + 48 };
            if (d->icon) DrawIconGlyph(hdc, rIcon, d->icon);

            RECT rText { d->icon ? (16 + 48 + 12) : 16, 50, rc.right - 16, 50 + d->textSize.cy };
            SetTextColor(hdc, RGB(40, 40, 40));
            DrawTextW(hdc, d->text.c_str(), -1, &rText, DT_WORDBREAK);

            // Buttons
            const bool twoBtns = (d->flags & (MYMB_OKCANCEL | MYMB_YESNO)) != 0;
            DrawButton(hdc, d->rcBtn1, d->btn1Label, d->hot == 1, d->def == 1);
            if (twoBtns) {
                DrawButton(hdc, d->rcBtn2, d->btn2Label, d->hot == 2, d->def == 2);
            }

            SelectObject(hdc, of);
            EndPaint(h, &ps);
            return 0;
        }

        case WM_MOUSEMOVE: {
            POINT pt { GET_X_LPARAM(l), GET_Y_LPARAM(l) };
            int   nh = 0;

            if (PtInRect(&d->rcBtn1, pt)) nh = 1;
            else if (PtInRect(&d->rcBtn2, pt)) nh = 2;

            if (nh != d->hot) {
                d->hot = nh;
                InvalidateRect(h, nullptr, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            POINT pt { GET_X_LPARAM(l), GET_Y_LPARAM(l) };

            if (PtInRect(&d->rcBtn1, pt)) {
                d->result = (d->flags & MYMB_YESNO) ? IDYES : IDOK;
                PostMessageW(h, WM_CLOSE, 0, 0);
            } else if (PtInRect(&d->rcBtn2, pt)) {
                d->result = (d->flags & MYMB_YESNO) ? IDNO : IDCANCEL;
                PostMessageW(h, WM_CLOSE, 0, 0);
            }
            return 0;
        }

        case WM_KEYDOWN:
            if (w == VK_ESCAPE) {
                d->result = (d->flags & MYMB_YESNO) ? IDNO : IDCANCEL;
                PostMessageW(h, WM_CLOSE, 0, 0);
            } else if (w == VK_RETURN || w == VK_SPACE) {
                d->result = (d->def == 2)
                    ? ((d->flags & MYMB_YESNO) ? IDNO    : IDCANCEL)
                    : ((d->flags & MYMB_YESNO) ? IDYES   : IDOK);
                PostMessageW(h, WM_CLOSE, 0, 0);
            }
            return 0;

        case WM_CLOSE:
            DestroyWindow(h);
            return 0;

        case WM_DESTROY:
            if (d && d->hFont) {
                DeleteObject(d->hFont);
                d->hFont = nullptr;
            }
            return 0;
    }

    return DefWindowProcW(h, msg, w, l);
}

// =========================
// Class registration
// =========================
static ATOM EnsureClass(HINSTANCE hi) {
    static ATOM a = 0;
    if (a) return a;

    WNDCLASSEXW wc { sizeof(wc) };
    wc.hInstance     = hi;
    wc.lpszClassName = kCls;
    wc.lpfnWndProc   = WndProc;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.style         = CS_HREDRAW | CS_VREDRAW;

    a = RegisterClassExW(&wc);
    return a;
}

// =========================
// Simple modal loop
// =========================
static int RunModal(HWND hwnd, HWND owner, MsgData* d) {
    if (owner) EnableWindow(owner, FALSE); // simple modality

    ShowWindow(hwnd, SW_SHOWNORMAL);
    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    SetFocus(hwnd);

    MSG m {};
    while (IsWindow(hwnd) && GetMessageW(&m, nullptr, 0, 0)) {
        if (!IsDialogMessageW(hwnd, &m)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    }

    if (owner) {
        EnableWindow(owner, TRUE);
        SetActiveWindow(owner);
    }
    return d->result ? d->result : IDCANCEL;
}

// =========================
// Public API
// =========================
extern "C" MYMSG_API int MyMessageBox(HWND owner,
                                      const wchar_t* text,
                                      const wchar_t* title,
                                      unsigned flags)
{
    HINSTANCE hi = (HINSTANCE)GetModuleHandleW(nullptr);
    EnsureClass(hi);

    MsgData data;
    data.title = title ? title : L"";
    data.text  = text  ? text  : L"";
    data.flags = flags;
    SetLabelsAndIcon(&data);

    // Initial size; adjust after measuring text
    const int initW = 480;
    const int initH = 200;

    DWORD ex = WS_EX_TOOLWINDOW | WS_EX_DLGMODALFRAME;
    if (flags & MYMB_TOPMOST) ex |= WS_EX_TOPMOST;

    const DWORD style = WS_POPUP | WS_CAPTION;

    HWND hwnd = CreateWindowExW(
        ex, kCls, data.title.c_str(), style,
        CW_USEDEFAULT, CW_USEDEFAULT, initW, initH,
        owner, nullptr, hi, &data
    );

    // Recalculate size to fit text nicely
    HDC hdc = GetDC(hwnd);
    const int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);

    LOGFONTW lf {};
    lf.lfHeight = -MulDiv(10, dpiY, 72);
    wcscpy_s(lf.lfFaceName, L"Segoe UI");

    HFONT f  = CreateFontIndirectW(&lf);
    HFONT of = (HFONT)SelectObject(hdc, f);

    RECT rc { 0, 0, initW - 32 - (data.icon ? 60 : 0), 0 };
    DrawTextW(hdc, data.text.c_str(), -1, &rc, DT_CALCRECT | DT_WORDBREAK);

    const int contentH = (std::max)((int)(rc.bottom - rc.top), data.icon ? 48 : 0);
    const int finalH   = 50 + contentH + 24 + 28 + 24; // paddings + buttons
    const int finalW   = (std::max)(initW, (int)(rc.right - rc.left) + (data.icon ? 60 : 0) + 32);

    SelectObject(hdc, of);
    DeleteObject(f);
    ReleaseDC(hwnd, hdc);

    SetWindowPos(hwnd, nullptr, 0, 0, finalW, finalH,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    Layout(hwnd, &data);
    CenterToOwner(hwnd, owner);

    return RunModal(hwnd, owner, &data);
}