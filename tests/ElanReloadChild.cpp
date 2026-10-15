#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

// Harmless ETDCtrl.exe stand-in, compiled into an ignored test fixture.
// It checks its working directory and exits; it never loads the vendor API.
int wmain() {
    wchar_t executable[32768], directory[32768];
    DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
    DWORD currentLength = GetCurrentDirectoryW(32768, directory);
    if (!length || length >= 32768 || !currentLength || currentLength >= 32768)
        return 2;
    std::wstring expected(executable, length);
    auto separator = expected.find_last_of(L"\\/");
    if (separator == std::wstring::npos)
        return 3;
    expected.resize(separator);
    return _wcsicmp(expected.c_str(), directory) == 0 ? 0 : 1;
}
