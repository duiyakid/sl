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
#pragma comment(lib, "ole32.lib") // CoInitialize需要

// ========== 工具函数 ==========
// 1. 修复：正确提取LNK快捷方式的真实EXE目标路径
std::wstring get_lnk_target_path(const std::wstring& lnk_path) {
    WCHAR szTargetPath[MAX_PATH * 2] = { 0 }; // 扩大缓冲区，避免路径截断
    IShellLinkW* pShellLink = NULL;
    IPersistFile* pPersistFile = NULL;
    HRESULT hr = CoInitialize(NULL); // 初始化COM库（必须）

    if (FAILED(hr)) {
        wprintf(L"[ERROR] COM库初始化失败！\n");
        return L"";
    }

    // 1. 创建IShellLink实例
    hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
        IID_IShellLinkW, (LPVOID*)&pShellLink);
    if (FAILED(hr)) {
        wprintf(L"[ERROR] 创建ShellLink实例失败！错误码：%08X\n", hr);
        CoUninitialize();
        return L"";
    }

    // 2. 查询IPersistFile接口（用于加载LNK文件）
    hr = pShellLink->QueryInterface(IID_IPersistFile, (LPVOID*)&pPersistFile);
    if (FAILED(hr)) {
        wprintf(L"[ERROR] 获取PersistFile接口失败！错误码：%08X\n", hr);
        pShellLink->Release();
        CoUninitialize();
        return L"";
    }

    // 3. 加载LNK文件（必须用WCHAR，支持中文路径）
    hr = pPersistFile->Load(lnk_path.c_str(), STGM_READ);
    if (FAILED(hr)) {
        wprintf(L"[ERROR] 加载LNK文件失败！请检查路径是否正确，错误码：%08X\n", hr);
        pPersistFile->Release();
        pShellLink->Release();
        CoUninitialize();
        return L"";
    }

    // 4. 修复核心：获取完整的目标路径（SLGP_RAWPATH=原始路径，不简化）
    hr = pShellLink->GetPath(szTargetPath, MAX_PATH * 2, NULL, SLGP_RAWPATH);
    if (FAILED(hr)) {
        wprintf(L"[ERROR] 提取LNK目标路径失败！错误码：%08X\n", hr);
        pPersistFile->Release();
        pShellLink->Release();
        CoUninitialize();
        return L"";
    }

    // 释放资源
    pPersistFile->Release();
    pShellLink->Release();
    CoUninitialize();

    // 返回真实的EXE路径
    return szTargetPath;
}

// 2. 提取文件名（从EXE路径/自定义名称）
std::wstring get_file_name(const std::wstring& path) {
    if (path.empty()) return L"";
    std::wstring new_name = path.substr(path.find_last_of(L"\\/") + 1);
    size_t dot_pos = new_name.rfind(L'.');
    if (dot_pos != std::wstring::npos)
        new_name = new_name.substr(0, dot_pos);
    return new_name;
}

// 3. 补空格对齐（表格美观）
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
    if (name.empty() || exe_path.empty()) {
        wprintf(L"[ERROR] 文件名或EXE路径为空，写入失败！\n");
        return false;
    }

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
    // 表头：序号  文件名          EXE路径
    wprintf(L"%ls\t%ls\t%ls\n",
        pad_space(L"序号", 2).c_str(),       // 序号列（占2位）
        pad_space(L"文件名", 10).c_str(),    // 文件名列（占10位）
        L"EXE路径");                         // EXE路径列

    if (list.empty()) {
        wprintf(L"%ls\t%ls\t%ls\n",
            pad_space(L"-", 2).c_str(),
            pad_space(L"暂无记录", 10).c_str(),
            L"-");
    }
    else {
        for (int i = 0; i < list.size(); i++) {
            // 行内容：序号  文件名（补空格）  EXE路径
            std::wstring idx = std::to_wstring(i + 1);
            wprintf(L"%ls\t%ls\t%ls\n",
                pad_space(idx, 2).c_str(),       // 序号
                pad_space(list[i].first, 10).c_str(),  // 文件名（对齐）
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
        wprintf(L"[INFO] 正在解析LNK文件：%ls\n", lnk_path.c_str());

        // 核心修复：提取真实的EXE路径
        std::wstring exe_path = get_lnk_target_path(lnk_path);
        if (exe_path.empty()) {
            wprintf(L"[ERROR] 解析LNK失败！请检查LNK路径是否正确，或该LNK是否指向有效EXE\n");
            return -1;
        }
        wprintf(L"[INFO] 解析成功，EXE真实路径：%ls\n", exe_path.c_str());

        // 确定文件名（用户自定义 > 从EXE路径提取）
        std::wstring soft_name;
        if (argc > 3) {
            soft_name = argv[3]; // 用户自定义文件名（如789）
        }
        else {
            soft_name = get_file_name(exe_path); // 从EXE路径提取（如Excel）
            wprintf(L"[INFO] 自动提取文件名：%ls\n", soft_name.c_str());
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