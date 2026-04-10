#define NOMINMAX
#ifdef UNICODE
#undef UNICODE
#endif
#ifdef _UNICODE
#undef _UNICODE
#endif
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <functional>
#include <cctype>
#include <cstdio>
#include <cstring>
#include "fat32/DiskReader.h"
#include "fat32/BootSector.h"
#include "fat32/FATTable.h"
#include "fat32/DirectoryEntry.h"
#include "scheduler/Scheduler.h"

#pragma comment(lib, "Comctl32.lib")

struct QueueInfo {
    std::string id;
    int timeSlice = 0;
    std::string algorithm;
};

struct ProcessInfo {
    std::string id;
    int arrivalTime = 0;
    int burstTime = 0;
    std::string queueId;
};

struct ParsedSchedule {
    bool ok = false;
    std::string error;
    int queueCount = 0;
    std::vector<QueueInfo> queues;
    std::vector<ProcessInfo> processes;
};

struct SimulationResult {
    bool ok = false;
    std::string error;
    std::vector<ScheduleEvent> timeline;
    std::vector<Process> processes;
};

enum ControlId {
    IDC_DRIVE_COMBO = 1001,
    IDC_REFRESH_BTN,
    IDC_LOAD_BTN,
    IDC_BOOT_LIST,
    IDC_FILE_LIST,
    IDC_RIGHT_TABS,
    IDC_RIGHT_TAB_FILE_BTN,
    IDC_RIGHT_TAB_SCHEDULE_BTN,
    IDC_DETAIL_EDIT,
    IDC_CONTENT_EDIT,
    IDC_PROCESS_LIST,
    IDC_STATUS_TEXT,
    IDC_BOOT_LABEL,
    IDC_FILE_LABEL,
    IDC_DETAIL_LABEL,
    IDC_CONTENT_LABEL,
    IDC_PROCESS_LABEL,
    IDC_GANTT_LABEL,
    IDC_GANTT_CHART
};

static HWND g_hMainWnd = nullptr;
static HWND g_hDriveCombo = nullptr;
static HWND g_hRefreshBtn = nullptr;
static HWND g_hScanBtn = nullptr;
static HWND g_hBootList = nullptr;
static HWND g_hFileList = nullptr;
static HWND g_hRightTabs = nullptr;
static HWND g_hRightTabFileBtn = nullptr;
static HWND g_hRightTabScheduleBtn = nullptr;
static HWND g_hFileTabPage = nullptr;
static HWND g_hScheduleTabPage = nullptr;
static HWND g_hDetailEdit = nullptr;
static HWND g_hContentEdit = nullptr;
static HWND g_hProcessList = nullptr;
static HWND g_hStatusText = nullptr;
static HWND g_hGanttLabel = nullptr;
static HWND g_hGanttChart = nullptr;
static HWND g_hBootLabel = nullptr;
static HWND g_hFileLabel = nullptr;
static HWND g_hDetailLabel = nullptr;
static HWND g_hContentLabel = nullptr;
static HWND g_hProcessLabel = nullptr;

static HFONT g_hFontUI = nullptr;
static HFONT g_hFontLabel = nullptr;
static HFONT g_hFontMono = nullptr;
static HBRUSH g_hMainBrush = nullptr;
static HBRUSH g_hCardBrush = nullptr;

static DiskReader g_reader;
static BootSector g_boot;
static FATTable g_fat;
static DirectoryEntry g_directoryScanner;
static std::vector<FileInfo> g_txtFiles;
static std::string g_initialDrive;
static bool g_hasLoadedDrive = false;
static ParsedSchedule g_currentSchedule;
static SimulationResult g_currentSimulation;
static std::string g_currentDetailBase;
static bool g_hasSimulationData = false;
static int g_ganttScrollX = 0;
static int g_activeRightTab = 0;
static HBITMAP g_hGanttCanvasBitmap = nullptr;
static int g_ganttCanvasWidth = 0;
static int g_ganttCanvasHeight = 0;
static bool g_ganttCanvasDirty = true;

static const char* kAppTitle = "Lab 2 | FAT32 Schedule Explorer";
static const char* kGanttChartClass = "Lab2GanttChartView";
static const char* kRightTabPageClass = "Lab2RightTabPage";
static const char* kActionButtonClass = "Lab2ActionButton";
static const COLORREF kColorWindowBg = RGB(245, 247, 250);
static const COLORREF kColorCardBg = RGB(255, 255, 255);
static const COLORREF kColorTextMain = RGB(32, 32, 32);
static const COLORREF kColorTextMuted = RGB(96, 96, 96);
static const COLORREF kColorTabActiveBg = RGB(68, 128, 171);
static const COLORREF kColorTabActiveBorder = RGB(45, 96, 136);
static const COLORREF kColorTabInactiveBg = RGB(250, 251, 253);
static const COLORREF kColorTabInactiveBorder = RGB(210, 216, 224);
static const COLORREF kColorTabInactiveText = RGB(72, 72, 72);
static const COLORREF kColorRefreshBtnBg = RGB(236, 242, 248);
static const COLORREF kColorRefreshBtnBorder = RGB(168, 184, 201);
static const COLORREF kColorRefreshBtnPressed = RGB(205, 220, 235);
static const COLORREF kColorRefreshBtnText = RGB(52, 74, 94);
static const COLORREF kColorRefreshBtnHover = RGB(226, 235, 244);
static const COLORREF kColorScanBtnBg = RGB(72, 156, 120);
static const COLORREF kColorScanBtnBorder = RGB(51, 118, 89);
static const COLORREF kColorScanBtnPressed = RGB(58, 132, 101);
static const COLORREF kColorScanBtnText = RGB(255, 255, 255);
static const COLORREF kColorScanBtnHover = RGB(82, 169, 132);

enum RightTabIndex {
    RIGHT_TAB_FILE_VIEW = 0,
    RIGHT_TAB_SCHEDULE_VIEW = 1
};

enum ActionButtonStateFlags {
    ACTIONBTN_HOVER = 0x01,
    ACTIONBTN_PRESSED = 0x02
};

static void LayoutControls(HWND hwnd);
static void RedrawProcessTableHeader();

static const QueueInfo* FindQueueById(const std::vector<QueueInfo>& queues, const std::string& queueId) {
    for (const auto& q : queues) {
        if (q.id == queueId) return &q;
    }
    return nullptr;
}

static Process* FindProcessById(std::vector<Process>& processes, const std::string& processId) {
    for (auto& process : processes) {
        if (process.getPID() == processId) return &process;
    }
    return nullptr;
}

static COLORREF ColorFromKey(const std::string& key) {
    std::hash<std::string> hasher;
    const size_t value = hasher(key);
    const int r = 90 + static_cast<int>(value & 0x3F);
    const int g = 110 + static_cast<int>((value >> 6) & 0x5F);
    const int b = 120 + static_cast<int>((value >> 13) & 0x5F);
    return RGB(r, g, b);
}

static COLORREF AdjustColor(COLORREF color, int delta) {
    int r = std::clamp(static_cast<int>(GetRValue(color)) + delta, 0, 255);
    int g = std::clamp(static_cast<int>(GetGValue(color)) + delta, 0, 255);
    int b = std::clamp(static_cast<int>(GetBValue(color)) + delta, 0, 255);
    return RGB(r, g, b);
}

static COLORREF GetContrastingTextColor(COLORREF background) {
    const int luminance = (299 * GetRValue(background) +
                           587 * GetGValue(background) +
                           114 * GetBValue(background)) / 1000;
    return (luminance >= 150) ? RGB(32, 32, 32) : RGB(255, 255, 255);
}

static std::string ToWindowsNewlines(const std::string& text) {
    std::string result;
    result.reserve(text.size() + 32);

    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r') {
            result.push_back('\r');
            if (i + 1 < text.size() && text[i + 1] == '\n') {
                result.push_back('\n');
                ++i;
            } else {
                result.push_back('\n');
            }
        } else if (text[i] == '\n') {
            result.push_back('\r');
            result.push_back('\n');
        } else {
            result.push_back(text[i]);
        }
    }

    return result;
}

static std::string StripUtf8Bom(std::string text) {
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }
    return text;
}

static bool LooksLikeUtf16Le(const std::vector<uint8_t>& bytes) {
    if (bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE) {
        return true;
    }

    size_t sample = std::min<size_t>(bytes.size(), 128);
    if (sample < 4) return false;

    size_t oddNulls = 0;
    size_t pairs = 0;
    for (size_t i = 1; i < sample; i += 2) {
        ++pairs;
        if (bytes[i] == 0) {
            ++oddNulls;
        }
    }

    return pairs > 0 && oddNulls * 100 / pairs >= 60;
}

static std::string WideToAnsi(const std::wstring& wide) {
    if (wide.empty()) return "";

    int needed = WideCharToMultiByte(CP_ACP, 0, wide.c_str(), static_cast<int>(wide.size()),
                                     nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return "";

    std::string result(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_ACP, 0, wide.c_str(), static_cast<int>(wide.size()),
                        result.data(), needed, nullptr, nullptr);
    return result;
}

static std::string DecodeTextBytes(const std::vector<uint8_t>& bytes) {
    if (bytes.empty()) return "";

    if (LooksLikeUtf16Le(bytes)) {
        size_t start = (bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE) ? 2 : 0;
        std::wstring wide;
        wide.reserve((bytes.size() - start) / 2);

        for (size_t i = start; i + 1 < bytes.size(); i += 2) {
            uint16_t ch = static_cast<uint16_t>(bytes[i]) |
                          (static_cast<uint16_t>(bytes[i + 1]) << 8);
            if (ch == 0) continue;
            wide.push_back(static_cast<wchar_t>(ch));
        }
        return WideToAnsi(wide);
    }

    std::string text(bytes.begin(), bytes.end());
    return StripUtf8Bom(text);
}

static void SetStatusText(const std::string& text) {
    if (g_hStatusText) {
        SetWindowTextA(g_hStatusText, text.c_str());
    }
}

static std::string TrimCopy(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        s = s.substr(1, s.size() - 2);
    }
    return s;
}

static std::string NormalizeDriveInput(std::string s) {
    s = TrimCopy(s);

    if (s.rfind("\\\\.\\", 0) == 0) {
        s = s.substr(4);
    }

    while (s.size() > 2 && (s.back() == '\\' || s.back() == '/')) {
        s.pop_back();
    }

    if (s.size() == 1 && std::isalpha(static_cast<unsigned char>(s[0]))) {
        s.push_back(':');
    }

    if (s.size() >= 2 && std::isalpha(static_cast<unsigned char>(s[0])) && s[1] == ':') {
        s.resize(2);
        s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
        return s;
    }

    return "";
}

static bool SelectDriveInCombo(const std::string& drive) {
    std::string target = NormalizeDriveInput(drive);
    if (target.empty() || !g_hDriveCombo) return false;

    int count = static_cast<int>(SendMessageA(g_hDriveCombo, CB_GETCOUNT, 0, 0));
    char buffer[64]{};

    for (int i = 0; i < count; ++i) {
        SendMessageA(g_hDriveCombo, CB_GETLBTEXT, i, reinterpret_cast<LPARAM>(buffer));
        if (NormalizeDriveInput(buffer) == target) {
            SendMessageA(g_hDriveCombo, CB_SETCURSEL, i, 0);
            return true;
        }
    }

    return false;
}

static void ShowError(const std::string& message) {
    MessageBoxA(g_hMainWnd, message.c_str(), kAppTitle, MB_ICONERROR | MB_OK);
    SetStatusText(message);
}

static HFONT CreateAppFont(int ptSize, int weight, const char* faceName) {
    HDC hdc = GetDC(nullptr);
    int height = -MulDiv(ptSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(nullptr, hdc);
    return CreateFontA(height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, faceName);
}

static void ApplyControlFont(HWND hWnd, HFONT hFont) {
    if (hWnd && hFont) {
        SendMessageA(hWnd, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
    }
}

static HBRUSH ApplySharedControlColors(HWND hCtrl, HDC hdc) {
    if (!hdc) return nullptr;

    if (hCtrl == g_hDetailEdit || hCtrl == g_hContentEdit) {
        SetBkColor(hdc, kColorCardBg);
        SetTextColor(hdc, kColorTextMain);
        return g_hCardBrush;
    }

    SetBkMode(hdc, TRANSPARENT);
    if (hCtrl == g_hBootLabel || hCtrl == g_hFileLabel || hCtrl == g_hDetailLabel ||
        hCtrl == g_hContentLabel || hCtrl == g_hGanttLabel || hCtrl == g_hProcessLabel) {
        SetTextColor(hdc, kColorTextMain);
        return g_hMainBrush;
    }
    if (hCtrl == g_hStatusText) {
        SetTextColor(hdc, kColorTextMuted);
        return g_hMainBrush;
    }
    return g_hMainBrush;
}

static void SetControlVisibility(HWND hWnd, bool visible) {
    if (hWnd) {
        ShowWindow(hWnd, visible ? SW_SHOW : SW_HIDE);
    }
}

static RECT GetRightTabPageRect(HWND hContainer) {
    RECT rc{};
    if (!hContainer) return rc;

    GetClientRect(hContainer, &rc);
    const int padding = 4;
    const int buttonHeight = 40;
    const int buttonTop = 6;
    const int gapBelowButtons = 8;
    rc.left += padding;
    rc.right -= padding;
    rc.top = buttonTop + buttonHeight + gapBelowButtons;
    rc.bottom -= padding;
    return rc;
}

static void UpdateRightTabVisibility() {
    bool showFileView = (g_activeRightTab == RIGHT_TAB_FILE_VIEW);
    SetControlVisibility(g_hFileTabPage, showFileView);
    SetControlVisibility(g_hScheduleTabPage, !showFileView);

    if (g_hRightTabFileBtn) InvalidateRect(g_hRightTabFileBtn, nullptr, TRUE);
    if (g_hRightTabScheduleBtn) InvalidateRect(g_hRightTabScheduleBtn, nullptr, TRUE);

    HWND activePage = showFileView ? g_hFileTabPage : g_hScheduleTabPage;
    if (activePage) {
        SetWindowPos(activePage, HWND_TOP, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        InvalidateRect(activePage, nullptr, TRUE);
        UpdateWindow(activePage);
    }
}

static void ActivateRightTab(int tabIndex) {
    int clampedTab = (tabIndex == RIGHT_TAB_SCHEDULE_VIEW) ? RIGHT_TAB_SCHEDULE_VIEW : RIGHT_TAB_FILE_VIEW;
    if (g_activeRightTab == clampedTab) {
        if (g_hRightTabs) InvalidateRect(g_hRightTabs, nullptr, TRUE);
        return;
    }

    g_activeRightTab = clampedTab;
    if (g_hMainWnd) {
        LayoutControls(g_hMainWnd);
    }
    if (g_activeRightTab == RIGHT_TAB_SCHEDULE_VIEW) {
        RedrawProcessTableHeader();
    }
}

static void RedrawProcessTableHeader() {
    if (!g_hProcessList) return;

    HWND hHeader = ListView_GetHeader(g_hProcessList);
    RedrawWindow(g_hProcessList, nullptr, nullptr,
        RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
    if (hHeader) {
        RedrawWindow(hHeader, nullptr, nullptr,
            RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_UPDATENOW);
    }
}

static void DrawRightTabButton(const DRAWITEMSTRUCT* dis) {
    if (!dis) return;

    HDC hdc = dis->hDC;
    RECT rect = dis->rcItem;
    bool isSelected = false;
    if (dis->CtlID == IDC_RIGHT_TAB_FILE_BTN) {
        isSelected = (g_activeRightTab == RIGHT_TAB_FILE_VIEW);
    } else if (dis->CtlID == IDC_RIGHT_TAB_SCHEDULE_BTN) {
        isSelected = (g_activeRightTab == RIGHT_TAB_SCHEDULE_VIEW);
    }

    int insetX = 4;
    int insetTop = 4;
    int insetBottom = 4;
    RECT buttonRect{
        rect.left + insetX,
        rect.top + insetTop,
        rect.right - insetX,
        rect.bottom - insetBottom
    };

    COLORREF fillColor = isSelected ? kColorTabActiveBg : kColorTabInactiveBg;
    COLORREF borderColor = isSelected ? kColorTabActiveBorder : kColorTabInactiveBorder;
    COLORREF textColor = isSelected ? RGB(255, 255, 255) : kColorTabInactiveText;

    HBRUSH fillBrush = CreateSolidBrush(fillColor);
    HPEN borderPen = CreatePen(PS_SOLID, 1, borderColor);
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, fillBrush));
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
    SetBkMode(hdc, TRANSPARENT);
    RoundRect(hdc, buttonRect.left, buttonRect.top, buttonRect.right, buttonRect.bottom, 12, 12);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(fillBrush);
    DeleteObject(borderPen);

    char textBuffer[128]{};
    GetWindowTextA(dis->hwndItem, textBuffer, static_cast<int>(sizeof(textBuffer)));

    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, g_hFontLabel ? g_hFontLabel : g_hFontUI));
    SetTextColor(hdc, textColor);
    RECT textRect = buttonRect;
    DrawTextA(hdc, textBuffer, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);
}

static void DrawActionButton(const DRAWITEMSTRUCT* dis) {
    if (!dis) return;

    HDC hdc = dis->hDC;
    RECT rect = dis->rcItem;
    bool isPressed = (dis->itemState & ODS_SELECTED) != 0;
    bool isDisabled = (dis->itemState & ODS_DISABLED) != 0;
    bool isScan = (dis->CtlID == IDC_LOAD_BTN);

    COLORREF fillColor = isScan ? kColorScanBtnBg : kColorRefreshBtnBg;
    COLORREF borderColor = isScan ? kColorScanBtnBorder : kColorRefreshBtnBorder;
    COLORREF textColor = isScan ? kColorScanBtnText : kColorRefreshBtnText;
    COLORREF pressedColor = isScan ? kColorScanBtnPressed : kColorRefreshBtnPressed;

    if (isPressed) {
        fillColor = pressedColor;
    }
    if (isDisabled) {
        fillColor = AdjustColor(fillColor, 18);
        borderColor = AdjustColor(borderColor, 30);
        textColor = RGB(150, 150, 150);
    }

    HBRUSH fillBrush = CreateSolidBrush(fillColor);
    HPEN borderPen = CreatePen(PS_SOLID, 1, borderColor);
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, fillBrush));
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
    SetBkMode(hdc, TRANSPARENT);

    RECT buttonRect = rect;
    InflateRect(&buttonRect, -1, -1);
    if (isPressed) {
        OffsetRect(&buttonRect, 0, 1);
    }
    RoundRect(hdc, buttonRect.left, buttonRect.top, buttonRect.right, buttonRect.bottom, 12, 12);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(fillBrush);
    DeleteObject(borderPen);

    char textBuffer[128]{};
    GetWindowTextA(dis->hwndItem, textBuffer, static_cast<int>(sizeof(textBuffer)));

    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, g_hFontLabel ? g_hFontLabel : g_hFontUI));
    SetTextColor(hdc, textColor);
    RECT textRect = buttonRect;
    if (isPressed) {
        OffsetRect(&textRect, 0, 1);
    }
    DrawTextA(hdc, textBuffer, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);

    if ((dis->itemState & ODS_FOCUS) != 0) {
        RECT focusRect = buttonRect;
        InflateRect(&focusRect, -8, -8);
        DrawFocusRect(hdc, &focusRect);
    }
}

static void PaintActionButton(HWND hwnd, HDC hdc) {
    if (!hwnd || !hdc) return;

    RECT rect{};
    GetClientRect(hwnd, &rect);
    int state = static_cast<int>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
    bool isPressed = (state & ACTIONBTN_PRESSED) != 0;
    bool isHover = (state & ACTIONBTN_HOVER) != 0;
    bool isDisabled = !IsWindowEnabled(hwnd);
    bool isScan = (GetDlgCtrlID(hwnd) == IDC_LOAD_BTN);

    COLORREF fillColor = isScan ? kColorScanBtnBg : kColorRefreshBtnBg;
    COLORREF borderColor = isScan ? kColorScanBtnBorder : kColorRefreshBtnBorder;
    COLORREF textColor = isScan ? kColorScanBtnText : kColorRefreshBtnText;
    COLORREF hoverColor = isScan ? kColorScanBtnHover : kColorRefreshBtnHover;
    COLORREF pressedColor = isScan ? kColorScanBtnPressed : kColorRefreshBtnPressed;

    if (isPressed) {
        fillColor = pressedColor;
    } else if (isHover) {
        fillColor = hoverColor;
    }
    if (isDisabled) {
        fillColor = AdjustColor(fillColor, 18);
        borderColor = AdjustColor(borderColor, 24);
        textColor = RGB(150, 150, 150);
    }

    HBRUSH bgBrush = CreateSolidBrush(kColorWindowBg);
    FillRect(hdc, &rect, bgBrush);
    DeleteObject(bgBrush);

    HBRUSH fillBrush = CreateSolidBrush(fillColor);
    HPEN borderPen = CreatePen(PS_SOLID, 1, borderColor);
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, fillBrush));
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
    SetBkMode(hdc, TRANSPARENT);

    RECT buttonRect = rect;
    InflateRect(&buttonRect, -1, -1);
    if (isPressed) {
        OffsetRect(&buttonRect, 0, 1);
    }
    RoundRect(hdc, buttonRect.left, buttonRect.top, buttonRect.right, buttonRect.bottom, 12, 12);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(fillBrush);
    DeleteObject(borderPen);

    char textBuffer[128]{};
    GetWindowTextA(hwnd, textBuffer, static_cast<int>(sizeof(textBuffer)));
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, g_hFontLabel ? g_hFontLabel : g_hFontUI));
    SetTextColor(hdc, textColor);
    RECT textRect = buttonRect;
    if (isPressed) {
        OffsetRect(&textRect, 0, 1);
    }
    DrawTextA(hdc, textBuffer, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);
}

static void ApplyFontsToChildren(HWND) {
    ApplyControlFont(g_hDriveCombo, g_hFontUI);
    ApplyControlFont(g_hRefreshBtn, g_hFontUI);
    ApplyControlFont(g_hScanBtn, g_hFontUI);
    ApplyControlFont(g_hRightTabFileBtn, g_hFontLabel ? g_hFontLabel : g_hFontUI);
    ApplyControlFont(g_hRightTabScheduleBtn, g_hFontLabel ? g_hFontLabel : g_hFontUI);
    ApplyControlFont(g_hBootLabel, g_hFontLabel);
    ApplyControlFont(g_hFileLabel, g_hFontLabel);
    ApplyControlFont(g_hDetailLabel, g_hFontLabel);
    ApplyControlFont(g_hContentLabel, g_hFontLabel);
    ApplyControlFont(g_hGanttLabel, g_hFontLabel);
    ApplyControlFont(g_hProcessLabel, g_hFontLabel);
    ApplyControlFont(g_hBootList, g_hFontUI);
    ApplyControlFont(g_hFileList, g_hFontUI);
    ApplyControlFont(g_hDetailEdit, g_hFontMono);
    ApplyControlFont(g_hContentEdit, g_hFontMono);
    ApplyControlFont(g_hProcessList, g_hFontUI);
    ApplyControlFont(g_hStatusText, g_hFontUI);
    ApplyControlFont(g_hBootList ? ListView_GetHeader(g_hBootList) : nullptr, g_hFontUI);
    ApplyControlFont(g_hProcessList ? ListView_GetHeader(g_hProcessList) : nullptr, g_hFontUI);
}

static void AddColumn(HWND hList, int index, int width, const char* title) {
    LVCOLUMNA col{};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    col.cx = width;
    col.pszText = const_cast<char*>(title);
    col.iSubItem = index;
    ListView_InsertColumn(hList, index, &col);
}

static void ResizeProcessColumns() {
    if (!g_hProcessList) return;

    RECT rc{};
    GetClientRect(g_hProcessList, &rc);
    int totalWidth = rc.right - rc.left;
    if (totalWidth <= 0) return;

    if ((GetWindowLongA(g_hProcessList, GWL_STYLE) & WS_VSCROLL) != 0) {
        totalWidth -= GetSystemMetrics(SM_CXVSCROLL);
    }
    if (totalWidth <= 0) return;

    const int columnCount = 9;
    const int baseWidth = totalWidth / columnCount;
    int remainder = totalWidth % columnCount;

    for (int i = 0; i < columnCount; ++i) {
        int width = baseWidth;
        if (remainder > 0) {
            ++width;
            --remainder;
        }
        ListView_SetColumnWidth(g_hProcessList, i, width);
    }

    RedrawProcessTableHeader();
}

static void ResizeBootColumns() {
    if (!g_hBootList) return;

    HWND hHeader = ListView_GetHeader(g_hBootList);
    RECT rc{};
    if (hHeader) {
        GetClientRect(hHeader, &rc);
    } else {
        GetClientRect(g_hBootList, &rc);
    }

    int totalWidth = rc.right - rc.left;
    if (totalWidth <= 0) return;

    int fieldWidth = totalWidth / 2;
    int valueWidth = totalWidth - fieldWidth;
    ListView_SetColumnWidth(g_hBootList, 0, fieldWidth);
    ListView_SetColumnWidth(g_hBootList, 1, valueWidth);
}

static void AddBootRow(const std::string& field, const std::string& value) {
    LVITEMA item{};
    item.mask = LVIF_TEXT;
    item.iItem = ListView_GetItemCount(g_hBootList);
    item.iSubItem = 0;
    item.pszText = const_cast<char*>(field.c_str());
    int row = ListView_InsertItem(g_hBootList, &item);
    ListView_SetItemText(g_hBootList, row, 1, const_cast<char*>(value.c_str()));
}

static void ClearBootList() {
    ListView_DeleteAllItems(g_hBootList);
}

static void ClearProcessList() {
    ListView_DeleteAllItems(g_hProcessList);
}

static void DestroyGanttCanvasCache() {
    if (g_hGanttCanvasBitmap) {
        DeleteObject(g_hGanttCanvasBitmap);
        g_hGanttCanvasBitmap = nullptr;
    }
    g_ganttCanvasWidth = 0;
    g_ganttCanvasHeight = 0;
    g_ganttCanvasDirty = true;
}

static void InvalidateGanttCanvasCache(bool destroyBitmap = false) {
    g_ganttCanvasDirty = true;
    if (destroyBitmap) {
        DestroyGanttCanvasCache();
    }
}

static void ClearFileViews() {
    SetWindowTextA(g_hDetailEdit, "");
    SetWindowTextA(g_hContentEdit, "");
    SendMessageA(g_hDetailEdit, EM_SETSEL, 0, 0);
    SendMessageA(g_hContentEdit, EM_SETSEL, 0, 0);
    ClearProcessList();
    InvalidateGanttCanvasCache(true);
}

static void ClearProcessHighlight() {
    if (!g_hProcessList) return;

    int count = ListView_GetItemCount(g_hProcessList);
    for (int i = 0; i < count; ++i) {
        ListView_SetItemState(g_hProcessList, i, 0, LVIS_SELECTED | LVIS_FOCUSED);
    }
}

static void RedrawGanttChart() {
    if (g_hGanttChart) {
        InvalidateRect(g_hGanttChart, nullptr, FALSE);
        UpdateWindow(g_hGanttChart);
    }
}

static int GetTimelineEndTime() {
    int totalEndTime = 0;
    for (const auto& event : g_currentSimulation.timeline) {
        totalEndTime = std::max(totalEndTime, event.EndTime);
    }
    return std::max(totalEndTime, 1);
}

static int GetGanttPixelsPerTimeUnit() {
    return 96;
}

static int GetGanttWorldLeftPadding() {
    return 72;
}

static int GetGanttWorldRightPadding() {
    return 56;
}

static int GetGanttContentWidth() {
    const int totalEndTime = GetTimelineEndTime();
    const int widthFromTime = totalEndTime * GetGanttPixelsPerTimeUnit();
    const int widthFromEvents = static_cast<int>(g_currentSimulation.timeline.size()) * 120;
    return GetGanttWorldLeftPadding() + GetGanttWorldRightPadding() +
           std::max(1100, std::max(widthFromTime, widthFromEvents));
}

static void UpdateGanttScrollInfo() {
    if (!g_hGanttChart) return;

    RECT rc{};
    GetClientRect(g_hGanttChart, &rc);
    const int viewportWidth = std::max(1, static_cast<int>(rc.right - rc.left));
    const int contentWidth = (g_hasSimulationData && g_currentSimulation.ok && !g_currentSimulation.timeline.empty())
        ? std::max(GetGanttContentWidth(), viewportWidth)
        : viewportWidth;

    const int maxScroll = std::max(0, contentWidth - viewportWidth);
    g_ganttScrollX = std::clamp(g_ganttScrollX, 0, maxScroll);

    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_PAGE | SIF_RANGE | SIF_POS;
    si.nMin = 0;
    si.nMax = std::max(contentWidth - 1, 0);
    si.nPage = static_cast<UINT>(viewportWidth);
    si.nPos = g_ganttScrollX;
    SetScrollInfo(g_hGanttChart, SB_HORZ, &si, TRUE);

}

static bool HandleGanttHScroll(HWND hwnd, WPARAM wParam) {
    if (!hwnd) return false;

    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    GetScrollInfo(hwnd, SB_HORZ, &si);

    int newPos = g_ganttScrollX;
    switch (LOWORD(wParam)) {
    case SB_LINELEFT:
        newPos -= 40;
        break;
    case SB_LINERIGHT:
        newPos += 40;
        break;
    case SB_PAGELEFT:
        newPos -= static_cast<int>(si.nPage);
        break;
    case SB_PAGERIGHT:
        newPos += static_cast<int>(si.nPage);
        break;
    case SB_THUMBPOSITION:
    case SB_THUMBTRACK:
        newPos = si.nTrackPos;
        break;
    case SB_LEFT:
        newPos = si.nMin;
        break;
    case SB_RIGHT:
        newPos = si.nMax;
        break;
    default:
        return false;
    }

    const int maxScroll = std::max(0, si.nMax - static_cast<int>(si.nPage) + 1);
    const int oldScrollX = g_ganttScrollX;
    g_ganttScrollX = std::clamp(newPos, 0, maxScroll);
    SetScrollPos(hwnd, SB_HORZ, g_ganttScrollX, TRUE);

    if (g_ganttScrollX != oldScrollX) {
        InvalidateRect(hwnd, nullptr, FALSE);
        UpdateWindow(hwnd);
    }
    return true;
}

static void HighlightProcessRow(const std::string& processId) {
    if (!g_hProcessList) return;

    ClearProcessHighlight();
    int count = ListView_GetItemCount(g_hProcessList);
    char buffer[256]{};

    for (int i = 0; i < count; ++i) {
        ListView_GetItemText(g_hProcessList, i, 0, buffer, static_cast<int>(sizeof(buffer)));
        if (processId == buffer) {
            ListView_SetItemState(g_hProcessList, i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(g_hProcessList, i, FALSE);
            break;
        }
    }
}

static ParsedSchedule ParseScheduleText(const std::string& text) {
    ParsedSchedule parsed;
    std::vector<std::string> lines;
    std::istringstream iss(StripUtf8Bom(text));
    std::string line;

    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        line = TrimCopy(line);
        if (!line.empty()) {
            lines.push_back(line);
        }
    }

    if (lines.empty()) {
        parsed.error = "The file is empty.";
        return parsed;
    }

    std::istringstream header(lines[0]);
    if (!(header >> parsed.queueCount)) {
        parsed.error = "Could not read the queue count on the first non-empty line.";
        return parsed;
    }

    if (parsed.queueCount < 0) {
        parsed.error = "The queue count is invalid.";
        return parsed;
    }

    if (static_cast<int>(lines.size()) < parsed.queueCount + 1) {
        parsed.error = "The file does not contain enough queue definitions.";
        return parsed;
    }

    for (int i = 0; i < parsed.queueCount; ++i) {
        QueueInfo q;
        std::istringstream queueLine(lines[i + 1]);
        if (!(queueLine >> q.id >> q.timeSlice >> q.algorithm)) {
            std::ostringstream oss;
            oss << "Could not parse queue #" << (i + 1) << ".";
            parsed.error = oss.str();
            return parsed;
        }
        parsed.queues.push_back(q);
    }

    for (size_t i = static_cast<size_t>(parsed.queueCount) + 1; i < lines.size(); ++i) {
        ProcessInfo p;
        std::istringstream processLine(lines[i]);
        if (!(processLine >> p.id >> p.arrivalTime >> p.burstTime >> p.queueId)) {
            std::ostringstream oss;
            oss << "Could not parse process entry on line " << (i + 1) << ".";
            parsed.error = oss.str();
            return parsed;
        }
        parsed.processes.push_back(p);
    }

    parsed.ok = true;
    return parsed;
}

static SimulationResult RunScheduleSimulation(const ParsedSchedule& parsed) {
    SimulationResult result;

    if (!parsed.ok) {
        result.error = "Cannot simulate because the schedule input is invalid.";
        return result;
    }

    if (parsed.queues.empty()) {
        result.ok = true;
        result.error = "No queues were defined, so nothing was simulated.";
        return result;
    }

    Scheduler scheduler;
    for (const auto& queue : parsed.queues) {
        scheduler.addQueue(Queue(queue.id, queue.timeSlice, queue.algorithm));
    }

    for (const auto& process : parsed.processes) {
        scheduler.addProcess(Process(process.id, process.arrivalTime, process.burstTime, process.queueId));
    }

    scheduler.runScheduling();
    result.timeline = scheduler.getTimeline();
    result.processes = scheduler.getProcesses();
    result.ok = true;
    return result;
}

static std::string BuildSimulationSummary(const SimulationResult& simulation) {
    if (!simulation.ok) {
        return "Simulation error: " + simulation.error + "\r\n";
    }

    std::ostringstream summary;
    summary << "Simulation events: " << simulation.timeline.size() << "\r\n";

    if (simulation.processes.empty()) {
        summary << "No processes to execute.\r\n";
        return summary.str();
    }

    double turnaroundSum = 0.0;
    double waitingSum = 0.0;
    for (auto process : simulation.processes) {
        turnaroundSum += process.getTurnaroundTime();
        waitingSum += process.getWaitingTime();
    }

    summary << "Average turnaround: " << std::fixed << std::setprecision(2)
            << (turnaroundSum / simulation.processes.size()) << "\r\n";
    summary << "Average waiting: " << std::fixed << std::setprecision(2)
            << (waitingSum / simulation.processes.size()) << "\r\n";

    if (!simulation.timeline.empty()) {
        summary << "Timeline preview:\r\n";
        for (const auto& event : simulation.timeline) {
            summary << "[" << event.StartTime << " - " << event.EndTime << "] "
                    << event.QueueID << " -> " << event.ProcessID << "\r\n";
        }
    }

    return summary.str();
}

static void RefreshDetailPane() {
    SetWindowTextA(g_hDetailEdit, g_currentDetailBase.c_str());
    SendMessageA(g_hDetailEdit, EM_SETSEL, 0, 0);
    SendMessageA(g_hDetailEdit, EM_SCROLLCARET, 0, 0);
}

static RECT GetGanttInnerRect(const RECT& clientRect) {
    RECT inner = clientRect;
    InflateRect(&inner, -14, -12);
    return inner;
}

static RECT GetGanttCanvasViewportRect(const RECT& clientRect) {
    RECT viewport = GetGanttInnerRect(clientRect);
    viewport.top += 28;
    return viewport;
}

static void PaintGanttChartFrame(HDC hdc, const RECT& clientRect) {
    HBRUSH bgBrush = CreateSolidBrush(kColorCardBg);
    FillRect(hdc, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(210, 216, 224));
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
    Rectangle(hdc, clientRect.left, clientRect.top, clientRect.right, clientRect.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, kColorTextMain);
}

static void PaintGanttCanvas(HDC hdc, const RECT& canvasRect) {
    HBRUSH bgBrush = CreateSolidBrush(kColorCardBg);
    FillRect(hdc, &canvasRect, bgBrush);
    DeleteObject(bgBrush);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, kColorTextMain);
    HPEN oldPen = nullptr;
    HBRUSH oldBrush = nullptr;

    if (canvasRect.right <= canvasRect.left || canvasRect.bottom <= canvasRect.top) return;

    if (!g_hasSimulationData || !g_currentSimulation.ok || g_currentSimulation.timeline.empty()) {
        return;
    }

    const int totalEndTime = GetTimelineEndTime();
    const int contentWidth = GetGanttContentWidth();
    const int axisLeftWorld = GetGanttWorldLeftPadding();
    const int axisRightWorld = contentWidth - GetGanttWorldRightPadding();
    const int axisTop = canvasRect.top + 12;
    const int axisBottom = canvasRect.bottom - 34;
    const int axisWidthWorld = std::max(1, axisRightWorld - axisLeftWorld);
    const int barHeight = std::min(68, std::max(48, axisBottom - axisTop - 18));
    const int barTop = axisTop + std::max(0, (axisBottom - axisTop - barHeight) / 2);
    const int barBottom = barTop + barHeight;
    const int axisY = barBottom + 12;

    int axisLeft = canvasRect.left + axisLeftWorld;
    int axisRight = canvasRect.left + axisRightWorld;

    HBRUSH stripeBrush = CreateSolidBrush(RGB(248, 250, 252));
    HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(228, 232, 238));
    HPEN oldGridPen = static_cast<HPEN>(SelectObject(hdc, gridPen));

    for (int timeValue = 0; timeValue < totalEndTime; ++timeValue) {
        int x1 = canvasRect.left + axisLeftWorld + (timeValue * axisWidthWorld) / totalEndTime;
        int x2 = canvasRect.left + axisLeftWorld + ((timeValue + 1) * axisWidthWorld) / totalEndTime;

        if (timeValue % 2 == 1) {
            RECT stripeRect{ x1, axisTop - 6, x2, axisY };
            if (stripeRect.right > stripeRect.left) {
                FillRect(hdc, &stripeRect, stripeBrush);
            }
        }

        MoveToEx(hdc, x1, axisTop - 6, nullptr);
        LineTo(hdc, x1, axisY + 1);
    }

    MoveToEx(hdc, axisRight, axisTop - 6, nullptr);
    LineTo(hdc, axisRight, axisY + 1);

    SelectObject(hdc, oldGridPen);
    DeleteObject(gridPen);
    DeleteObject(stripeBrush);

    HPEN axisPen = CreatePen(PS_SOLID, 1, RGB(170, 176, 186));
    oldPen = static_cast<HPEN>(SelectObject(hdc, axisPen));
    MoveToEx(hdc, axisLeft, axisY, nullptr);
    LineTo(hdc, axisRight, axisY);

    for (int timeValue = 0; timeValue <= totalEndTime; ++timeValue) {
        int x = canvasRect.left + axisLeftWorld + (timeValue * axisWidthWorld) / totalEndTime;

        MoveToEx(hdc, x, axisY, nullptr);
        LineTo(hdc, x, axisY + 7);

        RECT tickRect{ x - 20, axisY + 10, x + 20, canvasRect.bottom };
        std::string timeText = std::to_string(timeValue);
        DrawTextA(hdc, timeText.c_str(), -1, &tickRect, DT_CENTER | DT_TOP | DT_SINGLELINE);
    }

    SelectObject(hdc, oldPen);
    DeleteObject(axisPen);

    int savedDc = SaveDC(hdc);
    IntersectClipRect(hdc, canvasRect.left, canvasRect.top, canvasRect.right, canvasRect.bottom);

    for (size_t i = 0; i < g_currentSimulation.timeline.size(); ++i) {
        const auto& event = g_currentSimulation.timeline[i];
        int left = canvasRect.left + axisLeftWorld + (event.StartTime * axisWidthWorld) / totalEndTime;
        int right = canvasRect.left + axisLeftWorld + (event.EndTime * axisWidthWorld) / totalEndTime;
        if (right <= left) right = left + 1;
        if (right - left < 56) right = left + 56;

        RECT barRect{ left, barTop, right, barBottom };
        if (barRect.right <= barRect.left) continue;
        COLORREF fillColor = ColorFromKey(event.QueueID + ":" + event.ProcessID);
        COLORREF borderColor = AdjustColor(fillColor, -26);
        HBRUSH fillBrush = CreateSolidBrush(fillColor);
        HPEN barPen = CreatePen(PS_SOLID, 1, borderColor);
        oldBrush = static_cast<HBRUSH>(SelectObject(hdc, fillBrush));
        oldPen = static_cast<HPEN>(SelectObject(hdc, barPen));
        RoundRect(hdc, barRect.left, barRect.top, barRect.right, barRect.bottom, 10, 10);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(fillBrush);
        DeleteObject(barPen);

        COLORREF textColor = GetContrastingTextColor(fillColor);
        int visibleWidth = barRect.right - barRect.left;

        RECT processRect = barRect;
        processRect.top += 8;
        processRect.bottom = (visibleWidth >= 88) ? (barRect.top + (barHeight / 2) + 4) : (barRect.bottom - 8);
        InflateRect(&processRect, -8, 0);
        SetTextColor(hdc, textColor);
        HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, g_hFontLabel ? g_hFontLabel : g_hFontUI));
        DrawTextA(hdc, event.ProcessID.c_str(), -1, &processRect,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        if (visibleWidth >= 88) {
            RECT queueRect = barRect;
            queueRect.top = processRect.bottom - 2;
            queueRect.bottom = barRect.bottom - 8;
            InflateRect(&queueRect, -8, 0);
            SelectObject(hdc, g_hFontUI ? g_hFontUI : oldFont);
            DrawTextA(hdc, event.QueueID.c_str(), -1, &queueRect,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }

        SelectObject(hdc, oldFont);
    }

    RestoreDC(hdc, savedDc);

    SetTextColor(hdc, kColorTextMain);
}

static void EnsureGanttCanvasCache(HDC referenceHdc, const RECT& viewportRect) {
    if (!referenceHdc) return;

    if (!g_hasSimulationData || !g_currentSimulation.ok || g_currentSimulation.timeline.empty()) {
        DestroyGanttCanvasCache();
        return;
    }

    const int desiredWidth = std::max(GetGanttContentWidth(),
        std::max(1, static_cast<int>(viewportRect.right - viewportRect.left)));
    const int desiredHeight = std::max(1, static_cast<int>(viewportRect.bottom - viewportRect.top));

    if (!g_ganttCanvasDirty &&
        g_hGanttCanvasBitmap &&
        g_ganttCanvasWidth == desiredWidth &&
        g_ganttCanvasHeight == desiredHeight) {
        return;
    }

    HDC memDc = CreateCompatibleDC(referenceHdc);
    HBITMAP newBitmap = CreateCompatibleBitmap(referenceHdc, desiredWidth, desiredHeight);
    if (!memDc || !newBitmap) {
        if (newBitmap) DeleteObject(newBitmap);
        if (memDc) DeleteDC(memDc);
        return;
    }

    HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(memDc, newBitmap));
    RECT bitmapRect{ 0, 0, desiredWidth, desiredHeight };
    PaintGanttCanvas(memDc, bitmapRect);
    SelectObject(memDc, oldBitmap);
    DeleteDC(memDc);

    if (g_hGanttCanvasBitmap) {
        DeleteObject(g_hGanttCanvasBitmap);
    }
    g_hGanttCanvasBitmap = newBitmap;
    g_ganttCanvasWidth = desiredWidth;
    g_ganttCanvasHeight = desiredHeight;
    g_ganttCanvasDirty = false;
}

static void PaintGanttChart(HDC hdc, const RECT& clientRect) {
    PaintGanttChartFrame(hdc, clientRect);

    RECT inner = GetGanttInnerRect(clientRect);
    if (inner.right <= inner.left || inner.bottom <= inner.top) return;

    RECT titleRect{ inner.left, inner.top - 2, inner.right, inner.top + 24 };
    std::string headerText = "Final Gantt Chart";
    DrawTextA(hdc, headerText.c_str(), -1, &titleRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT viewportRect = GetGanttCanvasViewportRect(clientRect);
    if (viewportRect.right <= viewportRect.left || viewportRect.bottom <= viewportRect.top) return;

    if (!g_hasSimulationData || !g_currentSimulation.ok || g_currentSimulation.timeline.empty()) {
        DrawTextA(hdc, "Load a valid schedule to see the Gantt chart.", -1,
                  &viewportRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    EnsureGanttCanvasCache(hdc, viewportRect);

    if (!g_hGanttCanvasBitmap) {
        DrawTextA(hdc, "Could not render the Gantt chart.", -1,
                  &viewportRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    HDC cacheDc = CreateCompatibleDC(hdc);
    if (!cacheDc) return;

    HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(cacheDc, g_hGanttCanvasBitmap));
    const int viewportWidth = viewportRect.right - viewportRect.left;
    const int viewportHeight = viewportRect.bottom - viewportRect.top;
    BitBlt(hdc, viewportRect.left, viewportRect.top, viewportWidth, viewportHeight,
           cacheDc, g_ganttScrollX, 0, SRCCOPY);
    SelectObject(cacheDc, oldBitmap);
    DeleteDC(cacheDc);
}

static LRESULT CALLBACK GanttChartProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;

    case WM_SIZE:
        InvalidateGanttCanvasCache();
        UpdateGanttScrollInfo();
        break;

    case WM_MOUSEWHEEL: {
        const short delta = GET_WHEEL_DELTA_WPARAM(wParam);
        const int step = 60;
        const int direction = delta > 0 ? -step : step;
        SendMessageA(hwnd, WM_HSCROLL, MAKEWPARAM(SB_THUMBPOSITION, std::max(0, g_ganttScrollX + direction)), 0);
        return 0;
    }

    case WM_HSCROLL: {
        if (HandleGanttHScroll(hwnd, wParam)) {
            return 0;
        }
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc{};
        GetClientRect(hwnd, &rc);

        const int width = std::max(1, static_cast<int>(rc.right - rc.left));
        const int height = std::max(1, static_cast<int>(rc.bottom - rc.top));
        HDC memDc = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap(hdc, width, height);

        if (memDc && memBitmap) {
            HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(memDc, memBitmap));
            PaintGanttChart(memDc, rc);
            BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top,
                   ps.rcPaint.right - ps.rcPaint.left,
                   ps.rcPaint.bottom - ps.rcPaint.top,
                   memDc, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
            SelectObject(memDc, oldBitmap);
        } else {
            PaintGanttChart(hdc, rc);
        }

        if (memBitmap) DeleteObject(memBitmap);
        if (memDc) DeleteDC(memDc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK ActionButtonProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;

    case WM_ENABLE:
    case WM_SETTEXT:
        InvalidateRect(hwnd, nullptr, TRUE);
        break;

    case WM_MOUSEMOVE: {
        int state = static_cast<int>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
        if ((state & ACTIONBTN_HOVER) == 0) {
            state |= ACTIONBTN_HOVER;
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, state);
            TRACKMOUSEEVENT tme{};
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;
    }

    case WM_MOUSELEAVE: {
        int state = static_cast<int>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
        state &= ~(ACTIONBTN_HOVER | ACTIONBTN_PRESSED);
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, state);
        ReleaseCapture();
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        if (!IsWindowEnabled(hwnd)) return 0;
        int state = static_cast<int>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
        state |= ACTIONBTN_PRESSED | ACTIONBTN_HOVER;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, state);
        SetCapture(hwnd);
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    }

    case WM_LBUTTONUP: {
        int state = static_cast<int>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
        bool wasPressed = (state & ACTIONBTN_PRESSED) != 0;
        state &= ~ACTIONBTN_PRESSED;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, state);
        ReleaseCapture();
        InvalidateRect(hwnd, nullptr, TRUE);

        if (wasPressed) {
            POINT pt{
                static_cast<short>(LOWORD(lParam)),
                static_cast<short>(HIWORD(lParam))
            };
            RECT rc{};
            GetClientRect(hwnd, &rc);
            if (PtInRect(&rc, pt)) {
                HWND parent = GetParent(hwnd);
                if (parent) {
                    SendMessageA(parent, WM_COMMAND,
                        MAKEWPARAM(GetDlgCtrlID(hwnd), BN_CLICKED),
                        reinterpret_cast<LPARAM>(hwnd));
                }
            }
        }
        return 0;
    }

    case WM_CAPTURECHANGED: {
        int state = static_cast<int>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
        if ((state & ACTIONBTN_PRESSED) != 0) {
            state &= ~ACTIONBTN_PRESSED;
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, state);
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        PaintActionButton(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK RightTabPageProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND: {
        RECT rc{};
        GetClientRect(hwnd, &rc);
        FillRect(reinterpret_cast<HDC>(wParam), &rc, g_hMainBrush);
        return 1;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);
        if (code == BN_CLICKED) {
            if (id == IDC_RIGHT_TAB_FILE_BTN) {
                ActivateRightTab(RIGHT_TAB_FILE_VIEW);
                return 0;
            }
            if (id == IDC_RIGHT_TAB_SCHEDULE_BTN) {
                ActivateRightTab(RIGHT_TAB_SCHEDULE_VIEW);
                return 0;
            }
        }
        break;
    }

    case WM_DRAWITEM: {
        const DRAWITEMSTRUCT* dis = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (dis && (dis->CtlID == IDC_REFRESH_BTN || dis->CtlID == IDC_LOAD_BTN)) {
            DrawActionButton(dis);
            return TRUE;
        }
        if (dis && (dis->CtlID == IDC_RIGHT_TAB_FILE_BTN || dis->CtlID == IDC_RIGHT_TAB_SCHEDULE_BTN)) {
            DrawRightTabButton(dis);
            return TRUE;
        }
        break;
    }

    case WM_CTLCOLORSTATIC: {
        HBRUSH brush = ApplySharedControlColors(reinterpret_cast<HWND>(lParam), reinterpret_cast<HDC>(wParam));
        return reinterpret_cast<INT_PTR>(brush ? brush : g_hMainBrush);
    }

    case WM_CTLCOLOREDIT: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetBkColor(hdc, kColorCardBg);
        SetTextColor(hdc, kColorTextMain);
        return reinterpret_cast<INT_PTR>(g_hCardBrush);
    }

    case WM_CTLCOLORLISTBOX: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetBkColor(hdc, kColorCardBg);
        SetTextColor(hdc, kColorTextMain);
        return reinterpret_cast<INT_PTR>(g_hCardBrush);
    }

    case WM_NOTIFY: {
        NMHDR* hdr = reinterpret_cast<NMHDR*>(lParam);
        HWND processHeader = g_hProcessList ? ListView_GetHeader(g_hProcessList) : nullptr;
        if (hdr && hdr->hwndFrom == processHeader) {
            switch (hdr->code) {
            case HDN_BEGINTRACKA:
            case HDN_BEGINTRACKW:
            case HDN_TRACKA:
            case HDN_TRACKW:
            case HDN_DIVIDERDBLCLICKA:
            case HDN_DIVIDERDBLCLICKW:
                return TRUE;
            default:
                break;
            }
        }
        break;
    }
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static void RefreshSimulationView() {
    RefreshDetailPane();
    RedrawGanttChart();
}

static void PopulateBootInfo() {
    ClearBootList();

    if (!g_boot.isValid()) return;

    AddBootRow("Bytes per sector", std::to_string(g_boot.getBytesPerSector()));
    AddBootRow("Sectors per cluster", std::to_string(g_boot.getSectorsPerCluster()));
    AddBootRow("Reserved sectors", std::to_string(g_boot.getReservedSectors()));
    AddBootRow("FAT copies", std::to_string(g_boot.getNumFATs()));
    AddBootRow("Sectors per FAT", std::to_string(g_boot.getSectorsPerFAT()));
    AddBootRow("RDET sectors", std::to_string(g_boot.getRDETSectors()));
    AddBootRow("Total sectors", std::to_string(g_boot.getTotalSectors()));
}

static void PopulateFileList() {
    SendMessageA(g_hFileList, LB_RESETCONTENT, 0, 0);

    std::sort(g_txtFiles.begin(), g_txtFiles.end(), [](const FileInfo& a, const FileInfo& b) {
        return a.fullPath < b.fullPath;
    });

    for (const auto& file : g_txtFiles) {
        SendMessageA(g_hFileList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(file.fullPath.c_str()));
    }
}

static void PopulateProcessList(const ParsedSchedule& parsed, SimulationResult* simulation = nullptr) {
    ClearProcessList();

    for (const auto& proc : parsed.processes) {
        const QueueInfo* queue = FindQueueById(parsed.queues, proc.queueId);
        Process* simulatedProcess = simulation ? FindProcessById(simulation->processes, proc.id) : nullptr;
        std::string timeSlice = queue ? std::to_string(queue->timeSlice) : "N/A";
        std::string algorithm = queue ? queue->algorithm : "N/A";
        std::string completion = simulatedProcess ? std::to_string(simulatedProcess->getCompletionTime()) : "N/A";
        std::string turnaround = simulatedProcess ? std::to_string(simulatedProcess->getTurnaroundTime()) : "N/A";
        std::string waiting = simulatedProcess ? std::to_string(simulatedProcess->getWaitingTime()) : "N/A";

        LVITEMA item{};
        item.mask = LVIF_TEXT;
        item.iItem = ListView_GetItemCount(g_hProcessList);
        item.iSubItem = 0;
        item.pszText = const_cast<char*>(proc.id.c_str());
        int row = ListView_InsertItem(g_hProcessList, &item);

        std::string arrival = std::to_string(proc.arrivalTime);
        std::string burst = std::to_string(proc.burstTime);
        std::string queueId = proc.queueId;

        ListView_SetItemText(g_hProcessList, row, 1, const_cast<char*>(arrival.c_str()));
        ListView_SetItemText(g_hProcessList, row, 2, const_cast<char*>(burst.c_str()));
        ListView_SetItemText(g_hProcessList, row, 3, const_cast<char*>(queueId.c_str()));
        ListView_SetItemText(g_hProcessList, row, 4, const_cast<char*>(timeSlice.c_str()));
        ListView_SetItemText(g_hProcessList, row, 5, const_cast<char*>(algorithm.c_str()));
        ListView_SetItemText(g_hProcessList, row, 6, const_cast<char*>(completion.c_str()));
        ListView_SetItemText(g_hProcessList, row, 7, const_cast<char*>(turnaround.c_str()));
        ListView_SetItemText(g_hProcessList, row, 8, const_cast<char*>(waiting.c_str()));
    }
}

static void ShowSelectedFileInfo(int index) {
    ClearProcessHighlight();

    if (!g_hasLoadedDrive) {
        SetStatusText("Click Scan first, then select a .txt file to preview.");
        return;
    }

    if (index < 0 || index >= static_cast<int>(g_txtFiles.size())) {
        ClearFileViews();
        return;
    }

    const FileInfo& file = g_txtFiles[index];
    std::vector<uint8_t> bytes = g_directoryScanner.readFileContent(file, g_reader, g_boot, g_fat);
    const uint32_t displayedSize = (file.fileSize == 0 && !bytes.empty())
        ? static_cast<uint32_t>(bytes.size())
        : file.fileSize;

    if (bytes.empty() && displayedSize > 0) {
        ShowError("Could not read the selected file content.");
        return;
    }

    std::string content = DecodeTextBytes(bytes);
    std::string displayContent = ToWindowsNewlines(content);
    SetWindowTextA(g_hContentEdit, displayContent.c_str());
    SendMessageA(g_hContentEdit, EM_SETSEL, 0, 0);
    SendMessageA(g_hContentEdit, EM_SCROLLCARET, 0, 0);

    ParsedSchedule parsed = ParseScheduleText(content);
    SimulationResult simulation;
    if (parsed.ok) {
        simulation = RunScheduleSimulation(parsed);
    }

    std::ostringstream detail;
    detail << "Name: " << file.name << "\r\n"
           << "Full path: " << file.fullPath << "\r\n"
           << "Created date: " << file.createdAt.toDateString() << "\r\n"
           << "Created time: " << file.createdAt.toTimeString() << "\r\n"
           << "Total size: " << displayedSize << " bytes\r\n";

    if (file.fileSize == 0 && displayedSize > 0) {
        detail << "Note: directory metadata reported 0 bytes, so the preview uses recovered cluster data.\r\n";
    }

    if (parsed.ok) {
        detail << "Queue count: " << parsed.queueCount << "\r\n"
               << "Process count: " << parsed.processes.size() << "\r\n"
               << BuildSimulationSummary(simulation);
    } else {
        detail << "Schedule parse: " << parsed.error << "\r\n";
    }

    g_currentDetailBase = detail.str();

    if (parsed.ok) {
        g_currentSchedule = parsed;
        g_currentSimulation = simulation;
        g_hasSimulationData = simulation.ok;
        g_ganttScrollX = 0;
        InvalidateGanttCanvasCache(true);
        PopulateProcessList(parsed, &simulation);
        UpdateGanttScrollInfo();
        RefreshSimulationView();
        SetStatusText("Loaded the selected file content and displayed the final schedule.");
    } else {
        g_currentSchedule = ParsedSchedule{};
        g_currentSimulation = SimulationResult{};
        g_hasSimulationData = false;
        g_ganttScrollX = 0;
        InvalidateGanttCanvasCache(true);
        ClearProcessList();
        UpdateGanttScrollInfo();
        RefreshSimulationView();
        SetStatusText("Opened the .txt file, but the scheduling content does not match the expected format.");
    }
}

static std::vector<std::string> GetAvailableDrives() {
    std::vector<std::string> removableDrives;
    std::vector<std::string> fixedDrives;
    char buffer[512]{};
    DWORD len = GetLogicalDriveStringsA(static_cast<DWORD>(sizeof(buffer) - 1), buffer);

    if (len == 0 || len > sizeof(buffer) - 1) {
        return {};
    }

    for (char* p = buffer; *p != '\0'; p += std::strlen(p) + 1) {
        UINT type = GetDriveTypeA(p);
        if (std::strlen(p) < 2 || p[1] != ':') {
            continue;
        }

        std::string drive(p, p + 2);
        if (type == DRIVE_REMOVABLE) {
            removableDrives.push_back(drive);
        } else if (type == DRIVE_FIXED) {
            fixedDrives.push_back(drive);
        }
    }

    std::sort(removableDrives.begin(), removableDrives.end());
    removableDrives.erase(std::unique(removableDrives.begin(), removableDrives.end()), removableDrives.end());

    std::sort(fixedDrives.begin(), fixedDrives.end());
    fixedDrives.erase(std::unique(fixedDrives.begin(), fixedDrives.end()), fixedDrives.end());

    std::vector<std::string> drives = removableDrives;
    drives.insert(drives.end(), fixedDrives.begin(), fixedDrives.end());
    return drives;
}

static void ResetLoadedData(bool keepStatus = false) {
    ClearProcessHighlight();
    if (g_reader.isOpen()) {
        g_reader.close();
    }
    ClearBootList();
    SendMessageA(g_hFileList, LB_RESETCONTENT, 0, 0);
    ClearFileViews();
    g_txtFiles.clear();
    g_currentSchedule = ParsedSchedule{};
    g_currentSimulation = SimulationResult{};
    g_currentDetailBase.clear();
    g_hasSimulationData = false;
    g_ganttScrollX = 0;
    InvalidateGanttCanvasCache(true);
    g_hasLoadedDrive = false;
    RefreshSimulationView();
    if (!keepStatus) {
        SetStatusText("Choose a drive, then click Scan.");
    }
}

static void RefreshDriveCombo() {
    SendMessageA(g_hDriveCombo, CB_RESETCONTENT, 0, 0);
    std::vector<std::string> drives = GetAvailableDrives();

    for (const auto& drive : drives) {
        SendMessageA(g_hDriveCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(drive.c_str()));
    }

    if (!drives.empty()) {
        bool selected = false;
        if (!g_initialDrive.empty()) {
            selected = SelectDriveInCombo(g_initialDrive);
        }
        if (!selected) {
            SendMessageA(g_hDriveCombo, CB_SETCURSEL, 0, 0);
        }
        ResetLoadedData(true);
        SetStatusText("Drive list refreshed. Choose a drive and click Scan.");
    } else {
        ResetLoadedData(true);
        SetStatusText("No supported drives were detected. Reconnect the device and click Refresh.");
    }
}

static std::string GetSelectedDrive() {
    char buffer[64]{};
    int idx = static_cast<int>(SendMessageA(g_hDriveCombo, CB_GETCURSEL, 0, 0));
    if (idx == CB_ERR) return "";
    SendMessageA(g_hDriveCombo, CB_GETLBTEXT, idx, reinterpret_cast<LPARAM>(buffer));
    return std::string(buffer);
}

static void LoadSelectedDrive() {
    std::string drive = GetSelectedDrive();
    if (drive.empty()) {
        ShowError("Select a drive before scanning.");
        return;
    }

    SetCursor(LoadCursor(nullptr, IDC_WAIT));
    SetStatusText("Reading the boot sector and scanning FAT32 data...");
    ResetLoadedData(true);

    if (!g_reader.open(drive)) {
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
        ShowError("Could not open the drive. Run the program as Administrator and make sure the drive is formatted as FAT32.");
        return;
    }

    if (!g_boot.load(g_reader)) {
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
        ShowError("Could not read the FAT32 boot sector from the selected drive.");
        return;
    }

    if (!g_fat.load(g_reader, g_boot)) {
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
        ShowError("Could not load the FAT table.");
        return;
    }

    g_txtFiles = g_directoryScanner.findAllTxtFiles(g_reader, g_boot, g_fat);
    g_hasLoadedDrive = true;

    PopulateBootInfo();
    PopulateFileList();
    ClearFileViews();
    SetCursor(LoadCursor(nullptr, IDC_ARROW));

    std::ostringstream status;
    status << "Drive " << drive << " scanned successfully. Found " << g_txtFiles.size()
           << " .txt files. Select a file to inspect its content.";
    SetStatusText(status.str());
}

static void CreateChildControls(HWND hwnd) {
    g_hDriveCombo = CreateWindowExA(0, WC_COMBOBOXA, "",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_DRIVE_COMBO), GetModuleHandle(nullptr), nullptr);

    g_hRefreshBtn = CreateWindowExA(0, kActionButtonClass, "Refresh",
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_REFRESH_BTN), GetModuleHandle(nullptr), nullptr);

    g_hScanBtn = CreateWindowExA(0, kActionButtonClass, "Scan",
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_LOAD_BTN), GetModuleHandle(nullptr), nullptr);

    g_hBootLabel = CreateWindowExA(0, "STATIC", "Boot Sector Overview",
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_BOOT_LABEL), GetModuleHandle(nullptr), nullptr);

    g_hBootList = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | WS_VSCROLL,
        0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_BOOT_LIST), GetModuleHandle(nullptr), nullptr);

    g_hFileLabel = CreateWindowExA(0, "STATIC", "Text Files (.txt)",
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_FILE_LABEL), GetModuleHandle(nullptr), nullptr);

    g_hFileList = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "",
        WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL | WS_BORDER,
        0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_FILE_LIST), GetModuleHandle(nullptr), nullptr);

    g_hRightTabs = CreateWindowExA(0, kRightTabPageClass, "",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_RIGHT_TABS), GetModuleHandle(nullptr), nullptr);

    g_hRightTabFileBtn = CreateWindowExA(0, "BUTTON", "File View",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | BS_PUSHBUTTON,
        0, 0, 0, 0, g_hRightTabs, reinterpret_cast<HMENU>(IDC_RIGHT_TAB_FILE_BTN), GetModuleHandle(nullptr), nullptr);

    g_hRightTabScheduleBtn = CreateWindowExA(0, "BUTTON", "Schedule View",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | BS_PUSHBUTTON,
        0, 0, 0, 0, g_hRightTabs, reinterpret_cast<HMENU>(IDC_RIGHT_TAB_SCHEDULE_BTN), GetModuleHandle(nullptr), nullptr);

    g_hFileTabPage = CreateWindowExA(0, kRightTabPageClass, "",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, 0, 0, 0, g_hRightTabs, nullptr, GetModuleHandle(nullptr), nullptr);

    g_hScheduleTabPage = CreateWindowExA(0, kRightTabPageClass, "",
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, 0, 0, 0, g_hRightTabs, nullptr, GetModuleHandle(nullptr), nullptr);

    g_hDetailLabel = CreateWindowExA(0, "STATIC", "Selected File Result",
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, g_hFileTabPage, reinterpret_cast<HMENU>(IDC_DETAIL_LABEL), GetModuleHandle(nullptr), nullptr);

    g_hDetailEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY,
        0, 0, 0, 0, g_hFileTabPage, reinterpret_cast<HMENU>(IDC_DETAIL_EDIT), GetModuleHandle(nullptr), nullptr);

    g_hContentLabel = CreateWindowExA(0, "STATIC", "File Content",
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, g_hFileTabPage, reinterpret_cast<HMENU>(IDC_CONTENT_LABEL), GetModuleHandle(nullptr), nullptr);

    g_hContentEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL |
        WS_VSCROLL | WS_HSCROLL | ES_READONLY,
        0, 0, 0, 0, g_hFileTabPage, reinterpret_cast<HMENU>(IDC_CONTENT_EDIT), GetModuleHandle(nullptr), nullptr);

    g_hGanttLabel = CreateWindowExA(0, "STATIC", "Gantt Chart",
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, g_hScheduleTabPage, reinterpret_cast<HMENU>(IDC_GANTT_LABEL), GetModuleHandle(nullptr), nullptr);

    g_hGanttChart = CreateWindowExA(WS_EX_CLIENTEDGE, kGanttChartClass, "",
        WS_CHILD | WS_VISIBLE | WS_HSCROLL,
        0, 0, 0, 0, g_hScheduleTabPage, reinterpret_cast<HMENU>(IDC_GANTT_CHART), GetModuleHandle(nullptr), nullptr);

    g_hProcessLabel = CreateWindowExA(0, "STATIC", "Scheduling Table",
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, g_hScheduleTabPage, reinterpret_cast<HMENU>(IDC_PROCESS_LABEL), GetModuleHandle(nullptr), nullptr);

    g_hProcessList = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
        0, 0, 0, 0, g_hScheduleTabPage, reinterpret_cast<HMENU>(IDC_PROCESS_LIST), GetModuleHandle(nullptr), nullptr);

    g_hStatusText = CreateWindowExA(0, "STATIC", "Choose a drive, then click Scan.",
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_STATUS_TEXT), GetModuleHandle(nullptr), nullptr);

    ListView_SetExtendedListViewStyle(g_hBootList,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    ListView_SetExtendedListViewStyle(g_hProcessList,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    ListView_SetBkColor(g_hBootList, kColorCardBg);
    ListView_SetTextBkColor(g_hBootList, kColorCardBg);
    ListView_SetTextColor(g_hBootList, kColorTextMain);
    ListView_SetBkColor(g_hProcessList, kColorCardBg);
    ListView_SetTextBkColor(g_hProcessList, kColorCardBg);
    ListView_SetTextColor(g_hProcessList, kColorTextMain);

    SendMessageA(g_hDriveCombo, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), 24);
    SendMessageA(g_hDriveCombo, CB_SETITEMHEIGHT, 0, 24);
    SendMessageA(g_hFileList, LB_SETITEMHEIGHT, 0, 22);
    SendMessageA(g_hDetailEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(10, 10));
    SendMessageA(g_hContentEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(10, 10));

    AddColumn(g_hBootList, 0, 250, "Field");
    AddColumn(g_hBootList, 1, 220, "Value");
    ResizeBootColumns();

    AddColumn(g_hProcessList, 0, 110, "Process ID");
    AddColumn(g_hProcessList, 1, 95, "Arrival");
    AddColumn(g_hProcessList, 2, 110, "CPU Burst");
    AddColumn(g_hProcessList, 3, 110, "Queue ID");
    AddColumn(g_hProcessList, 4, 100, "Time Slice");
    AddColumn(g_hProcessList, 5, 150, "Algorithm");
    AddColumn(g_hProcessList, 6, 100, "Completion");
    AddColumn(g_hProcessList, 7, 110, "Turnaround");
    AddColumn(g_hProcessList, 8, 90, "Waiting");
    ResizeProcessColumns();

    g_hFontUI = CreateAppFont(11, FW_NORMAL, "Segoe UI");
    g_hFontLabel = CreateAppFont(11, FW_SEMIBOLD, "Segoe UI");
    g_hFontMono = CreateAppFont(10, FW_NORMAL, "Consolas");
    ApplyFontsToChildren(hwnd);
    UpdateRightTabVisibility();

}

static void LayoutControls(HWND hwnd) {
    RECT rc{};
    GetClientRect(hwnd, &rc);

    const int margin = 16;
    const int gap = 12;
    const int sectionGap = 10;
    const int labelHeight = 24;
    const int topHeight = 34;
    const int statusHeight = 28;

    int clientW = rc.right - rc.left;
    int clientH = rc.bottom - rc.top;

    int usableTop = margin + topHeight + gap;
    int usableBottom = clientH - margin - statusHeight;
    int contentH = usableBottom - usableTop;

    int leftW = clientW * 33 / 100;
    int rightX = margin + leftW + gap;
    int rightW = clientW - rightX - margin;

    int bootAreaH = contentH * 35 / 100;
    int fileAreaH = contentH - bootAreaH - gap;

    const int comboW = 160;
    const int topGap = 8;
    const int refreshW = 96;
    const int scanW = 84;
    MoveWindow(g_hDriveCombo, margin, margin, comboW, topHeight, TRUE);
    MoveWindow(g_hRefreshBtn, margin + comboW + topGap, margin, refreshW, topHeight, TRUE);
    MoveWindow(g_hScanBtn, margin + comboW + topGap + refreshW + topGap, margin, scanW, topHeight, TRUE);

    MoveWindow(g_hBootLabel, margin, usableTop, leftW, labelHeight, TRUE);
    MoveWindow(g_hBootList, margin, usableTop + labelHeight, leftW, bootAreaH - labelHeight, TRUE);
    ResizeBootColumns();

    int fileY = usableTop + bootAreaH + gap;
    MoveWindow(g_hFileLabel, margin, fileY, leftW, labelHeight, TRUE);
    MoveWindow(g_hFileList, margin, fileY + labelHeight, leftW, fileAreaH - labelHeight, TRUE);

    if (g_hRightTabs) {
        MoveWindow(g_hRightTabs, rightX, usableTop, rightW, contentH, TRUE);
        RECT tabsRc{};
        GetClientRect(g_hRightTabs, &tabsRc);
        const int tabsPadding = 4;
        const int tabsTop = 6;
        const int tabsGap = 0;
        const int tabsHeight = 40;
        int tabsClientW = std::max(0, static_cast<int>(tabsRc.right - tabsRc.left));
        int tabsInnerW = std::max(0, tabsClientW - tabsPadding * 2);
        int tabButtonW = std::max(0, (tabsInnerW - tabsGap) / 2);
        MoveWindow(g_hRightTabFileBtn, tabsPadding, tabsTop, tabButtonW, tabsHeight, TRUE);
        MoveWindow(g_hRightTabScheduleBtn, tabsPadding + tabButtonW + tabsGap, tabsTop,
            std::max(0, tabsInnerW - tabButtonW - tabsGap), tabsHeight, TRUE);

        RECT pageRect = GetRightTabPageRect(g_hRightTabs);
        int pageW = static_cast<int>(pageRect.right - pageRect.left);
        int pageH = static_cast<int>(pageRect.bottom - pageRect.top);
        if (pageW < 0) pageW = 0;
        if (pageH < 0) pageH = 0;
        const int pagePadding = 4;
        int contentW = std::max(0, pageW - pagePadding * 2);
        int availableSectionH = std::max(0, pageH - pagePadding * 2 - sectionGap);
        int firstSectionH = availableSectionH / 2;
        int secondSectionH = availableSectionH - firstSectionH;

        MoveWindow(g_hFileTabPage, pageRect.left, pageRect.top, pageW, pageH, TRUE);
        MoveWindow(g_hScheduleTabPage, pageRect.left, pageRect.top, pageW, pageH, TRUE);

        int firstTop = pagePadding;
        int secondTop = firstTop + firstSectionH + sectionGap;

        MoveWindow(g_hDetailLabel, pagePadding, firstTop, contentW, labelHeight, TRUE);
        MoveWindow(g_hDetailEdit, pagePadding, firstTop + labelHeight, contentW,
            std::max(0, firstSectionH - labelHeight), TRUE);

        MoveWindow(g_hContentLabel, pagePadding, secondTop, contentW, labelHeight, TRUE);
        MoveWindow(g_hContentEdit, pagePadding, secondTop + labelHeight, contentW,
            std::max(0, secondSectionH - labelHeight), TRUE);

        MoveWindow(g_hGanttLabel, pagePadding, firstTop, contentW, labelHeight, TRUE);
        MoveWindow(g_hGanttChart, pagePadding, firstTop + labelHeight, contentW,
            std::max(0, firstSectionH - labelHeight), TRUE);
        UpdateGanttScrollInfo();

        MoveWindow(g_hProcessLabel, pagePadding, secondTop, contentW, labelHeight, TRUE);
        MoveWindow(g_hProcessList, pagePadding, secondTop + labelHeight, contentW,
            std::max(0, secondSectionH - labelHeight), TRUE);
        ResizeProcessColumns();
        UpdateRightTabVisibility();
    }

    MoveWindow(g_hStatusText, margin, clientH - margin - statusHeight, clientW - margin * 2, statusHeight, TRUE);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        CreateChildControls(hwnd);
        RefreshDriveCombo();
        return 0;

    case WM_SIZE:
        LayoutControls(hwnd);
        return 0;

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);

        if (id == IDC_REFRESH_BTN && code == BN_CLICKED) {
            RefreshDriveCombo();
            return 0;
        }

        if (id == IDC_LOAD_BTN && code == BN_CLICKED) {
            LoadSelectedDrive();
            return 0;
        }

        if (id == IDC_RIGHT_TAB_FILE_BTN && code == BN_CLICKED) {
            if (g_activeRightTab != RIGHT_TAB_FILE_VIEW) {
                g_activeRightTab = RIGHT_TAB_FILE_VIEW;
                LayoutControls(hwnd);
            }
            return 0;
        }

        if (id == IDC_RIGHT_TAB_SCHEDULE_BTN && code == BN_CLICKED) {
            if (g_activeRightTab != RIGHT_TAB_SCHEDULE_VIEW) {
                g_activeRightTab = RIGHT_TAB_SCHEDULE_VIEW;
                LayoutControls(hwnd);
                RedrawProcessTableHeader();
            }
            return 0;
        }

        if (id == IDC_DRIVE_COMBO && code == CBN_SELCHANGE) {
            ResetLoadedData(true);
            std::string drive = GetSelectedDrive();
            if (!drive.empty()) {
                SetStatusText("Drive " + drive + " selected. Click Scan to begin.");
            }
            return 0;
        }

        if (id == IDC_FILE_LIST && (code == LBN_SELCHANGE || code == LBN_DBLCLK)) {
            int index = static_cast<int>(SendMessageA(g_hFileList, LB_GETCURSEL, 0, 0));
            ShowSelectedFileInfo(index);
            return 0;
        }
        return 0;
    }

    case WM_HSCROLL:
        if (reinterpret_cast<HWND>(lParam) == g_hGanttChart && HandleGanttHScroll(g_hGanttChart, wParam)) {
            return 0;
        }
        break;

    case WM_DRAWITEM: {
        const DRAWITEMSTRUCT* dis = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (dis && (dis->CtlID == IDC_RIGHT_TAB_FILE_BTN || dis->CtlID == IDC_RIGHT_TAB_SCHEDULE_BTN)) {
            DrawRightTabButton(dis);
            return TRUE;
        }
        break;
    }

    case WM_CTLCOLORSTATIC: {
        HBRUSH brush = ApplySharedControlColors(reinterpret_cast<HWND>(lParam), reinterpret_cast<HDC>(wParam));
        return reinterpret_cast<INT_PTR>(brush ? brush : g_hMainBrush);
    }

    case WM_CTLCOLOREDIT: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetBkColor(hdc, kColorCardBg);
        SetTextColor(hdc, kColorTextMain);
        return reinterpret_cast<INT_PTR>(g_hCardBrush);
    }

    case WM_CTLCOLORLISTBOX: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetBkColor(hdc, kColorCardBg);
        SetTextColor(hdc, kColorTextMain);
        return reinterpret_cast<INT_PTR>(g_hCardBrush);
    }

    case WM_DESTROY:
        DestroyGanttCanvasCache();
        if (g_hFontUI) DeleteObject(g_hFontUI);
        if (g_hFontLabel) DeleteObject(g_hFontLabel);
        if (g_hFontMono) DeleteObject(g_hFontMono);
        if (g_hMainBrush) DeleteObject(g_hMainBrush);
        if (g_hCardBrush) DeleteObject(g_hCardBrush);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static int RunGui(HINSTANCE hInstance, int nCmdShow, const std::string& initialDrive = "") {
    g_initialDrive = NormalizeDriveInput(initialDrive);
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES;
    InitCommonControlsEx(&icc);

    g_hMainBrush = CreateSolidBrush(kColorWindowBg);
    g_hCardBrush = CreateSolidBrush(kColorCardBg);

    WNDCLASSEXA ganttClass{};
    ganttClass.cbSize = sizeof(ganttClass);
    ganttClass.lpfnWndProc = GanttChartProc;
    ganttClass.hInstance = hInstance;
    ganttClass.lpszClassName = kGanttChartClass;
    ganttClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    ganttClass.hbrBackground = nullptr;
    ganttClass.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassExA(&ganttClass);

    WNDCLASSEXA actionButtonClass{};
    actionButtonClass.cbSize = sizeof(actionButtonClass);
    actionButtonClass.lpfnWndProc = ActionButtonProc;
    actionButtonClass.hInstance = hInstance;
    actionButtonClass.lpszClassName = kActionButtonClass;
    actionButtonClass.hCursor = LoadCursor(nullptr, IDC_HAND);
    actionButtonClass.hbrBackground = nullptr;
    actionButtonClass.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassExA(&actionButtonClass);

    WNDCLASSEXA pageClass{};
    pageClass.cbSize = sizeof(pageClass);
    pageClass.lpfnWndProc = RightTabPageProc;
    pageClass.hInstance = hInstance;
    pageClass.lpszClassName = kRightTabPageClass;
    pageClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    pageClass.hbrBackground = g_hMainBrush;
    pageClass.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassExA(&pageClass);

    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "Lab2Fat32GuiWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = g_hMainBrush;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);

    if (!RegisterClassExA(&wc)) {
        MessageBoxA(nullptr, "Could not register the main window class.", kAppTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    g_hMainWnd = CreateWindowExA(
        0,
        wc.lpszClassName,
        kAppTitle,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1360, 860,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!g_hMainWnd) {
        MessageBoxA(nullptr, "Could not create the application window.", kAppTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);

    MSG msg{};
    while (GetMessageA(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return static_cast<int>(msg.wParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow) {
    std::string initialDrive = lpCmdLine ? lpCmdLine : "";
    return RunGui(hInstance, nCmdShow, initialDrive);
}

int main(int argc, char* argv[]) {
    std::string initialDrive = (argc > 1) ? argv[1] : "";
    return RunGui(GetModuleHandleA(nullptr), SW_SHOWDEFAULT, initialDrive);
}
