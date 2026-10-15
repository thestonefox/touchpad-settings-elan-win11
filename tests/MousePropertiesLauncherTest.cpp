#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <cstdio>
#include <vector>
static const CLSID id = {
    0xddbd1a52, 0x717f, 0x43ac, {0xac, 0x2a, 0x73, 0x26, 0x2e, 0x8a, 0x31, 0xb8}};
static BOOL CALLBACK Add(HPROPSHEETPAGE p, LPARAM v) {
    reinterpret_cast<std::vector<HPROPSHEETPAGE> *>(v)->push_back(p);
    return TRUE;
}
int wmain(int argc, wchar_t **argv) {
    if (argc != 2)
        return 2;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_WIN95_CLASSES};
    InitCommonControlsEx(&controls);
    HMODULE dll = LoadLibraryExW(argv[1], nullptr,
                                 LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!dll) {
        printf("Load failed: %lu\n", GetLastError());
        return 1;
    }
    using Get = HRESULT(__stdcall *)(REFCLSID, REFIID, void **);
    using Unload = HRESULT(__stdcall *)();
    auto get = (Get)GetProcAddress(dll, "DllGetClassObject");
    auto unload = (Unload)GetProcAddress(dll, "DllCanUnloadNow");
    if (!get || !unload || unload() != S_OK)
        return 3;
    IClassFactory *f = nullptr;
    HRESULT hr = get(id, IID_PPV_ARGS(&f));
    if (FAILED(hr) || !f)
        return 4;
    IShellPropSheetExt *ext = nullptr;
    hr = f->CreateInstance(nullptr, IID_PPV_ARGS(&ext));
    if (FAILED(hr) || !ext)
        return 5;
    f->Release();
    IShellExtInit *init = nullptr;
    hr = ext->QueryInterface(IID_PPV_ARGS(&init));
    if (FAILED(hr) || !init || FAILED(init->Initialize(nullptr, nullptr, nullptr)))
        return 6;
    init->Release();
    std::vector<HPROPSHEETPAGE> pages;
    hr = ext->AddPages(Add, reinterpret_cast<LPARAM>(&pages));
    if (FAILED(hr) || pages.size() != 1)
        return 7;
    if (ext->ReplacePage(0, Add, reinterpret_cast<LPARAM>(&pages)) != S_FALSE || pages.size() != 1)
        return 8;
    ext->Release();
    if (unload() != S_FALSE)
        return 9;
    for (auto p : pages)
        DestroyPropertySheetPage(p);
    if (unload() != S_OK)
        return 10;
    FreeLibrary(dll);
    CoUninitialize();
    printf("PASS: COM activation, initialization, added page, preserved standard pages, page "
           "lifetime, unload. No registration or gesture changes.\n");
    return 0;
}
