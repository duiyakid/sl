#include <windows.h>
#include <shlobj.h>
#include <atlbase.h>
#include <string>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup")

std::wstring GetShortcutTarget(const std::wstring& shortcutPath)
{
    CComPtr<IShellLinkW> pShellLink;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&pShellLink);
    if (FAILED(hr)) return L"";

    CComPtr<IPersistFile> pPersistFile;
    hr = pShellLink->QueryInterface(IID_IPersistFile, (void**)&pPersistFile);
    if (FAILED(hr)) return L"";

    hr = pPersistFile->Load(shortcutPath.c_str(), STGM_READ);
    if (FAILED(hr)) return L"";

    WCHAR szTarget[MAX_PATH] = { 0 };
    WIN32_FIND_DATAW wfd = { 0 };
    hr = pShellLink->GetPath(szTarget, MAX_PATH, &wfd, SLGP_UNCPRIORITY);
    if (FAILED(hr)) return L"";

    return std::wstring(szTarget);
}

std::wstring GetShortcutArguments(const std::wstring& shortcutPath)
{
    CComPtr<IShellLinkW> pShellLink;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&pShellLink);
    if (FAILED(hr)) return L"";

    CComPtr<IPersistFile> pPersistFile;
    hr = pShellLink->QueryInterface(IID_IPersistFile, (void**)&pPersistFile);
    if (FAILED(hr)) return L"";

    hr = pPersistFile->Load(shortcutPath.c_str(), STGM_READ);
    if (FAILED(hr)) return L"";

    WCHAR szArgs[MAX_PATH] = { 0 };
    hr = pShellLink->GetArguments(szArgs, MAX_PATH);
    if (FAILED(hr)) return L"";

    return std::wstring(szArgs);
}

BOOL RunProcessAsAdmin(const std::wstring& path, const std::wstring& args)
{
    SHELLEXECUTEINFOW sei = { 0 };
    sei.cbSize = sizeof(SHELLEXECUTEINFOW);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.hwnd = NULL;
    sei.lpVerb = L"runas";
    sei.lpFile = path.c_str();
    sei.lpParameters = args.c_str();
    sei.lpDirectory = NULL;
    sei.nShow = SW_HIDE;
    sei.hInstApp = NULL;

    return ShellExecuteExW(&sei);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    CoInitialize(NULL);

    const std::wstring shortcutPath = L"D:\\var\\Bandicam.lnk";

    std::wstring targetPath = GetShortcutTarget(shortcutPath);
    std::wstring arguments = GetShortcutArguments(shortcutPath);

    if (targetPath.empty())
    {
        MessageBoxW(NULL, L"Failed to read shortcut target", L"Error", MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    if (!RunProcessAsAdmin(targetPath, arguments))
    {
        DWORD err = GetLastError();
        if (err != ERROR_CANCELLED)
        {
            MessageBoxW(NULL, L"Failed to launch application", L"Error", MB_ICONERROR);
        }
    }

    CoUninitialize();
    return 0;
}