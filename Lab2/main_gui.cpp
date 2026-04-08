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
#include <cctype>
#include <cstring>
#include "fat32/DiskReader.h"
#include "fat32/BootSector.h"
#include "fat32/FATTable.h"
#include "fat32/DirectoryEntry.h"

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

enum ControlId {
    IDC_DRIVE_COMBO = 1001,
    IDC_REFRESH_BTN,
    IDC_LOAD_BTN,
    IDC_BOOT_LIST,
    IDC_FILE_LIST,
    IDC_DETAIL_EDIT,
    IDC_CONTENT_EDIT,
    IDC_PROCESS_LIST,
    IDC_STATUS_TEXT,
    IDC_BOOT_LABEL,
    IDC_FILE_LABEL,
    IDC_DETAIL_LABEL,
    IDC_CONTENT_LABEL,
    IDC_PROCESS_LABEL
};

static HWND g_hMainWnd = nullptr;
static HWND g_hDriveCombo = nullptr;
static HWND g_hBootList = nullptr;
static HWND g_hFileList = nullptr;
static HWND g_hDetailEdit = nullptr;
static HWND g_hContentEdit = nullptr;
static HWND g_hProcessList = nullptr;
static HWND g_hStatusText = nullptr;
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

static const char* kAppTitle = "Lab 2 | FAT32 Schedule Explorer";
static const COLORREF kColorWindowBg = RGB(245, 247, 250);
static const COLORREF kColorCardBg = RGB(255, 255, 255);
static const COLORREF kColorTextMain = RGB(32, 32, 32);
static const COLORREF kColorTextMuted = RGB(96, 96, 96);

static const QueueInfo* FindQueueById(const std::vector<QueueInfo>& queues, const std::string& queueId) {
    for (const auto& q : queues) {
        if (q.id == queueId) return &q;
    }
    return nullptr;
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

static void ApplyFontsToChildren(HWND hwnd) {
    ApplyControlFont(g_hDriveCombo, g_hFontUI);
    ApplyControlFont(GetDlgItem(hwnd, IDC_REFRESH_BTN), g_hFontUI);
    ApplyControlFont(GetDlgItem(hwnd, IDC_LOAD_BTN), g_hFontUI);
    ApplyControlFont(g_hBootLabel, g_hFontLabel);
    ApplyControlFont(g_hFileLabel, g_hFontLabel);
    ApplyControlFont(g_hDetailLabel, g_hFontLabel);
    ApplyControlFont(g_hContentLabel, g_hFontLabel);
    ApplyControlFont(g_hProcessLabel, g_hFontLabel);
    ApplyControlFont(g_hBootList, g_hFontUI);
    ApplyControlFont(g_hFileList, g_hFontUI);
    ApplyControlFont(g_hDetailEdit, g_hFontMono);
    ApplyControlFont(g_hContentEdit, g_hFontMono);
    ApplyControlFont(g_hProcessList, g_hFontUI);
    ApplyControlFont(g_hStatusText, g_hFontUI);
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

    HWND hHeader = ListView_GetHeader(g_hProcessList);
    RECT rc{};
    if (hHeader) {
        GetClientRect(hHeader, &rc);
    } else {
        GetClientRect(g_hProcessList, &rc);
    }
    int totalWidth = rc.right - rc.left;
    if (totalWidth <= 0) return;

    const int columnCount = 6;
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

static void ClearFileViews() {
    SetWindowTextA(g_hDetailEdit, "");
    SetWindowTextA(g_hContentEdit, "");
    SendMessageA(g_hDetailEdit, EM_SETSEL, 0, 0);
    SendMessageA(g_hContentEdit, EM_SETSEL, 0, 0);
    ClearProcessList();
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

static void PopulateProcessList(const ParsedSchedule& parsed) {
    ClearProcessList();

    for (const auto& proc : parsed.processes) {
        const QueueInfo* queue = FindQueueById(parsed.queues, proc.queueId);
        std::string timeSlice = queue ? std::to_string(queue->timeSlice) : "N/A";
        std::string algorithm = queue ? queue->algorithm : "N/A";

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
    }
}

static void ShowSelectedFileInfo(int index) {
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
               << "Process count: " << parsed.processes.size() << "\r\n";
    } else {
        detail << "Schedule parse: " << parsed.error << "\r\n";
    }

    SetWindowTextA(g_hDetailEdit, detail.str().c_str());
    SendMessageA(g_hDetailEdit, EM_SETSEL, 0, 0);
    SendMessageA(g_hDetailEdit, EM_SCROLLCARET, 0, 0);

    if (parsed.ok) {
        PopulateProcessList(parsed);
        SetStatusText("Loaded the selected file content.");
    } else {
        ClearProcessList();
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
    if (g_reader.isOpen()) {
        g_reader.close();
    }
    ClearBootList();
    SendMessageA(g_hFileList, LB_RESETCONTENT, 0, 0);
    ClearFileViews();
    g_txtFiles.clear();
    g_hasLoadedDrive = false;
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

    CreateWindowExA(0, "BUTTON", "Refresh",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_REFRESH_BTN), GetModuleHandle(nullptr), nullptr);

    CreateWindowExA(0, "BUTTON", "Scan",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
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

    g_hDetailLabel = CreateWindowExA(0, "STATIC", "Selected File Details",
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_DETAIL_LABEL), GetModuleHandle(nullptr), nullptr);

    g_hDetailEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY,
        0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_DETAIL_EDIT), GetModuleHandle(nullptr), nullptr);

    g_hContentLabel = CreateWindowExA(0, "STATIC", "File Content",
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_CONTENT_LABEL), GetModuleHandle(nullptr), nullptr);

    g_hContentEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL |
        WS_VSCROLL | WS_HSCROLL | ES_READONLY,
        0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_CONTENT_EDIT), GetModuleHandle(nullptr), nullptr);

    g_hProcessLabel = CreateWindowExA(0, "STATIC", "Scheduling Table",
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_PROCESS_LABEL), GetModuleHandle(nullptr), nullptr);

    g_hProcessList = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
        0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_PROCESS_LIST), GetModuleHandle(nullptr), nullptr);

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
    ResizeProcessColumns();

    g_hFontUI = CreateAppFont(11, FW_NORMAL, "Segoe UI");
    g_hFontLabel = CreateAppFont(11, FW_SEMIBOLD, "Segoe UI");
    g_hFontMono = CreateAppFont(10, FW_NORMAL, "Consolas");
    ApplyFontsToChildren(hwnd);
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
    int detailAreaH = 140;
    int contentAreaH = 200;
    int processAreaH = contentH - detailAreaH - contentAreaH - sectionGap * 2;
    if (processAreaH < 160) processAreaH = 160;

    const int comboW = 160;
    const int topGap = 8;
    const int refreshW = 96;
    const int scanW = 84;

    MoveWindow(g_hDriveCombo, margin, margin, comboW, topHeight, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_REFRESH_BTN), margin + comboW + topGap, margin, refreshW, topHeight, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_LOAD_BTN), margin + comboW + topGap + refreshW + topGap, margin, scanW, topHeight, TRUE);

    MoveWindow(g_hBootLabel, margin, usableTop, leftW, labelHeight, TRUE);
    MoveWindow(g_hBootList, margin, usableTop + labelHeight, leftW, bootAreaH - labelHeight, TRUE);
    ResizeBootColumns();

    int fileY = usableTop + bootAreaH + gap;
    MoveWindow(g_hFileLabel, margin, fileY, leftW, labelHeight, TRUE);
    MoveWindow(g_hFileList, margin, fileY + labelHeight, leftW, fileAreaH - labelHeight, TRUE);

    int detailY = usableTop;
    MoveWindow(g_hDetailLabel, rightX, detailY, rightW, labelHeight, TRUE);
    MoveWindow(g_hDetailEdit, rightX, detailY + labelHeight, rightW, detailAreaH - labelHeight, TRUE);

    int contentY = detailY + detailAreaH + sectionGap;
    MoveWindow(g_hContentLabel, rightX, contentY, rightW, labelHeight, TRUE);
    MoveWindow(g_hContentEdit, rightX, contentY + labelHeight, rightW, contentAreaH - labelHeight, TRUE);

    int processY = contentY + contentAreaH + sectionGap;
    int remainingH = usableBottom - processY;
    if (remainingH > labelHeight + 40) {
        processAreaH = remainingH;
    }
    MoveWindow(g_hProcessLabel, rightX, processY, rightW, labelHeight, TRUE);
    MoveWindow(g_hProcessList, rightX, processY + labelHeight, rightW, processAreaH - labelHeight, TRUE);
    ResizeProcessColumns();

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

    case WM_CTLCOLORSTATIC: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        HWND hCtrl = reinterpret_cast<HWND>(lParam);
        if (hCtrl == g_hDetailEdit || hCtrl == g_hContentEdit) {
            SetBkColor(hdc, kColorCardBg);
            SetTextColor(hdc, kColorTextMain);
            return reinterpret_cast<INT_PTR>(g_hCardBrush);
        }
        SetBkMode(hdc, TRANSPARENT);
        if (hCtrl == g_hBootLabel || hCtrl == g_hFileLabel || hCtrl == g_hDetailLabel ||
            hCtrl == g_hContentLabel || hCtrl == g_hProcessLabel) {
            SetTextColor(hdc, kColorTextMain);
            return reinterpret_cast<INT_PTR>(g_hMainBrush);
        }
        if (hCtrl == g_hStatusText) {
            SetTextColor(hdc, kColorTextMuted);
            return reinterpret_cast<INT_PTR>(g_hMainBrush);
        }
        return reinterpret_cast<INT_PTR>(g_hMainBrush);
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
    icc.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icc);

    g_hMainBrush = CreateSolidBrush(kColorWindowBg);
    g_hCardBrush = CreateSolidBrush(kColorCardBg);

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
        WS_OVERLAPPEDWINDOW,
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
