#define wWinMain UnusedTouchpadStudioGuiEntry
#include "../src/TouchpadStudio.cpp"
#undef wWinMain
#include <cstdio>

// Exercise the production launch call using only our harmless fixture child.
// No installed driver is stopped, launched or connected to by this test.
int wmain(int argc, wchar_t **argv) {
    if (argc != 2)
        return 2;
    wchar_t before[32768], after[32768];
    DWORD beforeLength = GetCurrentDirectoryW(32768, before);
    if (!beforeLength || beforeLength >= 32768)
        return 3;
    PROCESS_INFORMATION child{};
    if (!app::StartElanControl(argv[1], child)) {
        std::fprintf(stderr, "FAIL: fixture launch (%lu).\n", GetLastError());
        return 4;
    }
    CloseHandle(child.hThread);
    DWORD wait = WaitForSingleObject(child.hProcess, 5000);
    if (wait != WAIT_OBJECT_0) {
        // This handle belongs exclusively to the fixture we just launched.
        TerminateProcess(child.hProcess, 5);
        WaitForSingleObject(child.hProcess, 5000);
    }
    DWORD result = 1;
    bool exited = GetExitCodeProcess(child.hProcess, &result) != FALSE;
    CloseHandle(child.hProcess);
    DWORD afterLength = GetCurrentDirectoryW(32768, after);
    if (wait != WAIT_OBJECT_0 || !exited || result != 0) {
        std::fprintf(stderr, "FAIL: child inherited the wrong working directory or failed.\n");
        return 5;
    }
    if (!afterLength || afterLength >= 32768 || _wcsicmp(before, after) != 0) {
        std::fprintf(stderr, "FAIL: parent working directory changed.\n");
        return 6;
    }
    std::printf("PASS: explicit child working directory and unchanged parent directory; "
                "no installed ELAN process accessed.\n");
    return 0;
}
