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
// ========== wmain入口 ==========
int wmain(int argc, wchar_t* argv[])
{
    SetConsoleOutputCP(CP_UTF8);                  // 控制台输出代码页设为UTF-8
    int rec = _setmode(_fileno(stdout), _O_U16TEXT);        // 标准输出设为宽字符模式
    
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

