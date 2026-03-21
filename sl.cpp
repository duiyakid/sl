#include <windows.h>
#include <iostream>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <stdio.h>      // 基础输入输出
#include <wchar.h>      // 宽字符处理
#include <io.h>         // _setmode 函数
#include <fcntl.h>      // _O_U16TEXT 宏

std::wstring get_file_name(std::wstring path) {

    std::wstring new_name = path.substr(path.find_last_of(L"\\/") + 1);
    // 第二步：找到最后一个"."的位置，删掉后面的扩展名
    size_t dot_pos = new_name.rfind(L'.');  // 找最后一个点
    if (dot_pos != std::wstring::npos)      // 如果找到点
        new_name = new_name.substr(0, dot_pos);  // 只保留点前面的部分
    return new_name;
}

// ========== 新增：获取配置文件路径 ==========
// 配置文件和sl.exe同目录，名称为sl_config.ini
std::wstring get_config_path() {
    // 直接返回当前目录的sl_config.ini（无需找exe路径）
    return L"sl_config.ini";
}

bool check_and_create_config() {
    std::wstring configPath = get_config_path();

    // 1. 检查文件是否存在（当前目录）
    DWORD fileAttr = GetFileAttributesW(configPath.c_str());
    if (fileAttr != INVALID_FILE_ATTRIBUTES) {
        wprintf(L"[INFO] 配置文件已存在（当前目录）：%ls\n", configPath.c_str());
        return true;
    }

    // 2. 文件不存在，创建空配置文件（当前目录）
    HANDLE hFile = CreateFileW(
        configPath.c_str(),          // 当前目录的sl_config.ini
        GENERIC_WRITE,               // 写入权限
        FILE_SHARE_READ,             // 允许其他程序读取
        NULL,                        // 默认安全属性
        CREATE_NEW,                  // 仅新建（避免覆盖）
        FILE_ATTRIBUTE_NORMAL,       // 普通文件
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        wprintf(L"[ERROR] 新建配置文件失败（当前目录）！错误码：%d\n", GetLastError());
        return false;
    }

    // 3. 关闭文件句柄
    CloseHandle(hFile);
    wprintf(L"[INFO] 配置文件已新建（当前目录）：%ls\n", configPath.c_str());
    return true;
}
// ========== wmain入口 ==========
int wmain(int argc, wchar_t* argv[])
{
    SetConsoleOutputCP(CP_UTF8);                  // 控制台输出代码页设为UTF-8
    int rec = _setmode(_fileno(stdout), _O_U16TEXT);        // 标准输出设为宽字符模式
    
    check_and_create_config();

    if (argc < 2) {
        wprintf(L"[ERROR] 缺少命令参数！\n");
        wprintf(L"用法：\n");
        wprintf(L"  sl2 -a <lnk路径> [新exe名]  # 生成exe\n");
        wprintf(L"  sl2 -s                      # 查看记录\n");
        return -1;
    }

    std::wstring opt;
    std::wstring para;
    opt = argv[1];
    if(argc>2) para = argv[2];
    std::wstring new_name;
    std::wstring cmd;
    if (opt == L"-a" && argc>2) {
        cmd += L"l2e.exe \"";
        cmd += para;
        cmd += L"\" ";
        if (argc > 3) {
            new_name = argv[3];
        }
        else {
            new_name = get_file_name(para);
        }
        cmd += new_name;
        _wsystem(cmd.c_str());
    }
    
        
    return 0;
    
}

