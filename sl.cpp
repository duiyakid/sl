#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <stdio.h>      
#include <wchar.h>      
#include <io.h>         
#include <fcntl.h>      
#include <shlobj.h>     // 解析LNK/打开路径需要

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib") // CoInitialize需要

// ========== 前置声明（解决函数调用顺序问题） ==========
std::vector<std::pair<std::wstring, std::wstring>> read_all_soft();

// ========== 工具函数 ==========
// 1. 正确提取LNK快捷方式的真实EXE目标路径
std::wstring get_lnk_target_path(const std::wstring& lnk_path) {
    WCHAR szTargetPath[MAX_PATH * 2] = { 0 }; // 扩大缓冲区，避免路径截断
    IShellLinkW* pShellLink = NULL;
    IPersistFile* pPersistFile = NULL;
    HRESULT hr = CoInitialize(NULL); // 初始化COM库（必须）

    if (FAILED(hr)) {
        wprintf(L"[ERROR] COM库初始化失败！\n");
        return L"";
    }

    // 创建IShellLink实例
    hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
        IID_IShellLinkW, (LPVOID*)&pShellLink);
    if (FAILED(hr)) {
        wprintf(L"[ERROR] 创建ShellLink实例失败！错误码：%08X\n", hr);
        CoUninitialize();
        return L"";
    }

    // 查询IPersistFile接口
    hr = pShellLink->QueryInterface(IID_IPersistFile, (LPVOID*)&pPersistFile);
    if (FAILED(hr)) {
        wprintf(L"[ERROR] 获取PersistFile接口失败！错误码：%08X\n", hr);
        pShellLink->Release();
        CoUninitialize();
        return L"";
    }

    // 加载LNK文件
    hr = pPersistFile->Load(lnk_path.c_str(), STGM_READ);
    if (FAILED(hr)) {
        wprintf(L"[ERROR] 加载LNK文件失败！请检查路径是否正确，错误码：%08X\n", hr);
        pPersistFile->Release();
        pShellLink->Release();
        CoUninitialize();
        return L"";
    }

    // 获取完整的目标路径（SLGP_RAWPATH=原始路径）
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

// 4. 根据文件名查找对应的EXE路径
std::wstring find_exe_path_by_name(const std::wstring& name) {
    if (name.empty()) return L"";
    std::vector<std::pair<std::wstring, std::wstring>> list = read_all_soft();
    for (auto& pair : list) {
        if (pair.first == name) {
            return pair.second; // 找到匹配的EXE路径
        }
    }
    return L""; // 未找到
}

// 5. 打开指定路径（启动EXE或打开文件夹）- 修复类型转换警告
// 5. 只打开路径所在文件夹（不运行EXE）
bool open_path(const std::wstring& path) {
    if (path.empty()) {
        wprintf(L"[ERROR] 路径为空，无法打开文件夹！\n");
        return false;
    }

    // 截取到所在文件夹
    size_t last_slash = path.find_last_of(L"\\/");
    if (last_slash == std::wstring::npos) {
        wprintf(L"[ERROR] 无法解析所在文件夹：%ls\n", path.c_str());
        return false;
    }

    std::wstring folder = path.substr(0, last_slash);
    HINSTANCE hInst = ShellExecuteW(NULL, L"open", folder.c_str(), NULL, NULL, SW_SHOWNORMAL);

    if ((UINT)hInst > 32) {
        wprintf(L"[✅] 已打开所在文件夹：%ls\n", folder.c_str());
        return true;
    }
    else {
        wprintf(L"[❌] 打开文件夹失败：%ls\n", folder.c_str());
        return false;
    }
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
        return true;
    }
    // 新建空文件
    std::wofstream f(cfg);
    f.close();
    return true;
}

// 读取所有记录（供添加/删除/查看/查找使用）
std::vector<std::pair<std::wstring, std::wstring>> read_all_soft() {
    std::vector<std::pair<std::wstring, std::wstring>> list;
    std::wstring cfg = get_config_path();

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
    return list;
}

// 写入所有记录（覆盖式写入）
bool write_all_soft(const std::vector<std::pair<std::wstring, std::wstring>>& list) {
    std::wstring cfg = get_config_path();
    std::wofstream out(cfg, std::ios::trunc);
    if (!out.is_open()) {
        wprintf(L"[❌] 写入配置文件失败！请以管理员身份运行\n");
        return false;
    }
    for (auto& pair : list) {
        out << pair.first << L"|" << pair.second << L"\n";
    }
    out.close();
    return true;
}

// 添加记录：存储「文件名|EXE目标路径」
bool add_soft(const std::wstring& name, const std::wstring& exe_path) {
    if (name.empty() || exe_path.empty()) {
        wprintf(L"[ERROR] 文件名或EXE路径为空，添加失败！\n");
        return false;
    }

    // 读取原有记录（去重）
    std::vector<std::pair<std::wstring, std::wstring>> list = read_all_soft();
    std::vector<std::pair<std::wstring, std::wstring>> new_list;
    // 保留非同名记录
    for (auto& pair : list) {
        if (pair.first != name) {
            new_list.push_back(pair);
        }
    }
    // 添加新记录
    new_list.push_back({ name, exe_path });

    // 写入文件
    if (write_all_soft(new_list)) {
        wprintf(L"[✅] 已添加记录：%ls → %ls\n", name.c_str(), exe_path.c_str());
        return true;
    }
    return false;
}

// 删除记录：按文件名删除（-r 命令）
bool remove_soft(const std::wstring& name) {
    if (name.empty()) {
        wprintf(L"[ERROR] 请指定要删除的文件名！\n");
        return false;
    }

    // 读取原有记录
    std::vector<std::pair<std::wstring, std::wstring>> list = read_all_soft();
    std::vector<std::pair<std::wstring, std::wstring>> new_list;
    bool found = false;

    // 过滤要删除的记录
    for (auto& pair : list) {
        if (pair.first == name) {
            found = true; // 标记找到该记录
            continue;     // 跳过（即删除）
        }
        new_list.push_back(pair);
    }

    if (!found) {
        wprintf(L"[❌] 删除失败！未找到文件名「%ls」的记录\n", name.c_str());
        return false;
    }

    // 写入过滤后的记录
    if (write_all_soft(new_list)) {
        wprintf(L"[✅] 已成功删除文件名「%ls」的记录\n", name.c_str());
        return true;
    }
    return false;
}

// 表格化展示所有记录（-l 命令）
void list_soft_table() {
    std::vector<std::pair<std::wstring, std::wstring>> list = read_all_soft();

    wprintf(L"\n==================== 已记录的软件列表 ====================\n");
    // 表头：序号  文件名          EXE路径
    wprintf(L"%ls\t%ls\t%ls\n",
        pad_space(L"序号", 2).c_str(),
        pad_space(L"文件名", 10).c_str(),
        L"EXE路径");

    if (list.empty()) {
        wprintf(L"%ls\t%ls\t%ls\n",
            pad_space(L"-", 2).c_str(),
            pad_space(L"暂无记录", 10).c_str(),
            L"-");
    }
    else {
        for (int i = 0; i < list.size(); i++) {
            std::wstring idx = std::to_wstring(i + 1);
            wprintf(L"%ls\t%ls\t%ls\n",
                pad_space(idx, 2).c_str(),
                pad_space(list[i].first, 10).c_str(),
                list[i].second.c_str());
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
        wprintf(L"支持的命令：\n");
        wprintf(L"  1. 添加记录：sl -a <lnk快捷方式路径> [自定义文件名]\n");
        wprintf(L"  2. 查看列表：sl -l\n");
        wprintf(L"  3. 打开路径：sl -s <要打开的文件名>\n");
        wprintf(L"  4. 删除记录：sl -r <要删除的文件名>\n");
        return -1;
    }

    std::wstring opt = argv[1];

    // 功能1：-l 查看软件列表（原-s功能）
    if (opt == L"-l") {
        list_soft_table();
        return 0;
    }

    // 功能2：-s 打开指定文件名的EXE路径（新增核心功能）
    if (opt == L"-s" && argc > 2) {
        std::wstring open_name = argv[2];
        wprintf(L"[INFO] 正在查找文件名「%ls」对应的路径...\n", open_name.c_str());

        // 查找EXE路径
        std::wstring exe_path = find_exe_path_by_name(open_name);
        if (exe_path.empty()) {
            wprintf(L"[❌] 未找到文件名「%ls」的记录！\n", open_name.c_str());
            return -1;
        }

        // 打开路径
        open_path(exe_path);
        return 0;
    }

    // 功能3：-r 删除指定文件名的记录（原-d功能）
    if (opt == L"-r" && argc > 2) {
        std::wstring del_name = argv[2];
        remove_soft(del_name);
        return 0;
    }

    // 功能4：-a 添加记录（保留原有功能）
    if (opt == L"-a" && argc > 2) {
        std::wstring lnk_path = argv[2];
        wprintf(L"[INFO] 正在解析LNK文件：%ls\n", lnk_path.c_str());

        // 提取真实的EXE路径
        std::wstring exe_path = get_lnk_target_path(lnk_path);
        if (exe_path.empty()) {
            wprintf(L"[ERROR] 解析LNK失败！请检查LNK路径是否正确，或该LNK是否指向有效EXE\n");
            return -1;
        }
        wprintf(L"[INFO] 解析成功，EXE真实路径：%ls\n", exe_path.c_str());

        // 确定文件名
        std::wstring soft_name;
        if (argc > 3) {
            soft_name = argv[3]; // 用户自定义文件名
        }
        else {
            soft_name = get_file_name(exe_path); // 自动提取
            wprintf(L"[INFO] 自动提取文件名：%ls\n", soft_name.c_str());
        }

        // 执行l2e.exe
        std::wstring cmd = L"l2e.exe \"" + lnk_path + L"\" " + soft_name;
        wprintf(L"[INFO] 执行命令：%ls\n", cmd.c_str());
        _wsystem(cmd.c_str());

        // 添加记录
        add_soft(soft_name, exe_path);
        return 0;
    }

    // 无效参数
    wprintf(L"[ERROR] 无效命令！\n");
    wprintf(L"支持的命令：\n");
    wprintf(L"  1. 添加记录：sl -a <lnk快捷方式路径> [自定义文件名]\n");
    wprintf(L"  2. 查看列表：sl -l\n");
    wprintf(L"  3. 打开路径：sl -s <要打开的文件名>\n");
    wprintf(L"  4. 删除记录：sl -r <要删除的文件名>\n");
    return -1;
}