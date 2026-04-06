#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <wchar.h>
#include <io.h>
#include <fcntl.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

#define MAX_RECORDS 1024
#define MAX_LINE 4096

typedef struct {
    WCHAR name[256];
    WCHAR path[4096];
} SoftRecord;

SoftRecord g_softs[MAX_RECORDS];
int g_count = 0;

// ==============================================
// 1. 解析 URL 快捷方式（.url）获取网址
// ==============================================
BOOL GetUrlTargetPath(const WCHAR* urlPath, WCHAR* targetPath, int maxTarget)
{
    return GetPrivateProfileStringW(L"InternetShortcut", L"URL", L"",
        targetPath, maxTarget, urlPath) > 0;
}

// ==============================================
// 2. 解析 LNK 快捷方式（.lnk）获取真实路径
// ==============================================
BOOL GetLnkTargetPath(const WCHAR* lnkPath, WCHAR* targetPath, int maxTarget)
{
    IShellLinkW* psl = NULL;
    IPersistFile* ppf = NULL;
    HRESULT hr;

    CoInitialize(NULL);

    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
        &IID_IShellLinkW, (void**)&psl);
    if (FAILED(hr)) goto fail;

    hr = psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile, (void**)&ppf);
    if (FAILED(hr)) goto fail2;

    hr = ppf->lpVtbl->Load(ppf, lnkPath, STGM_READ);
    if (FAILED(hr)) goto fail3;

    psl->lpVtbl->Resolve(psl, NULL, SLR_NO_UI);
    psl->lpVtbl->GetPath(psl, targetPath, maxTarget, NULL, SLGP_RAWPATH);

fail3:
    ppf->lpVtbl->Release(ppf);
fail2:
    psl->lpVtbl->Release(psl);
fail:
    CoUninitialize();
    return SUCCEEDED(hr);
}

// ==============================================
// 3. 统一接口：自动识别 lnk / url
// ==============================================
BOOL GetTargetPath(const WCHAR* filePath, WCHAR* targetPath, int maxTarget)
{
    targetPath[0] = 0;

    // 先尝试解析 url
    if (GetUrlTargetPath(filePath, targetPath, maxTarget))
        return TRUE;

    // 再尝试解析 lnk
    if (GetLnkTargetPath(filePath, targetPath, maxTarget))
        return TRUE;

    return FALSE;
}

// ==============================================
// 4. 从路径提取文件名（不含后缀）
// ==============================================
void GetFileNameFromPath(const WCHAR* path, WCHAR* name, int maxName)
{
    const WCHAR* p = wcsrchr(path, L'\\');
    if (!p) p = path;
    else p++;

    wcsncpy_s(name, maxName, p, _TRUNCATE);

    WCHAR* dot = wcsrchr(name, L'.');
    if (dot) *dot = 0;
}

// ==============================================
// 5. 获取 exe 所在目录 + 配置文件路径
// ==============================================
void GetConfigPath(WCHAR* cfgPath, int maxLen)
{
    WCHAR exePath[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    WCHAR* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) *lastSlash = 0;

    swprintf_s(cfgPath, maxLen, L"%s\\sl_config.txt", exePath);
}

// ==============================================
// 6. 加载配置文件到内存
// ==============================================
void LoadConfig()
{
    WCHAR cfgPath[MAX_PATH];
    GetConfigPath(cfgPath, MAX_PATH);

    g_count = 0;
    FILE* f = _wfopen(cfgPath, L"r, ccs=UTF-8");
    if (!f) return;

    WCHAR line[MAX_LINE];
    while (fgetws(line, MAX_LINE, f))
    {
        WCHAR* nl = wcschr(line, L'\n');
        if (nl) *nl = 0;

        WCHAR* sep = wcschr(line, L'|');
        if (!sep) continue;

        *sep = 0;
        SoftRecord* r = &g_softs[g_count++];
        wcsncpy_s(r->name, _countof(r->name), line, _TRUNCATE);
        wcsncpy_s(r->path, _countof(r->path), sep + 1, _TRUNCATE);

        if (g_count >= MAX_RECORDS) break;
    }
    fclose(f);
}

// ==============================================
// 7. 保存配置
// ==============================================
void SaveConfig()
{
    WCHAR cfgPath[MAX_PATH];
    GetConfigPath(cfgPath, MAX_PATH);

    FILE* f = _wfopen(cfgPath, L"w, ccs=UTF-8");
    if (!f) return;

    for (int i = 0; i < g_count; i++)
    {
        fwprintf(f, L"%s|%s\n", g_softs[i].name, g_softs[i].path);
    }
    fclose(f);
}

// ==============================================
// 8. 根据名称查找路径
// ==============================================
const WCHAR* FindExePath(const WCHAR* name)
{
    for (int i = 0; i < g_count; i++)
    {
        if (_wcsicmp(g_softs[i].name, name) == 0)
            return g_softs[i].path;
    }
    return NULL;
}

// ==============================================
// 9. 添加/覆盖记录
// ==============================================
void AddRecord(const WCHAR* name, const WCHAR* path)
{
    for (int i = 0; i < g_count; i++)
    {
        if (_wcsicmp(g_softs[i].name, name) == 0)
        {
            wcsncpy_s(g_softs[i].path, _countof(g_softs[i].path), path, _TRUNCATE);
            SaveConfig();
            return;
        }
    }

    if (g_count >= MAX_RECORDS) return;
    SoftRecord* r = &g_softs[g_count++];
    wcsncpy_s(r->name, _countof(r->name), name, _TRUNCATE);
    wcsncpy_s(r->path, _countof(r->path), path, _TRUNCATE);
    SaveConfig();
}

// ==============================================
// 10. 删除记录
// ==============================================
BOOL RemoveRecord(const WCHAR* name)
{
    for (int i = 0; i < g_count; i++)
    {
        if (_wcsicmp(g_softs[i].name, name) == 0)
        {
            for (int j = i; j < g_count - 1; j++)
                g_softs[j] = g_softs[j + 1];
            g_count--;
            SaveConfig();
            return TRUE;
        }
    }
    return FALSE;
}

// ==============================================
// 11. 列出所有记录
// ==============================================
void ListRecords()
{
    wprintf(L"\n==== 已记录软件/网址 ====\n");
    for (int i = 0; i < g_count; i++)
    {
        wprintf(L"[%d] %-12s -> %s\n", i + 1, g_softs[i].name, g_softs[i].path);
    }
    wprintf(L"==========================\n");
}

// ==============================================
// 12. 打开所在文件夹 或 直接打开网址
// ==============================================
void OpenTarget(const WCHAR* path)
{
    // 如果是网址，直接打开
    if (wcsstr(path, L"http://") || wcsstr(path, L"https://"))
    {
        ShellExecuteW(0, L"open", path, 0, 0, SW_SHOWNORMAL);
        wprintf(L"已打开网址：%s\n", path);
        return;
    }

    // 如果是文件，打开文件夹
    WCHAR folder[MAX_PATH];
    wcsncpy_s(folder, MAX_PATH, path, _TRUNCATE);
    WCHAR* last = wcsrchr(folder, L'\\');
    if (last) *last = 0;

    ShellExecuteW(0, L"open", folder, 0, 0, SW_SHOWNORMAL);
    wprintf(L"已打开目录：%s\n", folder);
}

// ==============================================
// 主函数
// ==============================================
int wmain(int argc, WCHAR** argv)
{
    SetConsoleOutputCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_U16TEXT);

    LoadConfig();

    if (argc < 2)
    {
        wprintf(L"用法：\n");
        wprintf(L"  sl -a <xxx.lnk/xxx.url> [name]    添加快捷方式\n");
        wprintf(L"  sl -l                            列表\n");
        wprintf(L"  sl -s <name>                     打开目录/网址\n");
        wprintf(L"  sl -r <name>                     删除\n");
        return 0;
    }

    const WCHAR* opt = argv[1];

    // 列表
    if (_wcsicmp(opt, L"-l") == 0)
    {
        ListRecords();
        return 0;
    }

    // 打开目录/网址
    if (_wcsicmp(opt, L"-s") == 0 && argc >= 3)
    {
        const WCHAR* path = FindExePath(argv[2]);
        if (path) OpenTarget(path);
        else wprintf(L"未找到：%s\n", argv[2]);
        return 0;
    }

    // 删除
    if (_wcsicmp(opt, L"-r") == 0 && argc >= 3)
    {
        if (RemoveRecord(argv[2]))
            wprintf(L"已删除：%s\n", argv[2]);
        else
            wprintf(L"不存在：%s\n", argv[2]);
        return 0;
    }

    // 添加：支持 lnk + url
    if (_wcsicmp(opt, L"-a") == 0 && argc >= 3)
    {
        WCHAR target[MAX_PATH] = { 0 };
        if (!GetTargetPath(argv[2], target, MAX_PATH))
        {
            wprintf(L"解析快捷方式失败！\n");
            return 1;
        }

        WCHAR name[256];
        if (argc >= 4)
            wcsncpy_s(name, _countof(name), argv[3], _TRUNCATE);
        else
            GetFileNameFromPath(argv[2], name, _countof(name));

        // 调用 l2e.exe 生成 exe
        WCHAR cmd[4096];
        swprintf_s(cmd, _countof(cmd), L"l2e.exe \"%s\" %s", argv[2], name);
        _wsystem(cmd);

        AddRecord(name, target);
        wprintf(L"已添加：%s -> %s\n", name, target);
        return 0;
    }

    wprintf(L"无效参数\n");
    return 1;
}