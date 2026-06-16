// tui.c - TUI interface for sl
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <wchar.h>
#include <io.h>
#include <fcntl.h>
#include <conio.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

#define MAX_RECORDS 1024
#define MAX_LINE 4096

// 颜色定义
#define COLOR_TITLE     FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY
#define COL_MENU        FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE
#define COLOR_SELECTED  FOREGROUND_GREEN | FOREGROUND_INTENSITY
#define COLOR_INFO      FOREGROUND_RED | FOREGROUND_GREEN
#define COLOR_ERROR     FOREGROUND_RED | FOREGROUND_INTENSITY
#define COLOR_HELP      FOREGROUND_GREEN | FOREGROUND_BLUE

typedef struct {
    WCHAR name[256];
    WCHAR path[4096];
} SoftRecord;

SoftRecord g_softs[MAX_RECORDS];
int g_count = 0;

// 设置控制台颜色
void SetColor(WORD color) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, color);
}

// 清屏
void ClearScreen() {
    system("cls");
}

// 移动光标到指定位置
void MoveCursor(int x, int y) {
    COORD coord = { (SHORT)x, (SHORT)y };
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

// 隐藏光标
void HideCursor() {
    CONSOLE_CURSOR_INFO cursorInfo;
    cursorInfo.dwSize = 1;
    cursorInfo.bVisible = FALSE;
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursorInfo);
}

// 显示光标
void ShowConsoleCursor() {
    CONSOLE_CURSOR_INFO cursorInfo;
    cursorInfo.dwSize = 1;
    cursorInfo.bVisible = TRUE;
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursorInfo);
}

// 解析 URL 快捷方式
BOOL GetUrlTargetPath(const WCHAR* urlPath, WCHAR* targetPath, int maxTarget) {
    return GetPrivateProfileStringW(L"InternetShortcut", L"URL", L"",
        targetPath, maxTarget, urlPath) > 0;
}

// 解析 LNK 快捷方式
BOOL GetLnkTargetPath(const WCHAR* lnkPath, WCHAR* targetPath, int maxTarget) {
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

// 统一接口
BOOL GetTargetPath(const WCHAR* filePath, WCHAR* targetPath, int maxTarget) {
    targetPath[0] = 0;
    if (GetUrlTargetPath(filePath, targetPath, maxTarget))
        return TRUE;
    if (GetLnkTargetPath(filePath, targetPath, maxTarget))
        return TRUE;
    return FALSE;
}

// 从路径提取文件名
void GetFileNameFromPath(const WCHAR* path, WCHAR* name, int maxName) {
    const WCHAR* p = wcsrchr(path, L'\\');
    if (!p) p = path;
    else p++;

    wcsncpy_s(name, maxName, p, _TRUNCATE);

    WCHAR* dot = wcsrchr(name, L'.');
    if (dot) *dot = 0;
}

// 获取配置文件路径
void GetConfigPath(WCHAR* cfgPath, int maxLen) {
    WCHAR exePath[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    WCHAR* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) *lastSlash = 0;

    swprintf_s(cfgPath, maxLen, L"%s\\sl_config.txt", exePath);
}

// 加载配置
void LoadConfig() {
    WCHAR cfgPath[MAX_PATH];
    GetConfigPath(cfgPath, MAX_PATH);

    g_count = 0;
    FILE* f = _wfopen(cfgPath, L"r, ccs=UTF-8");
    if (!f) return;

    WCHAR line[MAX_LINE];
    while (fgetws(line, MAX_LINE, f)) {
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

// 保存配置
void SaveConfig() {
    WCHAR cfgPath[MAX_PATH];
    GetConfigPath(cfgPath, MAX_PATH);

    FILE* f = _wfopen(cfgPath, L"w, ccs=UTF-8");
    if (!f) return;

    for (int i = 0; i < g_count; i++) {
        fwprintf(f, L"%s|%s\n", g_softs[i].name, g_softs[i].path);
    }
    fclose(f);
}

// 查找路径
const WCHAR* FindExePath(const WCHAR* name) {
    for (int i = 0; i < g_count; i++) {
        if (_wcsicmp(g_softs[i].name, name) == 0)
            return g_softs[i].path;
    }
    return NULL;
}

// 添加记录
void AddRecord(const WCHAR* name, const WCHAR* path) {
    for (int i = 0; i < g_count; i++) {
        if (_wcsicmp(g_softs[i].name, name) == 0) {
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

// 删除记录
BOOL RemoveRecord(const WCHAR* name) {
    for (int i = 0; i < g_count; i++) {
        if (_wcsicmp(g_softs[i].name, name) == 0) {
            for (int j = i; j < g_count - 1; j++)
                g_softs[j] = g_softs[j + 1];
            g_count--;
            SaveConfig();
            return TRUE;
        }
    }
    return FALSE;
}

// 打开目标
void OpenTarget(const WCHAR* path) {
    if (wcsstr(path, L"http://") || wcsstr(path, L"https://")) {
        ShellExecuteW(0, L"open", path, 0, 0, SW_SHOWNORMAL);
        return;
    }

    WCHAR folder[MAX_PATH];
    wcsncpy_s(folder, MAX_PATH, path, _TRUNCATE);
    WCHAR* last = wcsrchr(folder, L'\\');
    if (last) *last = 0;

    ShellExecuteW(0, L"open", folder, 0, 0, SW_SHOWNORMAL);
}

// 显示标题
void ShowTitle() {
    ClearScreen();
    SetColor(COLOR_TITLE);
    wprintf(L"\n");
    wprintf(L"    ╔════════════════════════════════════════════════╗\n");
    wprintf(L"    ║                                                ║\n");
    wprintf(L"    ║          快捷方式管理工具 (SL TUI)             ║\n");
    wprintf(L"    ║                                                ║\n");
    wprintf(L"    ╚════════════════════════════════════════════════╝\n");
    wprintf(L"\n");
    SetColor(COL_MENU);
}

// 显示主菜单
int ShowMainMenu() {
    ShowTitle();
    
    const WCHAR* menuItems[] = {
        L"📋 查看所有快捷方式",
        L"➕ 添加新的快捷方式",
        L"🚀 打开快捷方式位置",
        L"❌ 删除快捷方式",
        L"🚪 退出程序"
    };
    
    int selected = 0;
    int itemCount = 5;
    
    while (1) {
        // 显示菜单
        for (int i = 0; i < itemCount; i++) {
            MoveCursor(10, 8 + i);
            if (i == selected) {
                SetColor(COLOR_SELECTED);
                wprintf(L"▶ %s", menuItems[i]);
            } else {
                SetColor(COL_MENU);
                wprintf(L"  %s", menuItems[i]);
            }
        }
        
        // 显示帮助
        MoveCursor(10, 15);
        SetColor(COLOR_HELP);
        wprintf(L"使用 ↑↓ 方向键选择，按 Enter 确认");
        
        // 读取按键
        int key = _getwch();
        if (key == 224 || key == 0) {  // 方向键
            key = _getwch();
            if (key == 72) {  // 上
                selected = (selected - 1 + itemCount) % itemCount;
            } else if (key == 80) {  // 下
                selected = (selected + 1) % itemCount;
            }
        } else if (key == 13) {  // Enter
            return selected;
        } else if (key == 27) {  // Esc
            return 4;  // 退出
        }
    }
}

// 显示列表
void ShowList() {
    ClearScreen();
    SetColor(COLOR_TITLE);
    wprintf(L"\n    ════════════════════════════════════════════════\n");
    wprintf(L"              📋 已记录的快捷方式列表\n");
    wprintf(L"    ════════════════════════════════════════════════\n\n");
    
    if (g_count == 0) {
        SetColor(COLOR_INFO);
        wprintf(L"    （暂无记录，请先添加快捷方式）\n\n");
    } else {
        SetColor(COL_MENU);
        for (int i = 0; i < g_count; i++) {
            wprintf(L"    [%2d] %-20s → %s\n", i + 1, g_softs[i].name, g_softs[i].path);
        }
        wprintf(L"\n");
    }
    
    SetColor(COLOR_HELP);
    wprintf(L"    按任意键返回主菜单...");
    _getwch();
}

// 添加快捷方式
void AddShortcut() {
    ClearScreen();
    SetColor(COLOR_TITLE);
    wprintf(L"\n    ════════════════════════════════════════════════\n");
    wprintf(L"              ➕ 添加新的快捷方式\n");
    wprintf(L"    ════════════════════════════════════════════════\n\n");
    
    SetColor(COLOR_INFO);
    wprintf(L"    请输入快捷方式文件路径（.lnk 或 .url）：\n");
    wprintf(L"    提示：可以直接把文件拖到窗口中\n\n");
    
    SetColor(COL_MENU);
    wprintf(L"    路径: ");
    
    ShowConsoleCursor();
    WCHAR filePath[4096];
    fgetws(filePath, 4096, stdin);
    
    // 去掉换行符
    WCHAR* nl = wcschr(filePath, L'\n');
    if (nl) *nl = 0;
    
    // 去掉可能的引号
    if (filePath[0] == L'"') {
        memmove(filePath, filePath + 1, wcslen(filePath));
        WCHAR* quote = wcsrchr(filePath, L'"');
        if (quote) *quote = 0;
    }
    
    WCHAR target[MAX_PATH] = { 0 };
    if (!GetTargetPath(filePath, target, MAX_PATH)) {
        SetColor(COLOR_ERROR);
        wprintf(L"\n    ❌ 解析快捷方式失败！请检查文件路径。\n");
        SetColor(COLOR_HELP);
        wprintf(L"    按任意键返回...");
        HideCursor();
        _getwch();
        return;
    }
    
    // 询问名称
    SetColor(COLOR_INFO);
    wprintf(L"\n    请输入名称（直接回车使用默认名称）：");
    
    WCHAR name[256];
    fgetws(name, 256, stdin);
    nl = wcschr(name, L'\n');
    if (nl) *nl = 0;
    
    if (wcslen(name) == 0) {
        GetFileNameFromPath(filePath, name, 256);
    }
    
    AddRecord(name, target);
    
    SetColor(COLOR_SELECTED);
    wprintf(L"\n    ✅ 已添加：%s\n", name);
    wprintf(L"       → %s\n", target);
    
    SetColor(COLOR_HELP);
    wprintf(L"\n    按任意键返回...");
    HideCursor();
    _getwch();
}

// 打开快捷方式
void OpenShortcut() {
    if (g_count == 0) {
        ClearScreen();
        SetColor(COLOR_ERROR);
        wprintf(L"\n    ❌ 暂无记录，请先添加快捷方式。\n");
        SetColor(COLOR_HELP);
        wprintf(L"    按任意键返回...");
        _getwch();
        return;
    }
    
    ClearScreen();
    SetColor(COLOR_TITLE);
    wprintf(L"\n    ════════════════════════════════════════════════\n");
    wprintf(L"              🚀 打开快捷方式位置\n");
    wprintf(L"    ════════════════════════════════════════════════\n\n");
    
    SetColor(COLOR_INFO);
    wprintf(L"    请选择要打开的快捷方式（输入序号）：\n\n");
    
    SetColor(COL_MENU);
    for (int i = 0; i < g_count; i++) {
        wprintf(L"    [%2d] %s\n", i + 1, g_softs[i].name);
    }
    
    wprintf(L"\n    序号 (0 取消): ");
    
    ShowConsoleCursor();
    int choice;
    wscanf(L"%d", &choice);
    while (getwchar() != L'\n');  // 清空输入缓冲区
    
    if (choice <= 0 || choice > g_count) {
        HideCursor();
        return;
    }
    
    const WCHAR* path = g_softs[choice - 1].path;
    OpenTarget(path);
    
    SetColor(COLOR_SELECTED);
    wprintf(L"\n    ✅ 已打开：%s\n", g_softs[choice - 1].name);
    
    SetColor(COLOR_HELP);
    wprintf(L"    按任意键返回...");
    HideCursor();
    _getwch();
}

// 删除快捷方式
void DeleteShortcut() {
    if (g_count == 0) {
        ClearScreen();
        SetColor(COLOR_ERROR);
        wprintf(L"\n    ❌ 暂无记录，请先添加快捷方式。\n");
        SetColor(COLOR_HELP);
        wprintf(L"    按任意键返回...");
        _getwch();
        return;
    }
    
    ClearScreen();
    SetColor(COLOR_TITLE);
    wprintf(L"\n    ════════════════════════════════════════════════\n");
    wprintf(L"              ❌ 删除快捷方式\n");
    wprintf(L"    ════════════════════════════════════════════════\n\n");
    
    SetColor(COLOR_INFO);
    wprintf(L"    请选择要删除的快捷方式（输入序号）：\n\n");
    
    SetColor(COL_MENU);
    for (int i = 0; i < g_count; i++) {
        wprintf(L"    [%2d] %s → %s\n", i + 1, g_softs[i].name, g_softs[i].path);
    }
    
    wprintf(L"\n    序号 (0 取消): ");
    
    ShowConsoleCursor();
    int choice;
    wscanf(L"%d", &choice);
    while (getwchar() != L'\n');
    
    if (choice <= 0 || choice > g_count) {
        HideCursor();
        return;
    }
    
    const WCHAR* name = g_softs[choice - 1].name;
    
    // 确认删除
    SetColor(COLOR_INFO);
    wprintf(L"\n    确定要删除 \"%s\" 吗？(y/n): ", name);
    
    WCHAR confirm;
    wscanf(L"%c", &confirm);
    
    if (confirm == L'y' || confirm == L'Y') {
        if (RemoveRecord(name)) {
            SetColor(COLOR_SELECTED);
            wprintf(L"\n    ✅ 已删除：%s\n", name);
        } else {
            SetColor(COLOR_ERROR);
            wprintf(L"\n    ❌ 删除失败\n");
        }
    } else {
        SetColor(COLOR_INFO);
        wprintf(L"\n    已取消删除\n");
    }
    
    SetColor(COLOR_HELP);
    wprintf(L"    按任意键返回...");
    HideCursor();
    _getwch();
}

// 主函数
int wmain(int argc, WCHAR** argv) {
    SetConsoleOutputCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_U16TEXT);
    
    HideCursor();
    LoadConfig();
    
    while (1) {
        int choice = ShowMainMenu();
        
        switch (choice) {
            case 0:  // 查看列表
                ShowList();
                break;
            case 1:  // 添加
                AddShortcut();
                break;
            case 2:  // 打开
                OpenShortcut();
                break;
            case 3:  // 删除
                DeleteShortcut();
                break;
            case 4:  // 退出
                ClearScreen();
                SetColor(COLOR_TITLE);
                wprintf(L"\n    👋 感谢使用，再见！\n\n");
                ShowConsoleCursor();
                return 0;
        }
    }
}
