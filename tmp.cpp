#include <windows.h>
#include <shlobj.h>
#include <objbase.h>
#include <string>

#pragma comment(lib, "shell32")
#pragma comment(lib, "ole32")
#pragma comment(lib, "version")
#pragma comment(lib, "user32")

// 定义COM接口ID的GUID值
extern "C" {
    const IID IID_IShellLinkW = {0x000214F9, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
    const IID IID_IPersistFile = {0x0000010b, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
}

BOOL IsExeRequireAdmin(const std::wstring& exePath)
{
    DWORD dwSize = GetFileVersionInfoSizeW(exePath.c_str(), NULL);
    if (dwSize == 0) return FALSE;

    BYTE* pData = new BYTE[dwSize];
    if (!GetFileVersionInfoW(exePath.c_str(), 0, dwSize, pData)) {
        delete[] pData;
        return FALSE;
    }

    VS_FIXEDFILEINFO* pFixedInfo = NULL;
    UINT uLen = 0;
    if (!VerQueryValueW(pData, L"\\", (void**)&pFixedInfo, &uLen)) {
        delete[] pData;
        return FALSE;
    }

    BOOL bRequireAdmin = FALSE;
    wchar_t szSubBlock[256];
    WORD langCode = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
    WORD codepage = 1200;

    wsprintfW(szSubBlock, L"\\StringFileInfo\\%%04x%%04x\\RequestedExecutionLevel", langCode, codepage);
    wchar_t* pValue = NULL;
    if (VerQueryValueW(pData, szSubBlock, (void**)&pValue, &uLen) && pValue) {
        if (_wcsicmp(pValue, L"requireAdministrator") == 0) bRequireAdmin = TRUE;
    }

    delete[] pData;
    return bRequireAdmin;
}

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
    sei.nShow = SW_SHOWNORMAL;
    sei.hInstApp = NULL;
    return ShellExecuteExW(&sei);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    HRESULT hrCoInit = CoInitialize(NULL);
    if (FAILED(hrCoInit)) {
        MessageBoxW(NULL, L"COM initialization failed", L"Error", MB_ICONERROR);
        return 1;
    }

    const std::wstring targetPath = L"D:\\Software\\MCHOSE HUB\\MCHOSE HUB.exe";
    const std::wstring arguments = L"";

    if (targetPath.empty()) {
        MessageBoxW(NULL, L"Target path is empty", L"Error", MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    BOOL bNeedAdmin = IsExeRequireAdmin(targetPath);
    if (!RunProcess(targetPath, arguments, bNeedAdmin)) {
        DWORD err = GetLastError();
        if (err != ERROR_CANCELLED) {
            WCHAR errMsg[256];
            wsprintfW(errMsg, L"Failed to launch, error: %d", err);
            MessageBoxW(NULL, errMsg, L"Error", MB_ICONERROR);
        }
    }

    CoUninitialize();
    return 0;
}
