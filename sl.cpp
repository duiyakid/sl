#include <iostream>
#include <Windows.h>
#include <shlobj_core.h>
#include <shlwapi.h>
#include <locale>
#include <fstream>
#include <string>

#pragma comment(lib, "shlwapi.lib")

struct LnkInfo {
    wchar_t target_path[MAX_PATH] = { 0 };
    wchar_t arguments[MAX_PATH] = { 0 };
    wchar_t work_dir[MAX_PATH] = { 0 };
    DWORD show_cmd = SW_SHOWNORMAL;
};

std::wstring escape_wstring(const wchar_t* input) {
    std::wstring output;
    for (const wchar_t* p = input; *p != L'\0'; ++p) {
        switch (*p) {
        case L'"':
            output += L"\\\"";
            break;
        case L'\\':
            output += L"\\\\";
            break;
        default:
            output += *p;
            break;
        }
    }
    return output;
}

bool parse_lnk_file(const wchar_t* lnk_path, LnkInfo& lnk_info) {
    IShellLinkW* p_shell_link = nullptr;
    IPersistFile* p_persist_file = nullptr;
    HRESULT hr;

    hr = CoCreateInstance(
        CLSID_ShellLink,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_IShellLinkW,
        (void**)&p_shell_link
    );
    if (FAILED(hr)) {
        std::cerr << "创建IShellLinkW失败，错误码：" << hr << std::endl;
        return false;
    }

    hr = p_shell_link->QueryInterface(IID_IPersistFile, (void**)&p_persist_file);
    if (FAILED(hr)) {
        std::cerr << "查询IPersistFile失败，错误码：" << hr << std::endl;
        p_shell_link->Release();
        return false;
    }

    hr = p_persist_file->Load(lnk_path, STGM_READ);
    if (FAILED(hr)) {
        std::cerr << "加载LNK文件失败，错误码：" << hr << std::endl;
        p_persist_file->Release();
        p_shell_link->Release();
        return false;
    }

    p_shell_link->GetPath(lnk_info.target_path, MAX_PATH, nullptr, SLGP_UNCPRIORITY);
    p_shell_link->GetArguments(lnk_info.arguments, MAX_PATH);
    p_shell_link->GetWorkingDirectory(lnk_info.work_dir, MAX_PATH);

    int show_cmd = static_cast<int>(lnk_info.show_cmd);
    p_shell_link->GetShowCmd(&show_cmd);
    lnk_info.show_cmd = static_cast<DWORD>(show_cmd);

    p_persist_file->Release();
    p_shell_link->Release();
    return true;
}

bool generate_launcher_code(const LnkInfo& lnk_info, const wchar_t* code_file_path) {
    std::ofstream launcher_code_file(code_file_path, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!launcher_code_file.is_open()) {
        std::cerr << "创建启动器代码文件失败！" << std::endl;
        return false;
    }

    std::wstring target_path_escaped = escape_wstring(lnk_info.target_path);
    std::wstring arguments_escaped = escape_wstring(lnk_info.arguments);
    std::wstring work_dir_escaped = escape_wstring(lnk_info.work_dir);

    // 核心修改：生成GUI程序（无控制台）+ 静默启动目标程序
    std::wstring cpp_code;
    // 1. 添加Windows GUI程序入口（替代控制台main）
    cpp_code += L"#include <Windows.h>\n";
    cpp_code += L"#pragma comment(lib, \"shlwapi.lib\")\n";
    cpp_code += L"#pragma comment(linker, \"/SUBSYSTEM:WINDOWS /ENTRY:WinMainCRTStartup\")\n\n";
    // 2. WinMain为GUI程序入口，无控制台窗口
    cpp_code += L"int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {\n";
    cpp_code += L"    wchar_t target_path[] = L\"" + target_path_escaped + L"\";\n";
    cpp_code += L"    wchar_t arguments[] = L\"" + arguments_escaped + L"\";\n";
    cpp_code += L"    wchar_t work_dir[] = L\"" + work_dir_escaped + L"\";\n";
    cpp_code += L"    DWORD show_cmd = " + std::to_wstring(lnk_info.show_cmd) + L";\n\n";
    // 3. 拼接命令行（兼容带参数的情况）
    cpp_code += L"    wchar_t cmd_line[MAX_PATH * 2] = {0};\n";
    cpp_code += L"    wcscpy_s(cmd_line, target_path);\n";
    cpp_code += L"    if (wcslen(arguments) > 0) {\n";
    cpp_code += L"        wcscat_s(cmd_line, L\" \");\n";
    cpp_code += L"        wcscat_s(cmd_line, arguments);\n";
    cpp_code += L"    }\n\n";
    // 4. 静默启动配置：隐藏启动器自身窗口，正常显示目标程序
    cpp_code += L"    STARTUPINFOW si = {0};\n";
    cpp_code += L"    si.cb = sizeof(si);\n";
    cpp_code += L"    si.wShowWindow = static_cast<WORD>(show_cmd);\n";
    cpp_code += L"    si.dwFlags = STARTF_USESHOWWINDOW;\n"; // 启用窗口显示配置
    cpp_code += L"    PROCESS_INFORMATION pi = {0};\n\n";
    // 5. 启动目标程序（无任何输出/暂停）
    cpp_code += L"    CreateProcessW(\n";
    cpp_code += L"        nullptr,\n";
    cpp_code += L"        cmd_line,\n";
    cpp_code += L"        nullptr,\n";
    cpp_code += L"        nullptr,\n";
    cpp_code += L"        FALSE,\n";
    cpp_code += L"        0,\n";
    cpp_code += L"        nullptr,\n";
    cpp_code += L"        work_dir,\n";
    cpp_code += L"        &si,\n";
    cpp_code += L"        &pi\n";
    cpp_code += L"    );\n\n";
    // 6. 释放句柄后立即退出，无任何弹窗/暂停
    cpp_code += L"    if (pi.hProcess != nullptr) CloseHandle(pi.hProcess);\n";
    cpp_code += L"    if (pi.hThread != nullptr) CloseHandle(pi.hThread);\n";
    cpp_code += L"    return 0;\n";
    cpp_code += L"}\n";

    // 转换为UTF-8写入
    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, cpp_code.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8_code(utf8_len, 0);
    WideCharToMultiByte(CP_UTF8, 0, cpp_code.c_str(), -1, &utf8_code[0], utf8_len, nullptr, nullptr);
    utf8_code.pop_back();
    launcher_code_file.write(utf8_code.c_str(), utf8_code.size());

    if (!launcher_code_file.good()) {
        std::cerr << "写入启动器代码文件失败！" << std::endl;
        launcher_code_file.close();
        return false;
    }

    launcher_code_file.close();
    std::wcout << L"\n启动器代码文件生成成功：" << code_file_path << std::endl;
    return true;
}

bool compile_launcher(const wchar_t* code_file_path, const wchar_t* exe_file_path) {
    // 编译命令：指定SUBSYSTEM为WINDOWS（GUI程序），无控制台
    std::wstring compile_cmd = L"cl /EHsc /W3 /utf-8 ";
    compile_cmd += L"/Fo\"D:\\var\\launcher.obj\" ";
    compile_cmd += L"/Fd\"D:\\var\\launcher.pdb\" ";
    compile_cmd += L"\"" + std::wstring(code_file_path) + L"\" ";
    compile_cmd += L"/Fe:\"" + std::wstring(exe_file_path) + L"\" ";
    compile_cmd += L"/machine:x64 ";
    compile_cmd += L"/link /SUBSYSTEM:WINDOWS"; // 强制编译为GUI程序

    std::wcout << L"\n开始编译独立EXE，命令：" << compile_cmd << std::endl;
    int compile_result = _wsystem(compile_cmd.c_str());

    if (compile_result == 0) {
        std::wcout << L"\n独立EXE生成成功：" << exe_file_path << std::endl;
        return true;
    }
    else {
        std::cerr << "\n编译失败！请确保：" << std::endl;
        std::cerr << "1. 已打开\"x64 Native Tools Command Prompt for VS\"" << std::endl;
        std::cerr << "2. cl.exe能在命令行中正常调用" << std::endl;
        std::cerr << "3. D:\\var目录有读写权限" << std::endl;
        return false;
    }
}

int main() {
    setlocale(LC_ALL, "chs");

    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) {
        std::cerr << "COM库初始化失败，错误码：" << hr << std::endl;
        return -1;
    }

    const wchar_t* test_lnk_path = L"D:\\var\\Bandicam.lnk";
    LnkInfo lnk_data;

    if (parse_lnk_file(test_lnk_path, lnk_data)) {
        std::wcout << L"LNK文件解析成功！\n" << std::endl;
        std::wcout << L"目标程序路径：" << lnk_data.target_path << std::endl;
        std::wcout << L"启动参数：" << (wcslen(lnk_data.arguments) > 0 ? lnk_data.arguments : L"无") << std::endl;
        std::wcout << L"工作目录：" << (wcslen(lnk_data.work_dir) > 0 ? lnk_data.work_dir : L"默认") << std::endl;
        std::wcout << L"窗口显示方式：" << lnk_data.show_cmd << std::endl;

        const wchar_t* launcher_code_path = L"D:\\var\\launcher.cpp";
        const wchar_t* final_exe_path = L"D:\\var\\bandi.exe";

        if (generate_launcher_code(lnk_data, launcher_code_path)) {
            compile_launcher(launcher_code_path, final_exe_path);
        }
    }
    else {
        std::cerr << "LNK文件解析失败！" << std::endl;
    }

    CoUninitialize();
    system("pause");
    return 0;
}