#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <stdio.h>      
#include <wchar.h>      
#include <io.h>         
#include <fcntl.h>      
#include <shlobj.h>     // 解析LNK需要

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib") // CoInitialize需要

// ========== 前置声明 ==========
std::vector<std::pair<std::wstring, std::wstring>> read_all_soft();

// ========== 工具函数 ==========
// 1. 正确提取LNK快捷方式的真实EXE目标路径
std::wstring get_lnk_target_path(const std::wstring& lnk_path) {
    WCHAR szTargetPath[MAX_PATH * 2] = { 0 };
    IShellLinkW* pShellLink = NULL;
    IPersistFile* pPersistFile = NULL;
    HRESULT hr = CoInitialize(NULL);

    if (FAILED(hr)) {
        wprintf(L"[ERROR] COM库初始化失败！\n");
        return L"";
    }

    hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
        IID_IShellLinkW, (LPVOID*)&pShellLink);
    if (FAILED(hr)) {
        wprintf(L"[ERROR] 创建ShellLink实例失败！错误码：%08X\n", hr);
        CoUninitialize();
        return L"";
    }

    hr = pShellLink->QueryInterface(IID_IPersistFile, (LPVOID*)&pPersistFile);
    if (FAILED(hr)) {
        wprintf(L"[ERROR] 获取PersistFile接口失败！错误码：%08X\n", hr);
        pShellLink->Release();
        CoUninitialize();
        return L"";
    }

    hr = pPersistFile->Load(lnk_path.c_str(), STGM_READ);
    if (FAILED(hr)) {
        wprintf(L"[ERROR] 加载LNK文件失败！请检查路径是否正确，错误码：%08X\n", hr);
        pPersistFile->Release();
        pShellLink->Release();
        CoUninitialize();
        return L"";
    }

    hr = pShellLink->GetPath(szTargetPath, MAX_PATH * 2, NULL, SLGP_RAWPATH);
    if (FAILED(hr)) {
        wprintf(L"[ERROR] 提取LNK目标路径失败！错误码：%08X\n", hr);
        pPersistFile->Release();
        pShellLink->Release();
        CoUninitialize();
        return L"";
    }

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
            return pair.second;
        }
    }
    return L"";
}

// 5. 只打开路径所在文件夹（不运行EXE）
bool open_path(const std::wstring& path) {
    if (path.empty()) {
        wprintf(L"[ERROR] 路径为空，无法打开文件夹！\n");
        return false;
    }

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

// ========== 配置文件核心（仅读写exe同目录） ==========
// 配置文件路径：强制读取sl.exe同目录下的sl_config.txt
std::wstring get_config_path() {
    // 1. 获取sl.exe自身的绝对路径
    WCHAR szExePath[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, szExePath, MAX_PATH);

    // 2. 截取exe所在目录（去掉exe文件名）
    std::wstring exe_full_path = szExePath;
    std::wstring exe_dir = exe_full_path.substr(0, exe_full_path.find_last_of(L"\\/"));

    // 3. 拼接同目录下的sl_config.txt
    std::wstring cfg_path = exe_dir + L"\\sl_config.txt";
    // 打印路径（方便你验证）
    wprintf(L"[INFO] 配置文件路径：%ls\n", cfg_path.c_str());
    return cfg_path;
}

// 检查并创建配置文件（仅在exe同目录）
bool check_config() {
    std::wstring cfg = get_config_path();
    if (_waccess(cfg.c_str(), 0) == 0) {
        return true;
    }
    // 新建空文件
    std::wofstream f(cfg);
    f.close();
    wprintf(L"[INFO] 已在exe同目录创建配置文件：%ls\n", cfg.c_str());
    return true;
}

// 读取所有记录（仅从exe同目录读取）
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

// 写入所有记录（仅写入exe同目录）
bool write_all_soft(const std::vector<std::pair<std::wstring, std::wstring>>& list) {
    std::wstring cfg = get_config_path();
    std::wofstream out(cfg, std::ios::trunc);
    if (!out.is_open()) {
        wprintf(L"[❌] 写入配置文件失败！请检查exe目录权限\n");
        return false;
    }
    for (auto& pair : list) {
        out << pair.first << L"|" << pair.second << L"\n";
    }
    out.close();
    return true;
}

// 添加记录
bool add_soft(const std::wstring& name, const std::wstring& exe_path) {
    if (name.empty() || exe_path.empty()) {
        wprintf(L"[ERROR] 文件名或EXE路径为空，添加失败！\n");
        return false;
    }

    std::vector<std::pair<std::wstring, std::wstring>> list = read_all_soft();
    std::vector<std::pair<std::wstring, std::wstring>> new_list;
    for (auto& pair : list) {
        if (pair.first != name) {
            new_list.push_back(pair);
        }
    }
    new_list.push_back({ name, exe_path });

    if (write_all_soft(new_list)) {
        wprintf(L"[✅] 已添加记录：%ls → %ls\n", name.c_str(), exe_path.c_str());
        return true;
    }
    return false;
}

// 删除记录（-r 命令）
bool remove_soft(const std::wstring& name) {
    if (name.empty()) {
        wprintf(L"[ERROR] 请指定要删除的文件名！\n");
        return false;
    }

    std::vector<std::pair<std::wstring, std::wstring>> list = read_all_soft();
    std::vector<std::pair<std::wstring, std::wstring>> new_list;
    bool found = false;

    for (auto& pair : list) {
        if (pair.first == name) {
            found = true;
            continue;
        }
        new_list.push_back(pair);
    }

    if (!found) {
        wprintf(L"[❌] 删除失败！未找到文件名「%ls」的记录\n", name.c_str());
        return false;
    }

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
    SetConsoleOutputCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_U16TEXT);

    // 检查并创建exe同目录的配置文件
    check_config();

    if (argc < 2) {
        wprintf(L"[ERROR] 缺少命令参数！\n");
        wprintf(L"支持的命令：\n");
        wprintf(L"  1. 添加记录：sl -a <lnk快捷方式路径> [自定义文件名]\n");
        wprintf(L"  2. 查看列表：sl -l\n");
        wprintf(L"  3. 打开文件夹：sl -s <要打开的文件名>\n");
        wprintf(L"  4. 删除记录：sl -r <要删除的文件名>\n");
        return -1;
    }

    std::wstring opt = argv[1];

    // -l 查看列表
    if (opt == L"-l") {
        list_soft_table();
        return 0;
    }

    // -s 打开文件夹
    if (opt == L"-s" && argc > 2) {
        std::wstring open_name = argv[2];
        wprintf(L"[INFO] 正在查找文件名「%ls」对应的路径...\n", open_name.c_str());

        std::wstring exe_path = find_exe_path_by_name(open_name);
        if (exe_path.empty()) {
            wprintf(L"[❌] 未找到文件名「%ls」的记录！\n", open_name.c_str());
            return -1;
        }

        open_path(exe_path);
        return 0;
    }

    // -r 删除记录
    if (opt == L"-r" && argc > 2) {
        std::wstring del_name = argv[2];
        remove_soft(del_name);
        return 0;
    }

    // -a 添加记录
    if (opt == L"-a" && argc > 2) {
        std::wstring lnk_path = argv[2];
        wprintf(L"[INFO] 正在解析LNK文件：%ls\n", lnk_path.c_str());

        std::wstring exe_path = get_lnk_target_path(lnk_path);
        if (exe_path.empty()) {
            wprintf(L"[ERROR] 解析LNK失败！请检查LNK路径是否正确\n");
            return -1;
        }
        wprintf(L"[INFO] 解析成功，EXE真实路径：%ls\n", exe_path.c_str());

        std::wstring soft_name;
        if (argc > 3) {
            soft_name = argv[3];
        }
        else {
            soft_name = get_file_name(exe_path);
            wprintf(L"[INFO] 自动提取文件名：%ls\n", soft_name.c_str());
        }

        std::wstring cmd = L"l2e.exe \"" + lnk_path + L"\" " + soft_name;
        wprintf(L"[INFO] 执行命令：%ls\n", cmd.c_str());
        _wsystem(cmd.c_str());

        add_soft(soft_name, exe_path);
        return 0;
    }

    // 无效参数
    wprintf(L"[ERROR] 无效命令！\n");
    wprintf(L"支持的命令：\n");
    wprintf(L"  1. 添加记录：sl -a <lnk快捷方式路径> [自定义文件名]\n");
    wprintf(L"  2. 查看列表：sl -l\n");
    wprintf(L"  3. 打开文件夹：sl -s <要打开的文件名>\n");
    wprintf(L"  4. 删除记录：sl -r <要删除的文件名>\n");
    return -1;
}