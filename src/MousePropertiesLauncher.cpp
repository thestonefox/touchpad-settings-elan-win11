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
#include <string>
#include <new>

// A per-user, unmanaged Mouse Properties extension. It does not load the
// ELAN API, inspect input, or change device/gesture settings. Its only
// action is starting the sibling TouchpadStudio.exe after a button click.
static const CLSID CLSID_StudioLauncher = {
    0xddbd1a52, 0x717f, 0x43ac, {0xac, 0x2a, 0x73, 0x26, 0x2e, 0x8a, 0x31, 0xb8}};
static HINSTANCE module = nullptr;
static LONG references = 0;

static std::wstring Executable() {
    wchar_t path[32768];
    DWORD n = GetModuleFileNameW(module, path, 32768);
    if (!n || n >= 32768)
        return L"";
    std::wstring p(path, n);
    size_t slash = p.find_last_of(L"\\/");
    if (slash == std::wstring::npos)
        return L"";
    return p.substr(0, slash + 1) + L"TouchpadStudio.exe";
}
static bool Launch(HWND owner) {
    std::wstring path = Executable();
    if (path.empty() || GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(owner,
                    L"TouchpadStudio.exe was not found beside the launcher DLL. Keep both files in "
                    L"the same folder, or reinstall the launcher from the new folder.",
                    L"Touchpad Studio", MB_OK | MB_ICONERROR);
        return false;
    }
    std::wstring command = L"\"" + path + L"\"",
                 directory = path.substr(0, path.find_last_of(L"\\/"));
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(path.c_str(), &command[0], nullptr, nullptr, FALSE, 0, nullptr,
                        directory.c_str(), &startup, &process)) {
        MessageBoxW(
            owner,
            L"Windows could not open Touchpad Studio. Try opening TouchpadStudio.exe directly.",
            L"Touchpad Studio", MB_OK | MB_ICONERROR);
        return false;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}
static INT_PTR CALLBACK PageProc(HWND page, UINT message, WPARAM w, LPARAM l) {
    if (message == WM_INITDIALOG) {
        std::wstring path = Executable();
        EnableWindow(GetDlgItem(page, 1001),
                     !path.empty() && GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES);
        return TRUE;
    }
    if (message == WM_COMMAND && LOWORD(w) == 1001 && HIWORD(w) == BN_CLICKED) {
        if (Launch(page))
            SetDlgItemTextW(page, 1002, L"Touchpad Studio is open.");
        return TRUE;
    }
    if (message == WM_NOTIFY) {
        auto *n = reinterpret_cast<NMHDR *>(l);
        if (n->code == PSN_APPLY) {
            SetWindowLongPtrW(page, DWLP_MSGRESULT, PSNRET_NOERROR);
            return TRUE;
        }
    }
    return FALSE;
}
class Launcher final : public IShellExtInit, public IShellPropSheetExt {
    LONG refs = 1;

  public:
    Launcher() {
        InterlockedIncrement(&references);
    }
    ~Launcher() {
        InterlockedDecrement(&references);
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **object) override {
        if (!object)
            return E_POINTER;
        *object = nullptr;
        if (IsEqualIID(iid, IID_IUnknown) || IsEqualIID(iid, IID_IShellExtInit))
            *object = static_cast<IShellExtInit *>(this);
        else if (IsEqualIID(iid, IID_IShellPropSheetExt))
            *object = static_cast<IShellPropSheetExt *>(this);
        else
            return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return InterlockedIncrement(&refs);
    }
    ULONG STDMETHODCALLTYPE Release() override {
        LONG n = InterlockedDecrement(&refs);
        if (!n)
            delete this;
        return n;
    }
    HRESULT STDMETHODCALLTYPE Initialize(PCIDLIST_ABSOLUTE, IDataObject *, HKEY) override {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE AddPages(LPFNADDPROPSHEETPAGE add, LPARAM data) override {
        if (!add)
            return E_INVALIDARG;
        PROPSHEETPAGEW p{};
        p.dwSize = sizeof(p);
        p.dwFlags = PSP_USETITLE | PSP_USEREFPARENT;
        p.hInstance = module;
        p.pszTemplate = MAKEINTRESOURCEW(201);
        p.pszTitle = L"Touchpad Studio";
        p.pfnDlgProc = PageProc;
        p.pcRefParent = reinterpret_cast<UINT *>(&references);
        HPROPSHEETPAGE page = CreatePropertySheetPageW(&p);
        if (!page)
            return HRESULT_FROM_WIN32(GetLastError());
        if (!add(page, data)) {
            DestroyPropertySheetPage(page);
            return S_FALSE;
        }
        return S_OK;
    }
    // Preserve every standard Windows page. This extension only adds a tab.
    HRESULT STDMETHODCALLTYPE ReplacePage(UINT, LPFNADDPROPSHEETPAGE, LPARAM) override {
        return S_FALSE;
    }
};
class Factory final : public IClassFactory {
    LONG refs = 1;

  public:
    Factory() {
        InterlockedIncrement(&references);
    }
    ~Factory() {
        InterlockedDecrement(&references);
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **object) override {
        if (!object)
            return E_POINTER;
        *object = nullptr;
        if (!IsEqualIID(iid, IID_IUnknown) && !IsEqualIID(iid, IID_IClassFactory))
            return E_NOINTERFACE;
        *object = static_cast<IClassFactory *>(this);
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return InterlockedIncrement(&refs);
    }
    ULONG STDMETHODCALLTYPE Release() override {
        LONG n = InterlockedDecrement(&refs);
        if (!n)
            delete this;
        return n;
    }
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown *outer, REFIID iid, void **object) override {
        if (!object)
            return E_POINTER;
        *object = nullptr;
        if (outer)
            return CLASS_E_NOAGGREGATION;
        auto *p = new (std::nothrow) Launcher;
        if (!p)
            return E_OUTOFMEMORY;
        HRESULT hr = p->QueryInterface(iid, object);
        p->Release();
        return hr;
    }
    HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) override {
        if (lock)
            InterlockedIncrement(&references);
        else
            InterlockedDecrement(&references);
        return S_OK;
    }
};
extern "C" HRESULT __stdcall DllCanUnloadNow() {
    return references == 0 ? S_OK : S_FALSE;
}
extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID clsid, REFIID iid, void **object) {
    if (!object)
        return E_POINTER;
    *object = nullptr;
    if (!IsEqualCLSID(clsid, CLSID_StudioLauncher))
        return CLASS_E_CLASSNOTAVAILABLE;
    auto *f = new (std::nothrow) Factory;
    if (!f)
        return E_OUTOFMEMORY;
    HRESULT hr = f->QueryInterface(iid, object);
    f->Release();
    return hr;
}
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH)
        module = instance;
    return TRUE;
}
