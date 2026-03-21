#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <stdio.h>      
#include <wchar.h>      
#include <io.h>         
#include <fcntl.h>      

// 提取文件名
std::wstring get_file_name(std::wstring path) {
    std::wstring new_name = path.substr(path.find_last_of(L"\\/") + 1);
    size_t dot_pos = new_name.rfind(L'.');
    if (dot_pos != std::wstring::npos)
        new_name = new_name.substr(0, dot_pos);
    return new_name;
}

// 配置文件路径：当前目录的sl_config.txt（无任何路径陷阱）
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

// 写入记录：纯文本，格式「软件名|路径」
bool write_soft(const std::wstring& name, const std::wstring& path) {
    std::wstring cfg = get_config_path();
    // 先读原有内容，去重
    std::vector<std::pair<std::wstring, std::wstring>> list;
    std::wifstream in(cfg);
    if (in.is_open()) {
        std::wstring line;
        while (std::getline(in, line)) {
            size_t sep = line.find(L'|');
            if (sep == std::wstring::npos) continue;
            std::wstring n = line.substr(0, sep);
            std::wstring p = line.substr(sep + 1);
            if (n != name) list.push_back({ n, p });
        }
        in.close();
    }
    // 追加新记录
    list.push_back({ name, path });
    // 写入文件
    std::wofstream out(cfg, std::ios::trunc);
    if (!out.is_open()) {
        wprintf(L"[❌] 写入失败！请以管理员运行\n");
        return false;
    }
    for (auto& pair : list) {
        out << pair.first << L"|" << pair.second << L"\n";
    }
    out.close();
    wprintf(L"[✅] 写入成功：%ls → %ls\n", name.c_str(), path.c_str());
    return true;
}

// 读取记录：纯文本解析
void read_soft() {
    std::wstring cfg = get_config_path();
    std::vector<std::pair<std::wstring, std::wstring>> list;
    std::wifstream in(cfg);
    if (!in.is_open()) {
        wprintf(L"[ERROR] 读取失败！\n");
        return;
    }
    std::wstring line;
    while (std::getline(in, line)) {
        size_t sep = line.find(L'|');
        if (sep == std::wstring::npos) continue;
        std::wstring n = line.substr(0, sep);
        std::wstring p = line.substr(sep + 1);
        list.push_back({ n, p });
    }
    in.close();

    // 展示
    wprintf(L"\n==================== 软件列表 ====================\n");
    if (list.empty()) {
        wprintf(L"               暂无记录\n");
    }
    else {
        int i = 1;
        for (auto& pair : list) {
            wprintf(L"%d. 名称：%ls\n   路径：%ls\n", i++, pair.first.c_str(), pair.second.c_str());
        }
    }
    wprintf(L"==================================================\n");
    wprintf(L"总计：%zd 条\n", list.size());
}

// 主函数
int wmain(int argc, wchar_t* argv[])
{
    SetConsoleOutputCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_U16TEXT);

    check_config();

    if (argc < 2) {
        wprintf(L"用法：\n  sl -a <lnk路径> [软件名]\n  sl -s\n");
        return -1;
    }

    std::wstring opt = argv[1];
    // 查看记录
    if (opt == L"-s") {
        read_soft();
        return 0;
    }
    // 写入记录
    if (opt == L"-a" && argc > 2) {
        std::wstring path = argv[2];
        std::wstring name = (argc > 3) ? argv[3] : get_file_name(path);
        // 执行l2e.exe
        std::wstring cmd = L"l2e.exe \"" + path + L"\" " + name;
        wprintf(L"[INFO] 执行：%ls\n", cmd.c_str());
        _wsystem(cmd.c_str());
        // 写入配置
        write_soft(name, path);
        return 0;
    }

    wprintf(L"无效命令！\n");
    return -1;
}