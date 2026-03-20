#include <windows.h>
#include <shlobj.h>
#include <atlbase.h>
#include <string>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "version.lib")

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

// New: Check if exe requires admin rights
BOOL IsExeRequireAdmin(const std::wstring& exePath)
{
    DWORD dwSize = GetFileVersionInfoSizeW(exePath.c_str(), NULL);
    if (dwSize == 0) return FALSE;

    BYTE* pData = new BYTE[dwSize];
    if (!GetFileVersionInfoW(exePath.c_str(), 0, dwSize, pData))
    {
        delete[] pData;
        return FALSE;
    }

    VS_FIXEDFILEINFO* pFixedInfo = NULL;
    UINT uLen = 0;
    if (!VerQueryValueW(pData, L"\\", (void**)&pFixedInfo, &uLen))
    {
        delete[] pData;
        return FALSE;
    }

    BOOL bRequireAdmin = FALSE;
    wchar_t szSubBlock[256];
    WORD langCode = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
    WORD codepage = 1200;

    wsprintfW(szSubBlock, L"\\StringFileInfo\\%04x%04x\\RequestedExecutionLevel", langCode, codepage);
    wchar_t* pValue = NULL;
    if (VerQueryValueW(pData, szSubBlock, (void**)&pValue, &uLen) && pValue)
    {
        if (_wcsicmp(pValue, L"requireAdministrator") == 0)
        {
            bRequireAdmin = TRUE;
        }
    }

    delete[] pData;
    return bRequireAdmin;
}

// Modified: Dynamic run with/without admin
BOOL RunProcess(const std::wstring& path, const std::wstring& args, BOOL requireAdmin)
{
    SHELLEXECUTEINFOW sei = { 0 };
    sei.cbSize = sizeof(SHELLEXECUTEINFOW);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.hwnd = NULL;
    sei.lpVerb = requireAdmin ? L"runas" : NULL;
    sei.lpFile = path.c_str();
    sei.lpParameters = args.c_str();
    sei.lpDirectory = NULL;
    sei.nShow = SW_HIDE;
    sei.hInstApp = NULL;

    return ShellExecuteExW(&sei);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    HRESULT hrCoInit = CoInitialize(NULL);
    if (FAILED(hrCoInit))
    {
        MessageBoxW(NULL, L"COM initialization failed", L"Error", MB_ICONERROR);
        return 1;
    }

    const std::wstring shortcutPath = L"D:\\var\\明日方舟-MuMu安卓设备.lnk";

    std::wstring targetPath = GetShortcutTarget(shortcutPath);
    std::wstring arguments = GetShortcutArguments(shortcutPath);

    if (targetPath.empty())
    {
        MessageBoxW(NULL, L"Failed to read shortcut target", L"Error", MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    // New: Check admin requirement before run
    BOOL bNeedAdmin = IsExeRequireAdmin(targetPath);
    if (!RunProcess(targetPath, arguments, bNeedAdmin))
    {
        DWORD err = GetLastError();
        if (err != ERROR_CANCELLED)
        {
            WCHAR errMsg[256];
            wsprintfW(errMsg, L"Failed to launch application, error code: %d", err);
            MessageBoxW(NULL, errMsg, L"Error", MB_ICONERROR);
        }
    }

    CoUninitialize();
    return 0;
}