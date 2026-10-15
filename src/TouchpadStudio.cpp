#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <dwmapi.h>
#include <commdlg.h>
#include <tlhelp32.h>
#include <sddl.h>
#include <shellapi.h>
#include <vector>
#include <array>
#include <deque>
#include <map>
#include <string>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <sstream>
#include "FiveFingerRecognizer.h"
#include "DriverGestureControls.h"

// Standalone x64 Win32 / Direct2D application. No PowerShell or new drivers.
// ETDApi 11.9.4.1 ABI inspected against this machine's signed vendor DLL.
// Contact records are 64 bytes: button bytes, padding, then five 12-byte
// entries (one active byte + padding, int32 X, int32 Y). AcquireFingerReport
// returns complete records since a driver ring-buffer index, not mouse input.
namespace app {
const wchar_t *KeyPath = L"Software\\Elantech\\SmartPad";
const wchar_t *ClassName = L"ElanTouchpadStudioWindow";
struct Value {
    DWORD type = 0;
    std::vector<BYTE> bytes;
    bool operator==(const Value &o) const {
        return type == o.type && bytes == o.bytes;
    }
};
using Snapshot = std::map<std::wstring, Value>;
struct Setting {
    const wchar_t *name;
    const wchar_t *title;
    const wchar_t *subtitle;
};
const Setting Settings[] = {
    {L"Tap_Two_Finger_Enable", L"Two-finger right-click", L"Tap with two fingers"},
    {L"Zoom_Enable", L"Pinch to zoom", L"Move two fingers together or apart"},
    {L"SC_Enable", L"Scrolling", L"Enable touchpad scrolling"},
    {L"SC_V_Enable", L"Vertical scrolling", L"Scroll up and down"},
    {L"SC_H_Enable", L"Horizontal scrolling", L"Scroll left and right"},
    {L"SC_Reverse_Enable", L"Reverse scroll direction", L"Reverse the direction of scrolling"}};
std::wstring SysDir() {
    wchar_t b[MAX_PATH];
    GetSystemDirectoryW(b, MAX_PATH);
    return b;
}
std::wstring AppDir() {
    wchar_t b[32768];
    GetModuleFileNameW(nullptr, b, 32768);
    std::wstring p = b;
    return p.substr(0, p.find_last_of(L"\\/"));
}
std::wstring Sid() {
    HANDLE h = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &h))
        throw std::runtime_error("Cannot read current user");
    DWORD n = 0;
    GetTokenInformation(h, TokenUser, nullptr, 0, &n);
    std::vector<BYTE> b(n);
    bool ok = GetTokenInformation(h, TokenUser, b.data(), n, &n);
    CloseHandle(h);
    LPWSTR s = nullptr;
    if (!ok || !ConvertSidToStringSidW(((TOKEN_USER *)b.data())->User.Sid, &s))
        throw std::runtime_error("Cannot identify current user");
    std::wstring r = s;
    LocalFree(s);
    return r;
}
std::wstring Widen(const char *s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s, -1, &w[0], n);
    if (!w.empty())
        w.pop_back();
    return w;
}
Value Dword(DWORD x) {
    Value v;
    v.type = REG_DWORD;
    v.bytes.resize(4);
    memcpy(v.bytes.data(), &x, 4);
    return v;
}
DWORD Number(const Value &v) {
    if (v.type != REG_DWORD || v.bytes.size() != 4)
        throw std::runtime_error("Unexpected registry value type");
    DWORD x;
    memcpy(&x, v.bytes.data(), 4);
    return x;
}

struct DriverGesture {
    std::wstring action, enable;
    const wchar_t *title;
};
std::array<DriverGesture, 5> DriverGestures(int fingers) {
    std::wstring count = fingers == 3 ? L"Three" : L"Four", tap = L"Tap_" + count + L"_Finger",
                 prefix = count + L"FingerMove";
    return {{{tap, tap + L"_Enable", L"Tap"},
             {prefix + L"Up_Func", prefix + L"Up_Enable", L"Swipe up"},
             {prefix + L"Down_Func", prefix + L"Down_Enable", L"Swipe down"},
             {prefix + L"Left_Func", prefix + L"Left_Enable", L"Swipe left"},
             {prefix + L"Right_Func", prefix + L"Right_Enable", L"Swipe right"}}};
}
struct DriverAction {
    DWORD value;
    const wchar_t *title;
};
// Every number below is defined in the tested driver's etd.inf [Strings].
const DriverAction DriverActions[] = {{0, L"Left click"},
                                      {1, L"Right click"},
                                      {2, L"Middle click"},
                                      {3, L"Browser back"},
                                      {4, L"Browser forward"},
                                      {7, L"No action"},
                                      {15, L"Start menu"},
                                      {19, L"Show desktop"},
                                      {22, L"Switch windows"},
                                      {23, L"Window switcher (legacy)"},
                                      {26, L"Search"},
                                      {27, L"Double click"},
                                      {2561, L"Search / Cortana (legacy)"},
                                      {2562, L"Notifications (legacy)"},
                                      {2563, L"Switch windows (Win10)"},
                                      {2564, L"Previous window"},
                                      {2565, L"Next window"},
                                      {2566, L"Window switch up"},
                                      {2567, L"Window switch down"},
                                      {2568, L"Release window switch"},
                                      {2569, L"Task view"},
                                      {2570, L"Task view left"},
                                      {2571, L"Task view right"},
                                      {2572, L"Release task view"},
                                      {2573, L"Exit task view"},
                                      {2574, L"Next desktop"},
                                      {2575, L"Previous desktop"}};
bool ActionField(const std::wstring &n) {
    for (int f : {3, 4})
        for (auto &g : DriverGestures(f))
            if (n == g.action)
                return true;
    return false;
}
bool Allowed(const std::wstring &n) {
    if (n == L"Tap_Two_Finger" || n == L"MultiFingerGesture_Enable" ||
        n == L"ThreeFingerMove_Enable" || n == L"FourFingerMove_Enable")
        return true;
    for (auto &v : Settings)
        if (n == v.name)
            return true;
    for (int f : {3, 4})
        for (auto &g : DriverGestures(f))
            if (n == g.action || n == g.enable)
                return true;
    return false;
}
bool ValidValue(const std::wstring &n, const Value &v) {
    if (v.type != REG_DWORD || v.bytes.size() != 4)
        return false;
    DWORD x = Number(v);
    if (n == L"Tap_Two_Finger")
        return x == 1 || x == 7;
    if (ActionField(n)) {
        for (auto &a : DriverActions)
            if (a.value == x)
                return true;
        return false;
    }
    return x <= 1;
}
FiveConfig appliedFive, pendingFive;
DriverValues appliedSuspended, pendingSuspended;
const std::wstring SuspendedPrefix = L"@Studio.Suspended.";
DriverValues DecodeSuspended(const Snapshot &s) {
    DriverValues result;
    for (const auto &kv : s) {
        if (kv.first.compare(0, SuspendedPrefix.size(), SuspendedPrefix) != 0)
            continue;
        auto name = kv.first.substr(SuspendedPrefix.size());
        if (!MultiFingerField(name) || !Allowed(name) || !ValidValue(name, kv.second))
            throw std::runtime_error("Invalid remembered multi-finger setting");
        result[name] = Number(kv.second);
    }
    return result;
}
const wchar_t *FiveTitles[] = {
    L"Off",  L"Show desktop", L"Task view",   L"Previous desktop", L"Next desktop", L"Play / pause",
    L"Mute", L"Volume up",    L"Volume down", L"Previous track",   L"Next track"};
const wchar_t *FiveNames[] = {L"@Studio.FiveTap", L"@Studio.FiveUp", L"@Studio.FiveDown",
                              L"@Studio.FiveLeft", L"@Studio.FiveRight"};
Snapshot WithAppSettings(const Snapshot &in) {
    auto out = in;
    out[L"@Studio.FiveEnabled"] = Dword(appliedFive.enabled);
    for (int i = 0; i < 5; ++i)
        out[FiveNames[i]] = Dword(appliedFive.actions[i]);
    for (const auto &kv : appliedSuspended)
        out[SuspendedPrefix + kv.first] = Dword(kv.second);
    return out;
}
FiveConfig DecodeFive(const Snapshot &s, const FiveConfig &fallback) {
    FiveConfig c = fallback;
    auto e = s.find(L"@Studio.FiveEnabled");
    if (e != s.end()) {
        if (Number(e->second) > 1)
            throw std::runtime_error("Invalid five-finger enable setting");
        c.enabled = Number(e->second) == 1;
    }
    for (int i = 0; i < 5; ++i) {
        auto it = s.find(FiveNames[i]);
        if (it != s.end()) {
            DWORD x = Number(it->second);
            if (x >= FiveActionCount)
                throw std::runtime_error("Unknown five-finger action");
            c.actions[i] = int(x);
        }
    }
    return c;
}

Snapshot ReadSettings() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, KeyPath, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        throw std::runtime_error("No ELAN SmartPad settings were found for this Windows user.");
    Snapshot r;
    DWORD maxName = 0, maxData = 0;
    LONG rc = RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                               &maxName, &maxData, nullptr, nullptr);
    if (rc != ERROR_SUCCESS) {
        RegCloseKey(key);
        throw std::runtime_error("Cannot enumerate ELAN settings");
    }
    std::vector<wchar_t> name(maxName + 2);
    std::vector<BYTE> data(maxData + 8);
    for (DWORD i = 0;; ++i) {
        DWORD n = (DWORD)name.size(), z = (DWORD)data.size(), type = 0;
        rc = RegEnumValueW(key, i, name.data(), &n, nullptr, &type, data.data(), &z);
        if (rc == ERROR_NO_MORE_ITEMS)
            break;
        if (rc != ERROR_SUCCESS) {
            RegCloseKey(key);
            throw std::runtime_error("ELAN settings changed while reading. Try Reset.");
        }
        Value v;
        v.type = type;
        v.bytes.assign(data.begin(), data.begin() + z);
        r[std::wstring(name.data(), n)] = v;
    }
    RegCloseKey(key);
    return r;
}
void Put32(std::vector<BYTE> &b, DWORD x) {
    BYTE *p = (BYTE *)&x;
    b.insert(b.end(), p, p + 4);
}
void PutString(std::vector<BYTE> &b, const std::wstring &s) {
    Put32(b, (DWORD)s.size());
    const BYTE *p = (const BYTE *)s.data();
    b.insert(b.end(), p, p + s.size() * 2);
}
struct Reader {
    const std::vector<BYTE> &b;
    size_t pos = 0;
    DWORD u32() {
        if (pos + 4 > b.size())
            throw std::runtime_error("Truncated backup");
        DWORD x;
        memcpy(&x, b.data() + pos, 4);
        pos += 4;
        return x;
    }
    std::wstring str() {
        DWORD n = u32();
        if (n > 32768 || pos + size_t(n) * 2 > b.size())
            throw std::runtime_error("Invalid backup text");
        std::wstring s(n, L'\0');
        if (n)
            memcpy(s.data(), b.data() + pos, n * 2);
        pos += n * 2;
        return s;
    }
};
std::vector<BYTE> Encode(const Snapshot &s) {
    std::vector<BYTE> b = {'E', 'L', 'A', 'N', 'B', 'K', 'P', '1'};
    PutString(b, Sid());
    Put32(b, (DWORD)s.size());
    for (auto &kv : s) {
        PutString(b, kv.first);
        Put32(b, kv.second.type);
        Put32(b, (DWORD)kv.second.bytes.size());
        b.insert(b.end(), kv.second.bytes.begin(), kv.second.bytes.end());
    }
    return b;
}
Snapshot Decode(const std::vector<BYTE> &b) {
    if (b.size() < 8 || memcmp(b.data(), "ELANBKP1", 8))
        throw std::runtime_error("This is not a Touchpad Studio backup");
    Reader r{b, 8};
    if (r.str() != Sid())
        throw std::runtime_error("This backup belongs to a different Windows user");
    DWORD count = r.u32();
    if (count > 10000)
        throw std::runtime_error("Invalid backup entry count");
    Snapshot s;
    for (DWORD i = 0; i < count; ++i) {
        std::wstring name = r.str();
        Value v;
        v.type = r.u32();
        DWORD n = r.u32();
        if (n > 1024 * 1024 || r.pos + n > b.size())
            throw std::runtime_error("Invalid backup value");
        v.bytes.assign(b.begin() + r.pos, b.begin() + r.pos + n);
        r.pos += n;
        if (!s.emplace(name, v).second)
            throw std::runtime_error("Duplicate backup value");
    }
    if (r.pos != b.size())
        throw std::runtime_error("Unexpected backup data");
    return s;
}
std::wstring SaveBackup(const Snapshot &s) {
    std::wstring dir = AppDir() + L"\\ELAN-backups";
    if (!CreateDirectoryW(dir.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        throw std::runtime_error("Cannot create the backup folder. No settings changed.");
    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t name[160];
    swprintf(name, 160, L"\\SmartPad-%04u%02u%02u-%02u%02u%02u-%03u-%lu.elanbackup", t.wYear,
             t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds,
             GetCurrentProcessId());
    std::wstring path = dir + name;
    auto b = Encode(WithAppSettings(s));
    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Cannot save a backup. No settings changed.");
    DWORD n = 0;
    bool ok = WriteFile(f, b.data(), (DWORD)b.size(), &n, nullptr) && n == b.size() &&
              FlushFileBuffers(f);
    CloseHandle(f);
    if (!ok)
        throw std::runtime_error("Backup write failed. No settings changed.");
    HANDLE check = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                               0, nullptr);
    std::vector<BYTE> verify(b.size());
    n = 0;
    ok = check != INVALID_HANDLE_VALUE &&
         ReadFile(check, verify.data(), (DWORD)verify.size(), &n, nullptr) && n == verify.size() &&
         verify == b;
    if (check != INVALID_HANDLE_VALUE)
        CloseHandle(check);
    if (!ok)
        throw std::runtime_error("Backup verification failed. No settings changed.");
    return path;
}
std::wstring WriteChanges(const Snapshot &changes, const Snapshot &expected) {
    auto current = ReadSettings();
    for (auto &kv : changes) {
        auto it = current.find(kv.first);
        auto old = expected.find(kv.first);
        if (!Allowed(kv.first) || !ValidValue(kv.first, kv.second) || it == current.end() ||
            !ValidValue(kv.first, it->second) || old == expected.end() ||
            !(it->second == old->second))
            throw std::runtime_error("Settings changed elsewhere or have an unsupported type. "
                                     "Click Reset and try again.");
    }
    std::wstring backup = SaveBackup(current);
    if (changes.empty())
        return backup;
    HKEY k = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, KeyPath, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &k) !=
        ERROR_SUCCESS)
        throw std::runtime_error("Cannot open ELAN settings for writing");
    std::vector<std::wstring> written;
    try {
        for (auto &kv : changes) {
            if (RegSetValueExW(k, kv.first.c_str(), 0, kv.second.type, kv.second.bytes.data(),
                               (DWORD)kv.second.bytes.size()) != ERROR_SUCCESS)
                throw std::runtime_error("Could not save an ELAN setting");
            written.push_back(kv.first);
        }
        auto actual = ReadSettings();
        for (auto &kv : changes)
            if (!(actual.at(kv.first) == kv.second))
                throw std::runtime_error("Saved setting did not verify");
    } catch (...) {
        bool restored = true;
        for (auto &n : written) {
            auto &v = current.at(n);
            if (RegSetValueExW(k, n.c_str(), 0, v.type, v.bytes.data(), (DWORD)v.bytes.size()) !=
                ERROR_SUCCESS)
                restored = false;
        }
        RegCloseKey(k);
        if (!restored)
            throw std::runtime_error("A save failed and rollback was incomplete. Use the automatic "
                                     "backup in ELAN-backups.");
        throw;
    }
    RegCloseKey(k);
    return backup;
}

std::vector<BYTE> ReadBytes(const std::wstring &path) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0,
                           nullptr);
    if (f == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Cannot read settings file");
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(f, &size) || size.QuadPart < 1 || size.QuadPart > 16 * 1024 * 1024) {
        CloseHandle(f);
        throw std::runtime_error("Invalid settings file size");
    }
    std::vector<BYTE> b(size_t(size.QuadPart));
    DWORD n = 0;
    bool ok = ReadFile(f, b.data(), DWORD(b.size()), &n, nullptr) && n == b.size();
    CloseHandle(f);
    if (!ok)
        throw std::runtime_error("Cannot read complete settings file");
    return b;
}
void ReadLocal() {
    std::wstring file = AppDir() + L"\\TouchpadStudio.settings";
    if (GetFileAttributesW(file.c_str()) == INVALID_FILE_ATTRIBUTES) {
        appliedFive = {};
        appliedSuspended.clear();
        return;
    }
    auto data = Decode(ReadBytes(file));
    appliedFive = DecodeFive(data, {});
    appliedSuspended = DecodeSuspended(data);
}
void WriteLocal(const FiveConfig &cfg, const DriverValues &suspended) {
    Snapshot data;
    data[L"@Studio.FiveEnabled"] = Dword(cfg.enabled);
    for (int i = 0; i < 5; ++i)
        data[FiveNames[i]] = Dword(cfg.actions[i]);
    for (const auto &kv : suspended)
        data[SuspendedPrefix + kv.first] = Dword(kv.second);
    auto b = Encode(data);
    std::wstring file = AppDir() + L"\\TouchpadStudio.settings",
                 tmp = file + L".tmp-" + std::to_wstring(GetCurrentProcessId());
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Cannot save the app's local settings");
    DWORD n = 0;
    bool ok = WriteFile(h, b.data(), DWORD(b.size()), &n, nullptr) && n == b.size() &&
              FlushFileBuffers(h);
    CloseHandle(h);
    if (!ok) {
        DeleteFileW(tmp.c_str());
        throw std::runtime_error("Local settings could not be saved");
    }
    if (!MoveFileExW(tmp.c_str(), file.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmp.c_str());
        throw std::runtime_error("Local settings file could not be replaced");
    }
}
void ReloadElan() {
    DWORD session = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &session);
    DWORD chosen = 0, count = 0;
    HANDLE list = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (list == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Cannot inspect the ELAN process");
    PROCESSENTRY32W p{};
    p.dwSize = sizeof(p);
    for (BOOL ok = Process32FirstW(list, &p); ok; ok = Process32NextW(list, &p)) {
        DWORD s = 0;
        if (_wcsicmp(p.szExeFile, L"ETDCtrl.exe") == 0 &&
            ProcessIdToSessionId(p.th32ProcessID, &s) && s == session) {
            chosen = p.th32ProcessID;
            ++count;
        }
    }
    CloseHandle(list);
    if (count != 1)
        throw std::runtime_error("Expected one ELAN Control process in this session. Restart "
                                 "Windows to reload settings.");
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE | SYNCHRONIZE,
                           FALSE, chosen);
    if (!h)
        throw std::runtime_error("Cannot access ELAN Control");
    wchar_t buf[32768];
    DWORD n = 32768;
    std::wstring path = SysDir() + L"\\ETDCtrl.exe";
    if (!QueryFullProcessImageNameW(h, 0, buf, &n) || _wcsicmp(buf, path.c_str())) {
        CloseHandle(h);
        throw std::runtime_error("ELAN executable path could not be verified. No process stopped.");
    }
    if (!TerminateProcess(h, 0)) {
        CloseHandle(h);
        throw std::runtime_error("Cannot restart ELAN Control");
    }
    WaitForSingleObject(h, 3000);
    CloseHandle(h);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    std::wstring cmd = L"\"" + path + L"\"";
    if (!CreateProcessW(path.c_str(), &cmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                        nullptr, &si, &pi))
        throw std::runtime_error("ELAN Control did not start. Restart Windows to recover it.");
    CloseHandle(pi.hThread);
    Sleep(300);
    DWORD exit = 0;
    bool ok = GetExitCodeProcess(pi.hProcess, &exit) && exit == STILL_ACTIVE;
    CloseHandle(pi.hProcess);
    if (!ok)
        throw std::runtime_error(
            "ELAN Control exited unexpectedly. Restart Windows to recover it.");
}

struct Telemetry {
    HMODULE module = nullptr;
    bool connected = false;
    int width = 0, height = 0, index = 0;
    uint64_t records = 0;
    std::array<Contact, 5> contacts{};
    std::vector<BYTE> buffer = std::vector<BYTE>(1024 * 1024);
    using Connect = bool(__cdecl *)(unsigned);
    using Size = bool(__cdecl *)(int *, int *);
    using Index = int(__cdecl *)();
    using Acquire = int(__cdecl *)(int, void *, int *, int *);
    using End = bool(__cdecl *)();
    Connect connect = nullptr;
    Size size = nullptr;
    Index getIndex = nullptr;
    Acquire acquire = nullptr;
    End end = nullptr;
    std::vector<std::array<Contact, 5>> frames;
    std::wstring reason = L"Connecting to ELAN...";
    bool Open() {
        Close();
        std::wstring path = SysDir() + L"\\ETDApi.dll";
        DWORD dummy = 0, n = GetFileVersionInfoSizeW(path.c_str(), &dummy);
        std::vector<BYTE> info(n);
        VS_FIXEDFILEINFO *f = nullptr;
        UINT z = 0;
        if (!n || !GetFileVersionInfoW(path.c_str(), 0, n, info.data()) ||
            !VerQueryValueW(info.data(), L"\\", (void **)&f, &z) ||
            f->dwFileVersionMS != MAKELONG(9, 11) || f->dwFileVersionLS != MAKELONG(1, 4)) {
            reason = L"Live view unavailable for this ELAN API version";
            return false;
        }
        module = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!module) {
            reason = L"ELAN contact API could not load";
            return false;
        }
        connect = (Connect)GetProcAddress(module, "EstablishConnection");
        size = (Size)GetProcAddress(module, "GetTouchPadSize");
        getIndex = (Index)GetProcAddress(module, "GetCurrentReportIndex");
        acquire = (Acquire)GetProcAddress(module, "AcquireFingerReport");
        end = (End)GetProcAddress(module, "TerminateConnection");
        if (!connect || !size || !getIndex || !acquire || !end) {
            reason = L"ELAN contact API is incomplete";
            return false;
        }
        connected = connect(0x4e73be0);
        if (!connected) {
            reason = L"Live view unavailable; ELAN device may be busy";
            return false;
        }
        if (!size(&width, &height) || width <= 0 || height <= 0) {
            Close();
            reason = L"ELAN did not report valid touchpad dimensions";
            return false;
        }
        index = getIndex();
        if (index < 0) {
            Close();
            reason = L"ELAN report stream is unavailable";
            return false;
        }
        reason = L"Touch the trackpad to see your fingers";
        return true;
    }

    bool Poll() {
        frames.clear();
        if (!connected)
            return false;
        int count = 0, next = index;
        int result = acquire(index, buffer.data(), &count, &next);
        if (result != 1) {
            Close();
            reason = L"Contact stream interrupted. Use Reconnect in the menu.";
            return false;
        }
        if (count < 0 || count > 1024) {
            Close();
            reason = L"Unsupported ELAN report size";
            return false;
        }
        index = next;
        if (!count)
            return false;
        records += count;
        for (int r = 0; r < count; ++r) {
            const BYTE *p = buffer.data() + r * 64;
            for (int i = 0; i < 5; ++i) {
                BYTE flag = p[4 + 12 * i];
                int32_t x = 0, y = 0;
                memcpy(&x, p + 8 + 12 * i, 4);
                memcpy(&y, p + 12 + 12 * i, 4);
                contacts[i].down = flag == 1 && x >= 0 && y >= 0 && x <= width && y <= height;
                contacts[i].x = float(x) / width;
                contacts[i].y = 1.f - float(y) / height;
            }
            frames.push_back(contacts);
        }
        return true;
    }

    void Close() {
        if (connected && end)
            end();
        connected = false;
        for (auto &c : contacts)
            c.down = false;
        if (module)
            FreeLibrary(module);
        module = nullptr;
    }
    ~Telemetry() {
        Close();
    }
};

struct TrailPoint {
    float x, y;
    ULONGLONG time;
};
HWND window = nullptr;
ID2D1Factory *factory = nullptr;
IDWriteFactory *writes = nullptr;
ID2D1HwndRenderTarget *target = nullptr;
std::map<std::pair<int, int>, IDWriteTextFormat *> fonts;
Snapshot saved, extra;
FiveFingerRecognizer fiveEngine;
int page = 0;
std::wstring lastGesture = L"";
ULONGLONG lastGestureTime = 0;
std::array<bool, 6> toggles{}, available{};
int loadedAction = -1;
int hover = -1, focus = -1;
bool busy = false, preview = false;
std::wstring status = L"Up to date", lastBackup;
Telemetry telemetry;
std::array<std::deque<TrailPoint>, 5> trails;
float W = 720, H = 470, scale = 1;
int contacts = 0;
DWORD fpsReports = 0;
ULONGLONG rateTime = 0;
uint64_t previousRecords = 0;
D2D1_COLOR_F C(UINT32 rgb, float a = 1) {
    return D2D1::ColorF(rgb, a);
}
D2D1_RECT_F R(float x, float y, float w, float h) {
    return D2D1::RectF(x, y, x + w, y + h);
}
template <class T> void Release(T *&p) {
    if (p) {
        p->Release();
        p = nullptr;
    }
}
void Fill(ID2D1RenderTarget *t, D2D1_RECT_F r, UINT32 color, float radius = 0, float alpha = 1) {
    ID2D1SolidColorBrush *b = nullptr;
    t->CreateSolidColorBrush(C(color, alpha), &b);
    if (!b)
        return;
    if (radius)
        t->FillRoundedRectangle(D2D1::RoundedRect(r, radius, radius), b);
    else
        t->FillRectangle(r, b);
    b->Release();
}
void Outline(ID2D1RenderTarget *t, D2D1_RECT_F r, UINT32 color, float radius = 0, float alpha = 1,
             float stroke = 1) {
    ID2D1SolidColorBrush *b = nullptr;
    t->CreateSolidColorBrush(C(color, alpha), &b);
    if (!b)
        return;
    if (radius)
        t->DrawRoundedRectangle(D2D1::RoundedRect(r, radius, radius), b, stroke);
    else
        t->DrawRectangle(r, b, stroke);
    b->Release();
}
void Line(ID2D1RenderTarget *t, float x, float y, float x2, float y2, UINT32 color, float alpha = 1,
          float stroke = 1) {
    ID2D1SolidColorBrush *b = nullptr;
    t->CreateSolidColorBrush(C(color, alpha), &b);
    if (!b)
        return;
    t->DrawLine(D2D1::Point2F(x, y), D2D1::Point2F(x2, y2), b, stroke);
    b->Release();
}
void Circle(ID2D1RenderTarget *t, float x, float y, float radius, UINT32 color, float alpha = 1) {
    ID2D1SolidColorBrush *b = nullptr;
    t->CreateSolidColorBrush(C(color, alpha), &b);
    if (!b)
        return;
    t->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), radius, radius), b);
    b->Release();
}
void Text(ID2D1RenderTarget *t, const std::wstring &s, float x, float y, float w, float h,
          float size, UINT32 color, bool bold = false,
          DWRITE_TEXT_ALIGNMENT align = DWRITE_TEXT_ALIGNMENT_LEADING) {
    auto fontKey = std::make_pair(int(size * 10), (bold ? 1 : 0) + 2 * int(align));
    IDWriteTextFormat *f = fonts[fontKey];
    ID2D1SolidColorBrush *b = nullptr;
    if (!f)
        writes->CreateTextFormat(
            L"Segoe UI", nullptr, bold ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"en-GB", &f);
    fonts[fontKey] = f;
    t->CreateSolidColorBrush(C(color), &b);
    if (f && b) {
        f->SetTextAlignment(align);
        f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        t->DrawText(s.c_str(), (UINT32)s.size(), f, R(x, y, w, h), b, D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
    Release(b);
}
float Split() {
    return W * .46f;
}
float RowY(int i) {
    return 126.f + i * 37.f;
}
bool Hit(float x, float y, D2D1_RECT_F r) {
    return x >= r.left && x <= r.right && y >= r.top && y <= r.bottom;
}
D2D1_RECT_F ResetRect() {
    return R(W - 182, H - 57, 70, 32);
}
D2D1_RECT_F ApplyRect() {
    return R(W - 100, H - 57, 78, 32);
}
Snapshot Changes() {
    Snapshot c;
    for (int i = 0; i < 6; ++i) {
        if (!available[i])
            continue;
        const auto &name = Settings[i].name;
        bool old = Number(saved.at(name)) == 1;
        if (i == 0) {
            old = old && Number(saved.at(L"Tap_Two_Finger")) == 1;
            if (old == toggles[i])
                continue;
            if (toggles[i] && Number(saved.at(L"Tap_Two_Finger")) != 1)
                c[L"Tap_Two_Finger"] = Dword(1);
        }
        if (Number(saved.at(name)) != DWORD(toggles[i]))
            c[name] = Dword(toggles[i]);
    }
    if (loadedAction >= 0 && available[0]) {
        DWORD action = toggles[0] ? 1 : loadedAction;
        if (Number(saved.at(L"Tap_Two_Finger")) != action)
            c[L"Tap_Two_Finger"] = Dword(action);
    }
    for (auto &kv : extra) {
        if (Allowed(kv.first) && saved.count(kv.first) && !(kv.second == saved.at(kv.first)))
            c[kv.first] = kv.second;
    }
    return c;
}
void Reset() {
    saved = ReadSettings();
    extra.clear();
    pendingFive = appliedFive;
    pendingSuspended = appliedSuspended;
    loadedAction = -1;
    for (int i = 0; i < 6; ++i) {
        auto it = saved.find(Settings[i].name);
        available[i] = it != saved.end() && ValidValue(it->first, it->second);
        if (i == 0) {
            auto a = saved.find(L"Tap_Two_Finger");
            available[i] = available[i] && a != saved.end() && ValidValue(a->first, a->second);
        }
        toggles[i] = available[i] && Number(it->second) == 1;
        if (i == 0 && available[i])
            toggles[i] = toggles[i] && Number(saved.at(L"Tap_Two_Finger")) == 1;
    }
    status = L"Up to date";
}

bool Dirty() {
    return !Changes().empty() || pendingFive != appliedFive || pendingSuspended != appliedSuspended;
}
DWORD DriverNumber(const std::wstring &n) {
    auto x = extra.find(n);
    return Number(x == extra.end() ? saved.at(n) : x->second);
}
bool DriverAvailable(const std::wstring &n) {
    auto it = saved.find(n);
    return it != saved.end() && Allowed(n) && ValidValue(n, it->second);
}
std::wstring ActionTitle(DWORD v) {
    for (auto &a : DriverActions)
        if (a.value == v)
            return a.title;
    return L"Unsupported action " + std::to_wstring(v);
}
std::wstring MasterName() {
    return page == 1 ? L"ThreeFingerMove_Enable" : L"FourFingerMove_Enable";
}
int PageRows() {
    return page == 0 ? 6 : page == 3 ? 6 : 7;
}
void SetDriver(const std::wstring &n, DWORD x) {
    if (!DriverAvailable(n) || !ValidValue(n, Dword(x)))
        throw std::runtime_error("This driver setting is unavailable");
    extra[n] = Dword(x);
}
bool MultiFingerEnabled() {
    return DriverAvailable(L"MultiFingerGesture_Enable") &&
           DriverNumber(L"MultiFingerGesture_Enable") != 0;
}
void SetMultiFinger(bool enabled) {
    DriverValues values;
    for (const auto &kv : saved)
        if (MultiFingerField(kv.first) && DriverAvailable(kv.first))
            values[kv.first] = DriverNumber(kv.first);
    SetMultiFingerEnabled(values, pendingSuspended, enabled);
    for (const auto &kv : values)
        SetDriver(kv.first, kv.second);
}
void SetGestureAction(int slot, int value) {
    if (!MultiFingerEnabled() || slot < 0 || slot >= 5)
        return;
    auto g = DriverGestures(page == 1 ? 3 : 4)[slot];
    SetDriver(g.enable, value < 0 ? 0 : 1);
    // Some ELAN builds execute tap actions even with the tap enable bit
    // cleared. Explicitly clear the action as well when choosing Off.
    if (slot == 0 && value < 0)
        SetDriver(g.action, DriverNoAction);
    if (value >= 0) {
        SetDriver(g.action, DWORD(value));
        if (slot > 0 && DriverAvailable(MasterName()))
            SetDriver(MasterName(), 1);
    }
}
void PendingStatus() {
    status = Dirty() ? L"Unsaved" : L"Up to date";
}
void DispatchFive(int action) {
    if (action <= FiveOff || action >= FiveActionCount)
        return;
    // Never borrow or release a modifier key that the user is already holding.
    for (int key : {VK_SHIFT, VK_CONTROL, VK_MENU, VK_LWIN, VK_RWIN})
        if (GetAsyncKeyState(key) & 0x8000) {
            lastGesture = L"Skipped: keyboard modifier held";
            lastGestureTime = GetTickCount64();
            return;
        }
    std::vector<WORD> keys;
    switch (action) {
    case FiveDesktop:
        keys = {VK_LWIN, 'D'};
        break;
    case FiveTaskView:
        keys = {VK_LWIN, VK_TAB};
        break;
    case FivePreviousDesktop:
        keys = {VK_LWIN, VK_CONTROL, VK_LEFT};
        break;
    case FiveNextDesktop:
        keys = {VK_LWIN, VK_CONTROL, VK_RIGHT};
        break;
    case FivePlayPause:
        keys = {VK_MEDIA_PLAY_PAUSE};
        break;
    case FiveMute:
        keys = {VK_VOLUME_MUTE};
        break;
    case FiveVolumeUp:
        keys = {VK_VOLUME_UP};
        break;
    case FiveVolumeDown:
        keys = {VK_VOLUME_DOWN};
        break;
    case FivePreviousTrack:
        keys = {VK_MEDIA_PREV_TRACK};
        break;
    case FiveNextTrack:
        keys = {VK_MEDIA_NEXT_TRACK};
        break;
    default:
        return;
    }
    std::vector<INPUT> input;
    for (WORD key : keys) {
        INPUT i{};
        i.type = INPUT_KEYBOARD;
        i.ki.wVk = key;
        input.push_back(i);
    }
    for (auto it = keys.rbegin(); it != keys.rend(); ++it) {
        INPUT i{};
        i.type = INPUT_KEYBOARD;
        i.ki.wVk = *it;
        i.ki.dwFlags = KEYEVENTF_KEYUP;
        input.push_back(i);
    }
    UINT sent = SendInput(UINT(input.size()), input.data(), sizeof(INPUT));
    if (sent && sent < input.size()) {
        std::vector<INPUT> release;
        for (size_t i = std::min<size_t>(sent, keys.size()); i > 0; --i) {
            INPUT k{};
            k.type = INPUT_KEYBOARD;
            k.ki.wVk = keys[i - 1];
            k.ki.dwFlags = KEYEVENTF_KEYUP;
            release.push_back(k);
        }
        if (!release.empty())
            SendInput(UINT(release.size()), release.data(), sizeof(INPUT));
    }
    lastGesture = sent == input.size() ? std::wstring(L"5 fingers: ") + FiveTitles[action]
                                       : L"Shortcut blocked by Windows";
    lastGestureTime = GetTickCount64();
}
void ChooseAction(int row) {
    bool local = page == 3;
    int slot = local ? row - 1 : row - 2;
    if (slot < 0 || slot >= 5)
        return;
    if (!local && !MultiFingerEnabled())
        return;
    HMENU m = CreatePopupMenu();
    std::vector<int> codes;
    int selected = 0;
    if (local) {
        selected = pendingFive.actions[slot];
        for (int i = 0; i < FiveActionCount; ++i) {
            codes.push_back(i);
            AppendMenuW(m, MF_STRING | (i == selected ? MF_CHECKED : 0), 100 + i, FiveTitles[i]);
        }
    } else {
        auto g = DriverGestures(page == 1 ? 3 : 4)[slot];
        if (!DriverAvailable(g.action) || !DriverAvailable(g.enable)) {
            DestroyMenu(m);
            return;
        }
        int value = DriverNumber(g.enable) ? int(DriverNumber(g.action)) : -1;
        codes.push_back(-1);
        AppendMenuW(m, MF_STRING | (value == -1 ? MF_CHECKED : 0), 100, L"Off");
        for (auto &a : DriverActions) {
            codes.push_back(int(a.value));
            AppendMenuW(m, MF_STRING | (value == int(a.value) ? MF_CHECKED : 0),
                        100 + codes.size() - 1, a.title);
        }
    }
    POINT p{LONG((W - 30) * scale), LONG((RowY(row) + 32) * scale)};
    ClientToScreen(window, &p);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTALIGN, p.x, p.y, 0, window, nullptr);
    DestroyMenu(m);
    if (cmd < 100 || size_t(cmd - 100) >= codes.size())
        return;
    int value = codes[cmd - 100];
    if (local) {
        pendingFive.actions[slot] = value;
        if (value != FiveOff)
            pendingFive.enabled = true;
    } else
        SetGestureAction(slot, value);
    PendingStatus();
    InvalidateRect(window, nullptr, FALSE);
}

void Draw(ID2D1RenderTarget *t) {
    t->BeginDraw();
    t->Clear(C(0x10141C));
    float split = Split();
    Text(t, L"Touchpad Studio", 20, 14, 400, 32, 21, 0xF4F6FA, true);
    Text(t, L"ELAN / LENOVO V155", 21, 45, 300, 17, 9, 0x778396, true);
    Fill(t, R(W - 53, 19, 30, 28), hover == 12 ? 0x28303D : 0x1B222E, 6);
    Text(t, L"\x2026", W - 53, 12, 30, 31, 23, 0xBBC5D5, false, DWRITE_TEXT_ALIGNMENT_CENTER);
    Line(t, 20, 72, W - 20, 72, 0x252D39);
    float left = 20, right = split - 18, top = 115, pw = right - left, ph = pw * .654f,
          bottom = top + ph;
    Text(t, L"LIVE TOUCH VIEW", left, 88, pw, 18, 9, 0x8492A7, true);
    Circle(t, right - 52, 94, 2.5, telemetry.connected ? 0x54DBB0 : 0x738096);
    Text(t,
         preview               ? L"PREVIEW"
         : telemetry.connected ? L"LIVE"
                               : L"OFFLINE",
         right - 46, 87, 46, 18, 9, telemetry.connected ? 0x54DBB0 : 0x8492A7, true,
         DWRITE_TEXT_ALIGNMENT_TRAILING);
    Fill(t, R(left, top, pw, ph), 0x181F2B, 13);
    Outline(t, R(left, top, pw, ph), 0x364153, 13);
    for (int i = 1; i < 8; ++i)
        Line(t, left + pw * i / 8, top + 10, left + pw * i / 8, bottom - 10, 0x7A8AA2, .07f);
    for (int i = 1; i < 6; ++i)
        Line(t, left + 10, top + ph * i / 6, right - 10, top + ph * i / 6, 0x7A8AA2, .07f);
    Line(t, left + 10, bottom - ph * .14f, right - 10, bottom - ph * .14f, 0x536078, .24f);
    Line(t, left + pw / 2, bottom - ph * .14f, left + pw / 2, bottom - 8, 0x536078, .24f);
    const UINT32 colors[] = {0x61C9FF, 0xCB9FFF, 0xFFB86C, 0x74E7B1, 0xFF86AF};
    ULONGLONG now = GetTickCount64();
    contacts = 0;
    for (int i = 0; i < 5; ++i) {
        auto &tr = trails[i];
        for (size_t j = 1; j < tr.size(); ++j) {
            float age = float(now - tr[j].time) / 700.f;
            if (age < 1)
                Line(t, left + tr[j - 1].x * pw, top + tr[j - 1].y * ph, left + tr[j].x * pw,
                     top + tr[j].y * ph, colors[i], (1 - age) * .42f, 1.7f);
        }
        auto &c = telemetry.contacts[i];
        if (c.down) {
            ++contacts;
            float x = left + pw * c.x, y = top + ph * c.y;
            Circle(t, x, y, 20, colors[i], .07f);
            Circle(t, x, y, 14, colors[i], .15f);
            Circle(t, x, y, 7, colors[i], .95f);
            Circle(t, x, y, 2.3f, 0xFFFFFF, .95f);
            Text(t, std::to_wstring(i + 1), x + 13, y - 20, 22, 18, 10, colors[i], true);
        }
    }
    if (!contacts) {
        Text(t, telemetry.connected ? L"Touch the trackpad" : L"Live view unavailable", left,
             top + ph * .42f, pw, 25, 14, 0x69788F, false, DWRITE_TEXT_ALIGNMENT_CENTER);
        Text(t, telemetry.connected ? L"Up to five fingers" : L"Gesture settings still work", left,
             top + ph * .42f + 24, pw, 21, 10, 0x536179, false, DWRITE_TEXT_ALIGNMENT_CENTER);
    }
    Text(t, std::to_wstring(contacts) + (contacts == 1 ? L" finger" : L" fingers"), left,
         bottom + 15, 160, 30, 21, 0xEAF0FA, true);
    Text(t, std::to_wstring(fpsReports) + L" reports/s", right - 95, bottom + 22, 95, 19, 10,
         0x7A8AA2, false, DWRITE_TEXT_ALIGNMENT_TRAILING);
    std::wstring dims = telemetry.connected ? std::to_wstring(telemetry.width) + L" \x00D7 " +
                                                  std::to_wstring(telemetry.height)
                                            : telemetry.reason;
    Text(t, dims, left, bottom + 49, pw, 18, 10, 0x69788F);
    std::wstring pos = L"Real sensor data";
    for (int i = 0; i < 5; ++i)
        if (telemetry.contacts[i].down) {
            auto &c = telemetry.contacts[i];
            pos = L"Finger " + std::to_wstring(i + 1) + L"  X " +
                  std::to_wstring(int(c.x * telemetry.width)) + L"  Y " +
                  std::to_wstring(int((1 - c.y) * telemetry.height));
            break;
        }
    Text(t, pos, left, bottom + 73, pw, 20, 10, 0x536179);
    if (!lastGesture.empty() && now - lastGestureTime < 3500)
        Text(t, lastGesture, left, H - 57, pw, 24, 11, 0x74E7B1);
    Text(t,
         preview               ? L"Preview: simulated contacts"
         : appliedFive.enabled ? L"5-finger shortcuts active while app is open"
                               : L"5-finger shortcuts off",
         left, H - 24, pw, 18, 9, 0x69788F);
    Line(t, split - 3, 86, split - 3, H - 23, 0x252D39);
    float sx = split + 15, sw = W - sx - 22, tabw = sw / 4;
    const wchar_t *tabs[] = {L"2 fingers", L"3 fingers", L"4 fingers", L"5 fingers"};
    for (int i = 0; i < 4; ++i) {
        auto r = R(sx + i * tabw, 86, tabw - 3, 27);
        if (page == i)
            Fill(t, r, 0x29364E, 6);
        else if (hover == 20 + i)
            Fill(t, r, 0x1B2432, 6);
        Text(t, tabs[i], r.left, r.top + 5, tabw - 3, 20, 10, page == i ? 0xC7D8FF : 0x7B8BA3,
             page == i, DWRITE_TEXT_ALIGNMENT_CENTER);
    }
    for (int i = 0; i < PageRows(); ++i) {
        float y = RowY(i);
        bool isToggle = page == 0 || (page == 3 ? i == 0 : i < 2), valid = true, on = false;
        std::wstring label, action;
        if (page == 0) {
            label = Settings[i].title;
            valid = available[i];
            on = toggles[i];
        } else if (page == 3) {
            if (i == 0) {
                label = L"Enable 5-finger shortcuts";
                on = pendingFive.enabled;
            } else {
                const wchar_t *names[] = {L"Tap", L"Swipe up", L"Swipe down", L"Swipe left",
                                          L"Swipe right"};
                label = names[i - 1];
                action = FiveTitles[pendingFive.actions[i - 1]];
            }
        } else if (i < 2) {
            std::wstring n = i == 0 ? L"MultiFingerGesture_Enable" : MasterName();
            label = i == 0 ? L"Multi-finger gestures" : L"Swipes";
            valid = DriverAvailable(n);
            if (i == 1)
                valid = valid && MultiFingerEnabled();
            on = valid && DriverNumber(n) == 1;
        } else {
            auto g = DriverGestures(page == 1 ? 3 : 4)[i - 2];
            label = g.title;
            valid = MultiFingerEnabled() && DriverAvailable(g.action) && DriverAvailable(g.enable);
            action = valid ? (DriverNumber(g.enable) ? ActionTitle(DriverNumber(g.action)) : L"Off")
                           : MultiFingerEnabled() ? L"Unavailable" : L"Off";
        }
        if (hover == i)
            Fill(t, R(sx - 5, y - 3, sw + 9, 35), 0x19212D, 6);
        Text(t, label, sx, y + 6, isToggle ? sw - 50 : 105, 23, 12, valid ? 0xE3EAF5 : 0x58677D,
             true);
        if (isToggle) {
            float tx = W - 60;
            Fill(t, R(tx, y + 6, 38, 21), !valid ? 0x303644 : on ? 0x7199FF : 0x353F50, 11);
            Circle(t, tx + (on ? 27 : 11), y + 16.5f, 7.2f, valid ? 0xFFFFFF : 0x687181);
        } else {
            float x = sx + 99;
            Fill(t, R(x, y + 1, sw - 99, 29), valid ? 0x1C2636 : 0x18202C, 5);
            Text(t, action, x + 9, y + 7, sw - 130, 20, 10, valid ? 0xA9BDDD : 0x58677D);
            Text(t, L"\x2304", W - 42, y + 4, 16, 22, 14, 0x7D8FA9);
        }
        if (focus == i)
            Outline(t, R(sx - 5, y - 3, sw + 9, 35), 0x7199FF, 6, .8f);
        if (i < PageRows() - 1)
            Line(t, sx, y + 34, W - 23, y + 34, 0x252D39, .55f);
    }
    Line(t, split + 12, H - 70, W - 20, H - 70, 0x252D39);
    Text(t, status, split + 15, H - 48, sw - 170, 22, 10, 0x8D9BB0);
    bool dirty = Dirty();
    Fill(t, ResetRect(), hover == 10 ? 0x303C50 : 0x212B3A, 6);
    Outline(t, ResetRect(), focus == 10 ? 0x7199FF : 0x354258, 6);
    Text(t, L"Reset", W - 182, H - 50, 70, 23, 12, 0xD0DAE9, true, DWRITE_TEXT_ALIGNMENT_CENTER);
    Fill(t, ApplyRect(), dirty && !busy ? hover == 11 ? 0x8CAEFF : 0x7199FF : 0x303E5B, 6);
    if (focus == 11)
        Outline(t, ApplyRect(), 0xC3D3FF, 6);
    Text(t, busy ? L"Saving..." : L"Apply", W - 100, H - 50, 78, 23, 12,
         dirty ? 0x101A30 : 0x6A7A97, true, DWRITE_TEXT_ALIGNMENT_CENTER);
    Text(t,
         page == 3 ? L"5-finger actions need the app open (minimized is fine)"
                   : L"Driver gestures persist after closing this app",
         split + 15, H - 16, sw, 16, 8.5, 0x536179);
    HRESULT hr = t->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET && t == target)
        Release(target);
}

void Error(const std::exception &e) {
    MessageBoxW(window, Widen(e.what()).c_str(), L"Touchpad Studio", MB_OK | MB_ICONERROR);
}

void Apply() {
    if (busy || !Dirty())
        return;
    auto changes = Changes();
    auto before = saved;
    bool localChanged = pendingFive != appliedFive || pendingSuspended != appliedSuspended;
    busy = true;
    status = L"Applying...";
    InvalidateRect(window, nullptr, FALSE);
    UpdateWindow(window);
    bool wrote = false;
    try {
        lastBackup = WriteChanges(changes, saved);
        wrote = true;
        if (localChanged) {
            try {
                WriteLocal(pendingFive, pendingSuspended);
            } catch (...) {
                if (!changes.empty()) {
                    Snapshot undo;
                    for (auto &kv : changes)
                        undo[kv.first] = before.at(kv.first);
                    WriteChanges(undo, ReadSettings());
                }
                wrote = false;
                throw;
            }
            appliedFive = pendingFive;
            appliedSuspended = pendingSuspended;
        }
        fiveEngine.Reset();
        if (!changes.empty()) {
            telemetry.Close();
            ReloadElan();
        }
        Reset();
        for (auto &kv : changes)
            if (!(saved.at(kv.first) == kv.second))
                throw std::runtime_error("ELAN changed a setting again after reload");
        status = L"Applied";
        if (!telemetry.connected)
            telemetry.Open();
    } catch (const std::exception &e) {
        if (wrote) {
            try {
                Reset();
            } catch (...) {
            }
            status = L"Saved; reload failed";
            if (!telemetry.connected)
                telemetry.Open();
        } else
            status = L"Could not apply";
        Error(e);
    }
    busy = false;
    InvalidateRect(window, nullptr, FALSE);
}

void LoadBackup() {
    wchar_t file[32768] = {};
    OPENFILENAMEW o{};
    o.lStructSize = sizeof(o);
    o.hwndOwner = window;
    o.lpstrFilter = L"Touchpad Studio backups\0*.elanbackup\0\0";
    o.lpstrFile = file;
    o.nMaxFile = 32768;
    o.Flags = OFN_FILEMUSTEXIST | OFN_EXPLORER;
    std::wstring dir = AppDir() + L"\\ELAN-backups";
    o.lpstrInitialDir = dir.c_str();
    if (!GetOpenFileNameW(&o))
        return;
    try {
        auto source = Decode(ReadBytes(file));
        for (auto &kv : source)
            if (Allowed(kv.first) && !ValidValue(kv.first, kv.second))
                throw std::runtime_error("Unsupported value in backup");
        if (source.count(L"Tap_Two_Finger_Enable") && !source.count(L"Tap_Two_Finger"))
            throw std::runtime_error("Backup is missing the tap action");
        auto five = DecodeFive(source, appliedFive);
        auto suspended = DecodeSuspended(source);
        Reset();
        pendingFive = five;
        pendingSuspended = suspended;
        for (auto &kv : source)
            if (Allowed(kv.first) && DriverAvailable(kv.first)) {
                bool basic = false;
                for (auto &s : Settings)
                    if (kv.first == s.name)
                        basic = true;
                if (!basic && kv.first != L"Tap_Two_Finger")
                    extra[kv.first] = kv.second;
            }
        for (int i = 0; i < 6; ++i)
            if (available[i] && source.count(Settings[i].name)) {
                toggles[i] = Number(source.at(Settings[i].name)) == 1;
                if (i == 0)
                    toggles[i] = toggles[i] && Number(source.at(L"Tap_Two_Finger")) == 1;
            }
        if (source.count(L"Tap_Two_Finger"))
            loadedAction = Number(source.at(L"Tap_Two_Finger"));
        status = Dirty() ? L"Backup loaded" : L"Backup matches";
    } catch (const std::exception &e) {
        Error(e);
    }
    InvalidateRect(window, nullptr, FALSE);
}

void Menu() {
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, 1, L"Load a backup...");
    AppendMenuW(m, MF_STRING, 2, L"Open backup folder");
    AppendMenuW(m, MF_STRING, 3, L"Reconnect live view");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, 4, L"About Touchpad Studio");
    POINT p{LONG((W - 38) * scale), LONG(54 * scale)};
    ClientToScreen(window, &p);
    int choice = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTALIGN, p.x, p.y, 0, window, nullptr);
    DestroyMenu(m);
    if (choice == 1)
        LoadBackup();
    if (choice == 2) {
        std::wstring dir = AppDir() + L"\\ELAN-backups";
        CreateDirectoryW(dir.c_str(), nullptr);
        ShellExecuteW(window, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
    if (choice == 3) {
        fiveEngine.Reset();
        telemetry.Open();
        for (auto &t : trails)
            t.clear();
    }
    if (choice == 4)
        MessageBoxW(window,
                    L"Touchpad Studio\nNative C++ / Win32 / Direct2D\n\n3/4-finger actions use the "
                    L"installed driver.\n5-finger shortcuts run while this app is open.\n\nReads "
                    L"real contacts using the installed ELAN API 11.9.4.1.\nChanges only this "
                    L"user's supported gesture settings.\nApply saves a typed backup and reloads "
                    L"ELAN Control.\nReset discards edits. No driver or keyboard filter "
                    L"changes.\n\nIndependent utility; not an official ELAN application.",
                    L"About", MB_OK);
}

int HitTest(float x, float y) {
    float sx = Split() + 10, sw = W - (Split() + 15) - 22, tabw = sw / 4;
    for (int i = 0; i < 4; ++i)
        if (Hit(x, y, R(Split() + 15 + i * tabw, 86, tabw - 3, 27)))
            return 20 + i;
    for (int i = 0; i < PageRows(); ++i)
        if (Hit(x, y, R(sx, RowY(i) - 3, W - sx - 18, 35)))
            return i;
    if (Hit(x, y, ResetRect()))
        return 10;
    if (Hit(x, y, ApplyRect()))
        return 11;
    if (Hit(x, y, R(W - 53, 19, 30, 28)))
        return 12;
    return -1;
}
void Activate(int id) {
    if (busy)
        return;
    try {
        if (id >= 20 && id <= 23) {
            page = id - 20;
            focus = -1;
        } else if (id >= 0 && id < PageRows()) {
            if (page == 0) {
                if (available[id])
                    toggles[id] = !toggles[id];
            } else if (page == 3) {
                if (id == 0)
                    pendingFive.enabled = !pendingFive.enabled;
                else
                    ChooseAction(id);
            } else if (id < 2) {
                std::wstring n = id == 0 ? L"MultiFingerGesture_Enable" : MasterName();
                if (id == 0 && DriverAvailable(n)) {
                    SetMultiFinger(!MultiFingerEnabled());
                } else if (id == 1 && MultiFingerEnabled() && DriverAvailable(n)) {
                    DWORD value = DriverNumber(n) ? 0 : 1;
                    SetDriver(n, value);
                }
            } else
                ChooseAction(id);
            PendingStatus();
        } else if (id == 10)
            Reset();
        else if (id == 11)
            Apply();
        else if (id == 12)
            Menu();
    } catch (const std::exception &e) {
        Error(e);
    }
    InvalidateRect(window, nullptr, FALSE);
}

LRESULT CALLBACK Proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        SetTimer(h, 1, 25, nullptr);
        return 0;
    case WM_GETMINMAXINFO: {
        auto *m = (MINMAXINFO *)lp;
        m->ptMinTrackSize.x = LONG(700 * scale);
        m->ptMinTrackSize.y = LONG(510 * scale);
        return 0;
    }
    case WM_SIZE:
        if (target)
            target->Resize(D2D1::SizeU(LOWORD(lp), HIWORD(lp)));
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_DPICHANGED: {
        scale = HIWORD(wp) / 96.f;
        auto *r = (RECT *)lp;
        SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        if (target)
            target->SetDpi(96 * scale, 96 * scale);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(h, &ps);
        RECT r;
        GetClientRect(h, &r);
        W = (r.right - r.left) / scale;
        H = (r.bottom - r.top) / scale;
        if (!target) {
            factory->CreateHwndRenderTarget(
                D2D1::RenderTargetProperties(),
                D2D1::HwndRenderTargetProperties(h, D2D1::SizeU(r.right, r.bottom)), &target);
            if (target)
                target->SetDpi(96 * scale, 96 * scale);
        }
        if (target)
            Draw(target);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_MOUSEMOVE: {
        int id = HitTest(GET_X_LPARAM(lp) / scale, GET_Y_LPARAM(lp) / scale);
        if (id != hover) {
            hover = id;
            InvalidateRect(h, nullptr, FALSE);
        }
        TRACKMOUSEEVENT e{sizeof(e), TME_LEAVE, h, 0};
        TrackMouseEvent(&e);
        SetCursor(LoadCursor(nullptr, id >= 0 ? IDC_HAND : IDC_ARROW));
        return 0;
    }
    case WM_MOUSELEAVE:
        hover = -1;
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_LBUTTONUP:
        focus = HitTest(GET_X_LPARAM(lp) / scale, GET_Y_LPARAM(lp) / scale);
        Activate(focus);
        return 0;
    case WM_CONTEXTMENU:
        Menu();
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_TAB) {
            std::vector<int> order = {20, 21, 22, 23};
            for (int i = 0; i < PageRows(); ++i)
                order.push_back(i);
            order.insert(order.end(), {10, 11, 12});
            auto it = std::find(order.begin(), order.end(), focus);
            int pos = it == order.end() ? -1 : int(it - order.begin());
            int step = GetKeyState(VK_SHIFT) < 0 ? -1 : 1;
            focus = order[(pos + step + int(order.size())) % order.size()];
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        if (wp == VK_SPACE || wp == VK_RETURN) {
            Activate(focus);
            return 0;
        }
        if (wp == VK_ESCAPE) {
            Activate(10);
            return 0;
        }
        if (wp == 'S' && GetKeyState(VK_CONTROL) < 0) {
            Activate(11);
            return 0;
        }
        break;
    case WM_TIMER: {
        bool fresh = telemetry.Poll();
        if (telemetry.connected && !busy) {
            for (auto &frame : telemetry.frames) {
                int action = fiveEngine.Feed(frame, GetTickCount64(), appliedFive);
                if (action)
                    DispatchFive(action);
            }
            if (telemetry.frames.empty()) {
                int action = fiveEngine.Feed(telemetry.contacts, GetTickCount64(), appliedFive);
                if (action)
                    DispatchFive(action);
            }
        }
        bool visualChange = fresh;
        for (auto &trail : trails)
            if (!trail.empty())
                visualChange = true;
        ULONGLONG now = GetTickCount64();
        if (fresh)
            for (int i = 0; i < 5; ++i) {
                auto &c = telemetry.contacts[i];
                if (c.down)
                    trails[i].push_back({c.x, c.y, now});
            }
        for (auto &t : trails)
            while (!t.empty() && (now - t.front().time > 700 || t.size() > 48))
                t.pop_front();
        if (now - rateTime >= 1000) {
            visualChange = true;
            fpsReports = (DWORD)((telemetry.records - previousRecords) * 1000 / (now - rateTime));
            previousRecords = telemetry.records;
            rateTime = now;
        }
        if (visualChange && !IsIconic(h))
            InvalidateRect(h, nullptr, FALSE);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(h, 1);
        telemetry.Close();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}
void InitGraphics() {
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &factory)))
        throw std::runtime_error("Direct2D could not start");
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                   (IUnknown **)&writes)))
        throw std::runtime_error("DirectWrite could not start");
}
int SelfTest(const std::wstring &path) {
    std::wofstream log(path.c_str());
    try {
        Reset();
        Snapshot original = saved;
        bool all = true;
        for (bool a : available)
            all = all && a;
        log << L"All six settings available: " << all << L"\n";
        if (!all)
            throw std::runtime_error("A setting is unavailable");
        if (!Changes().empty())
            throw std::runtime_error("Initial state is dirty");
        for (int i = 0; i < 6; ++i) {
            toggles[i] = !toggles[i];
            auto c = Changes();
            if (!c.count(Settings[i].name))
                throw std::runtime_error("Toggle did not create expected change");
            toggles[i] = !toggles[i];
        }
        log << L"All six toggle mappings: PASS\n";
        toggles[0] = false;
        loadedAction = 7;
        auto action = Changes();
        if (Number(saved.at(L"Tap_Two_Finger")) != 7 &&
            (!action.count(L"Tap_Two_Finger") || Number(action.at(L"Tap_Two_Finger")) != 7))
            throw std::runtime_error("Backup action staging failed");
        Reset();
        if (!Changes().empty() || loadedAction != -1)
            throw std::runtime_error("Reset failed");
        log << L"Reset and backup staging: PASS\n";
        auto encoded = Encode(saved);
        if (Decode(encoded) != saved)
            throw std::runtime_error("Typed backup round trip failed");
        log << L"Full typed backup serialization: PASS (" << saved.size() << L" entries)\n";
        encoded.pop_back();
        bool rejected = false;
        try {
            Decode(encoded);
        } catch (...) {
            rejected = true;
        }
        if (!rejected)
            throw std::runtime_error("Truncated backup accepted");
        log << L"Malformed backup rejected: PASS\n";
        if (ReadSettings() != original)
            throw std::runtime_error("Unexpected live setting change");
        log << L"Live registry unchanged: PASS\n";
        bool connected = telemetry.Open();
        log << L"Contact API connection: " << connected << L"\nSensor: " << telemetry.width
            << L" x " << telemetry.height << L"\n";
        if (connected)
            for (int i = 0; i < 20; ++i) {
                telemetry.Poll();
                Sleep(25);
            }
        log << L"Contact records received while testing: " << telemetry.records << L"\n";
        telemetry.Close();
        for (int f : {3, 4})
            for (auto &g : DriverGestures(f)) {
                if (!DriverAvailable(g.action) || !DriverAvailable(g.enable))
                    throw std::runtime_error("Multi-finger field unavailable");
                DWORD old = DriverNumber(g.action);
                SetDriver(g.action, old == 19 ? 2569 : 19);
                if (!Changes().count(g.action))
                    throw std::runtime_error("Multi-finger action did not stage");
                Reset();
            }
        log << L"All 3/4-finger tap and swipe mappings: PASS\n";
        auto fiveEncoded = WithAppSettings(saved);
        FiveConfig round = DecodeFive(Decode(Encode(fiveEncoded)), {});
        if (round != appliedFive)
            throw std::runtime_error("Five-finger config round trip failed");
        FiveConfig cfg;
        cfg.enabled = true;
        cfg.actions = {FiveDesktop, FiveTaskView, FiveMute, FivePreviousDesktop, FiveNextDesktop};
        FiveFingerRecognizer engine;
        std::array<Contact, 5> points{};
        for (int i = 0; i < 5; ++i)
            points[i] = {true, .3f + .08f * i, .5f};
        auto empty = std::array<Contact, 5>{};
        engine.Feed(points, 1000, cfg);
        engine.Feed(points, 1150, cfg);
        if (engine.Feed(empty, 1200, cfg) != FiveDesktop)
            throw std::runtime_error("Five-finger tap failed");
        if (engine.Feed(empty, 1225, cfg) != FiveOff)
            throw std::runtime_error("Duplicate tap fired");
        for (int dir = 1; dir <= 4; ++dir) {
            engine.Reset();
            std::array<Contact, 5> moved = points;
            for (auto &p : moved) {
                if (dir == 1)
                    p.y -= .15f;
                if (dir == 2)
                    p.y += .15f;
                if (dir == 3)
                    p.x -= .15f;
                if (dir == 4)
                    p.x += .15f;
            }
            engine.Feed(points, 2000, cfg);
            if (engine.Feed(moved, 2180, cfg) != FiveOff)
                throw std::runtime_error("Gesture fired before lift");
            if (engine.Feed(empty, 2250, cfg) != cfg.actions[dir])
                throw std::runtime_error("Five-finger swipe failed");
        }
        engine.Reset();
        auto four = points;
        four[4].down = false;
        engine.Feed(four, 3000, cfg);
        if (engine.Feed(empty, 3200, cfg) != FiveOff)
            throw std::runtime_error("Four fingers triggered five-finger gesture");
        engine.Reset();
        engine.Feed(points, 4000, cfg);
        engine.Feed(points, 5800, cfg);
        if (engine.Feed(empty, 5900, cfg) != FiveOff)
            throw std::runtime_error("Long hold triggered tap");
        engine.Reset();
        auto spread = points;
        for (int i = 0; i < 5; ++i)
            spread[i].x += (i - 2) * .07f;
        engine.Feed(points, 6000, cfg);
        engine.Feed(spread, 6200, cfg);
        if (engine.Feed(empty, 6300, cfg) != FiveOff)
            throw std::runtime_error("Spread triggered swipe");
        engine.Reset();
        engine.Feed(points, 7000, cfg);
        engine.Feed(four, 7150, cfg);
        engine.Feed(points, 7200, cfg);
        if (engine.Feed(empty, 7300, cfg) != FiveOff)
            throw std::runtime_error("Re-touch triggered gesture");
        cfg.enabled = false;
        engine.Feed(points, 8000, cfg);
        if (engine.Feed(empty, 8200, cfg) != FiveOff)
            throw std::runtime_error("Disabled recognizer fired");
        log << L"Five-finger tap, four swipe directions, disabled, duplicate, four-finger, hold, "
               L"spread and re-touch tests: PASS\n";
        log << L"Five-finger typed backup settings: PASS\n";
        if (ReadSettings() != original)
            throw std::runtime_error("A test changed live settings");
        log << L"Live registry unchanged after all tests: PASS\nRESULT: PASS\n";
        return 0;
    } catch (const std::exception &e) {
        log << L"FAIL: " << Widen(e.what()) << L"\n";
        return 1;
    }
}
int Preview(const std::wstring &path) {
    preview = true;
    Reset();
    telemetry.connected = false;
    telemetry.width = 3209;
    telemetry.height = 2097;
    telemetry.reason = L"PREVIEW ONLY - simulated contact positions";
    telemetry.contacts[0] = {true, .34f, .48f};
    telemetry.contacts[1] = {true, .66f, .43f};
    ULONGLONG now = GetTickCount64();
    for (int i = 0; i < 22; ++i) {
        trails[0].push_back({.48f - i * .0067f, .53f - i * .0024f, now - ULONGLONG(22 - i) * 22});
        trails[1].push_back({.52f + i * .0067f, .53f - i * .0048f, now - ULONGLONG(22 - i) * 22});
    }
    W = 720;
    H = 470;
    IWICImagingFactory *wic = nullptr;
    IWICBitmap *bitmap = nullptr;
    ID2D1RenderTarget *rt = nullptr;
    IWICStream *stream = nullptr;
    IWICBitmapEncoder *enc = nullptr;
    IWICBitmapFrameEncode *frame = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&wic));
    if (SUCCEEDED(hr))
        hr = wic->CreateBitmap(720, 470, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad,
                               &bitmap);
    if (SUCCEEDED(hr))
        hr = factory->CreateWicBitmapRenderTarget(bitmap, D2D1::RenderTargetProperties(), &rt);
    if (SUCCEEDED(hr))
        Draw(rt);
    if (SUCCEEDED(hr))
        hr = wic->CreateStream(&stream);
    if (SUCCEEDED(hr))
        hr = stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE);
    if (SUCCEEDED(hr))
        hr = wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc);
    if (SUCCEEDED(hr))
        hr = enc->Initialize(stream, WICBitmapEncoderNoCache);
    if (SUCCEEDED(hr))
        hr = enc->CreateNewFrame(&frame, nullptr);
    if (SUCCEEDED(hr))
        hr = frame->Initialize(nullptr);
    if (SUCCEEDED(hr))
        hr = frame->WriteSource(bitmap, nullptr);
    if (SUCCEEDED(hr))
        hr = frame->Commit();
    if (SUCCEEDED(hr))
        hr = enc->Commit();
    Release(frame);
    Release(enc);
    Release(stream);
    Release(rt);
    Release(bitmap);
    Release(wic);
    return FAILED(hr) ? 1 : 0;
}
} // namespace app

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int show) {
    using namespace app;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    int result = 0;
    try {
        InitGraphics();
        if (argc >= 3 && std::wstring(argv[1]) == L"--self-test") {
            ReadLocal();
            result = SelfTest(argv[2]);
        } else if (argc >= 3 && std::wstring(argv[1]) == L"--preview") {
            if (argc >= 4) {
                int fingers = _wtoi(argv[3]);
                if (fingers >= 2 && fingers <= 5)
                    page = fingers - 2;
            }
            result = Preview(argv[2]);
        } else {
            HANDLE mutex = CreateMutexW(nullptr, FALSE, L"Local\\ElanTouchpadStudio");
            if (GetLastError() == ERROR_ALREADY_EXISTS) {
                HWND old = FindWindowW(ClassName, nullptr);
                if (old) {
                    ShowWindow(old, SW_RESTORE);
                    SetForegroundWindow(old);
                }
                if (mutex)
                    CloseHandle(mutex);
                LocalFree(argv);
                return 0;
            }
            ReadLocal();
            Reset();
            telemetry.Open();
            scale = GetDpiForSystem() / 96.f;
            WNDCLASSW wc{};
            wc.hInstance = instance;
            wc.lpfnWndProc = Proc;
            wc.lpszClassName = ClassName;
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
            RegisterClassW(&wc);
            RECT rect{0, 0, LONG(720 * scale), LONG(470 * scale)};
            AdjustWindowRectExForDpi(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0, UINT(scale * 96));
            window = CreateWindowExW(0, ClassName, L"Touchpad Studio", WS_OVERLAPPEDWINDOW,
                                     CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left,
                                     rect.bottom - rect.top, nullptr, nullptr, instance, nullptr);
            if (!window)
                throw std::runtime_error("Window creation failed");
            BOOL dark = TRUE;
            DwmSetWindowAttribute(window, 20, &dark, sizeof(dark));
            ShowWindow(window, show);
            MSG msg;
            while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            if (mutex)
                CloseHandle(mutex);
        }
    } catch (const std::exception &e) {
        Error(e);
        result = 1;
    }
    Release(target);
    for (auto &font : fonts)
        Release(font.second);
    Release(writes);
    Release(factory);
    if (argv)
        LocalFree(argv);
    CoUninitialize();
    return result;
}
