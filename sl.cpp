#include <windows.h>
#include <iostream>
#include <string>
#include <unordered_map>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")

// 修复结构体定义：添加结构体名，成员改为宽字符（适配wstring）
struct Para {
    std::wstring target;  // 改为wstring，兼容中文/宽字符
    std::wstring args;
};

// ========== 核心业务逻辑（原wmain内容，抽离为函数） ==========
int MainLogic(int argc, wchar_t* argv[])
{
    // 解决MinGW下wcout不输出的问题
    std::ios::sync_with_stdio(false);
    std::wcin.tie(nullptr);
    // 设置控制台UTF-8编码，解决中文乱码
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // ========== 1. 修复参数校验逻辑 ==========
    // 原逻辑argc判断错误：-a命令至少需要 程序名 + -a + lnk路径（3个参数）
    if (argc < 3) {
        std::wcout << L"Usage: sl -a <shortcut.lnk> [new_exe_name]" << std::endl;
        return 1;
    }

    // ========== 2. 解析命令参数 ==========
    std::wstring opt = argv[1];       // 命令（如 -a）
    std::wstring para = argv[2];      // lnk路径
    std::wstring new_name;            // 生成的exe名字

    // 解析新名字：有传则用传的，无传则从lnk路径截取
    if (argc >= 4) {
        new_name = argv[3];
    }
    else {
        // 截取lnk路径的文件名（去掉目录）
        new_name = para.substr(para.find_last_of(L"\\/") + 1);
        // 去掉扩展名（.lnk）
        size_t dot_pos = new_name.rfind(L'.');
        if (dot_pos != std::wstring::npos)
            new_name = new_name.substr(0, dot_pos);
    }

    // ========== 3. 拼接命令（修复逻辑顺序：先拼cmd再调用_wsystem） ==========
    std::wstring cmd;
    if (opt == L"-a") {
        cmd = L"l2e.exe ";          // 调用你的lnk2exe工具
        cmd += L"\"";               // 路径加引号（兼容空格）
        cmd += para;                // lnk路径
        cmd += L"\"";               // 闭合引号
        if (argc >= 4) {
            cmd += L" ";            // 空格分隔
            cmd += new_name;        // 新exe名字
        }
    }
    else {
        std::wcout << L"Unknown option: " << opt << std::endl;
        std::wcout << L"Only support: -a" << std::endl;
        return 1;
    }

    // ========== 4. 执行命令并输出 ==========
    std::wcout << L"Executing command: " << cmd << std::endl;
    int ret = _wsystem(cmd.c_str()); // 执行命令（需确保l2e.exe在PATH或同目录）

    // 输出执行结果
    if (ret == 0) {
        std::wcout << L"Command executed successfully!" << std::endl;
    }
    else {
        std::wcerr << L"Command failed! Error code: " << ret << std::endl;
    }

    return 0;
}

// ========== wmain入口（VS/MinGW加参数后调用） ==========
int wmain(int argc, wchar_t* argv[])
{
    return MainLogic(argc, argv);
}

// ========== WinMain入口（MinGW编译GUI程序时调用，完全模仿l2e.cpp） ==========
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // 打印原始命令行
    wchar_t* cmdLine = GetCommandLineW();
    std::wprintf(L"[DEBUG] Raw command line: %s\n", cmdLine);

    int argc;
    wchar_t** argv = CommandLineToArgvW(cmdLine, &argc);
    if (argv) {
        // 打印解析后的参数
        std::wprintf(L"[DEBUG] Parsed argc: %d\n", argc);
        for (int i = 0; i < argc; i++) {
            std::wprintf(L"[DEBUG] Parsed argv[%d]: %s\n", i, argv[i]);
        }

        int result = MainLogic(argc, argv); // 调用核心逻辑
        LocalFree(argv);
        return result;
    }
    return 1;
}