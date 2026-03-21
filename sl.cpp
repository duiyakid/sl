#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <stdio.h>      
#include <wchar.h>      
#include <io.h>         
#include <fcntl.h>      
#include <shlobj.h>     // 解析LNK快捷方式需要

#pragma comment(lib, "shell32.lib")

// ========== 工具函数 ==========
// 1. 提取LNK快捷方式的真实EXE目标路径
std::wstring get_lnk_target_path(const std::wstring& lnk_path) {
    WCHAR szTargetPath[MAX_PATH] = { 0 };
    IShellLinkW* pShellLink = NULL;
    IPersistFile* pPersistFile = NULL;
    HRESULT hr = CoInitialize(NULL);

    // 创建IShellLink实例
    hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&pShellLink);
    if (SUCCEEDED(hr)) {
        // 绑定到LNK文件
        hr = pShellLink->QueryInterface(IID_IPersistFile, (void**)&pPersistFile);
        if (SUCCEEDED(hr)) {
            hr = pPersistFile->Load(lnk_path.c_str(), STGM_READ);
            if (SUCCEEDED(hr)) {
                // 获取LNK指向的真实目标路径
                pShellLink->GetPath(szTargetPath, MAX_PATH, NULL, SLGP_UNCPRIORITY);
            }
            pPersistFile->Release();
        }
        pShellLink->Release();
    }
    CoUninitialize();

    return szTargetPath;
}

// 2. 提取文件名（从EXE路径/自定义名称）
std::wstring get_file_name(const std::wstring& path) {
    std::wstring new_name = path.substr(path.find_last_of(L"\\/") + 1);
    size_t dot_pos = new_name.rfind(L'.');
    if (dot_pos != std::wstring::npos)
        new_name = new_name.substr(0, dot_pos);
    return new_name;
}

// 3. 补空格对齐（保证表格美观）
std::wstring pad_space(const std::wstring& str, int len) {
    std::wstring res = str;
    while (res.size() < len) {
        res += L" ";
    }
    return res;
}

// ========== 配置文件核心 ==========
// 配置文件路径：当前目录sl_config.txt
std::wstring get_config_path() {
    return L"sl_config.txt";
}

// 检查并创建配置文件
bool check_config() {
    std::wstring cfg = get_config_path();
    if (_waccess(cfg.c_str(), 0) == 0) {
        wprintf(L"[INFO] 配置文件已存在：%ls\n", cfg.c_str());
        return true;
    }
    // 新建空文件
    std::wofstream f(cfg);
    f.close();
    wprintf(L"[INFO] 配置文件已新建：%ls\n", cfg.c_str());
    return true;
}

// 写入记录：存储「文件名|EXE目标路径」
bool write_soft(const std::wstring& name, const std::wstring& exe_path) {
    std::wstring cfg = get_config_path();
    // 读取原有记录（去重）
    std::vector<std::pair<std::wstring, std::wstring>> list;
    std::wifstream in(cfg);
    if (in.is_open()) {
        std::wstring line;
        while (std::getline(in, line)) {
            size_t sep = line.find(L'|');
            if (sep == std::wstring::npos) continue;
            std::wstring n = line.substr(0, sep);
            std::wstring p = line.substr(sep + 1);
            if (n != name) list.push_back({ n, p }); // 去重：跳过同名
        }
        in.close();
    }
    // 添加新记录
    list.push_back({ name, exe_path });
    // 写入文件
    std::wofstream out(cfg, std::ios::trunc);
    if (!out.is_open()) {
        wprintf(L"[❌] 写入配置文件失败！请以管理员身份运行\n");
        return false;
    }
    for (auto& pair : list) {
        out << pair.first << L"|" << pair.second << L"\n";
    }
    out.close();
    wprintf(L"[✅] 已记录软件：%ls → %ls\n", name.c_str(), exe_path.c_str());
    return true;
}

// 读取记录并表格化展示（序号 | 文件名 | EXE路径）
void show_soft_table() {
    std::wstring cfg = get_config_path();
    std::vector<std::pair<std::wstring, std::wstring>> list;

    // 读取记录
    std::wifstream in(cfg);
    if (in.is_open()) {
        std::wstring line;
        while (std::getline(in, line)) {
            size_t sep = line.find(L'|');
            if (sep == std::wstring::npos) continue;
            std::wstring name = line.substr(0, sep);
            std::wstring exe_path = line.substr(sep + 1);
            list.push_back({ name, exe_path });
        }
        in.close();
    }

    // 表格化展示
    wprintf(L"\n==================== 已记录的软件列表 ====================\n");
    // 表头：序号  文件名    EXE路径（对齐）
    wprintf(L"%ls\t%ls\t%ls\n",
        pad_space(L"序号", 2).c_str(),       // 序号列（占2位）
        pad_space(L"文件名", 8).c_str(),     // 文件名列（占8位）
        L"EXE路径");                         // EXE路径列

    if (list.empty()) {
        wprintf(L"%ls\t%ls\t%ls\n",
            pad_space(L"-", 2).c_str(),
            pad_space(L"暂无记录", 8).c_str(),
            L"-");
    }
    else {
        for (int i = 0; i < list.size(); i++) {
            // 行内容：序号  文件名（补空格）  EXE路径
            std::wstring idx = std::to_wstring(i + 1);
            wprintf(L"%ls\t%ls\t%ls\n",
                pad_space(idx, 2).c_str(),       // 序号
                pad_space(list[i].first, 8).c_str(),  // 文件名（对齐）
                list[i].second.c_str());         // EXE路径
        }
    }
    wprintf(L"==========================================================\n");
    wprintf(L"总计记录数：%zd\n", list.size());
}

// ========== 主函数 ==========
int wmain(int argc, wchar_t* argv[])
{
    // 配置控制台中文输出
    SetConsoleOutputCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_U16TEXT);

    // 检查配置文件
    check_config();

    // 参数检查
    if (argc < 2) {
        wprintf(L"[ERROR] 缺少命令参数！\n");
        wprintf(L"用法：\n");
        wprintf(L"  sl -a <lnk快捷方式路径> [自定义文件名]\n");
        wprintf(L"  sl -s                      查看所有记录\n");
        return -1;
    }

    std::wstring opt = argv[1];
    // 功能1：-s 查看表格化记录
    if (opt == L"-s") {
        show_soft_table();
        return 0;
    }

    // 功能2：-a 解析LNK并记录EXE路径
    if (opt == L"-a" && argc > 2) {
        std::wstring lnk_path = argv[2];       // LNK快捷方式路径
        std::wstring exe_path = get_lnk_target_path(lnk_path); // 提取EXE真实路径

        // 确定文件名（用户自定义 > 从EXE路径提取）
        std::wstring soft_name;
        if (argc > 3) {
            soft_name = argv[3]; // 用户自定义文件名（如512）
        }
        else {
            soft_name = get_file_name(exe_path); // 从EXE路径提取（如Excel）
        }

        // 执行l2e.exe（生成exe）
        std::wstring cmd = L"l2e.exe \"" + lnk_path + L"\" " + soft_name;
        wprintf(L"[INFO] 执行命令：%ls\n", cmd.c_str());
        _wsystem(cmd.c_str());

        // 写入配置（文件名 + EXE路径）
        write_soft(soft_name, exe_path);

        return 0;
    }

    // 无效参数
    wprintf(L"[ERROR] 无效命令！支持：-a <lnk路径> [文件名] 或 -s\n");
    return -1;
}