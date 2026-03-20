#include <iostream>
#include <Windows.h>
#include <shlobj_core.h>  // IShellLinkW的定义
#include <shlwapi.h>      // 辅助函数
#include <locale>         // 解决中文乱码
#include <fstream>        // 写入启动器代码文件
#include <string>         // 字符串处理
#pragma comment(lib, "shlwapi.lib") // 链接shlwapi库（避免手动配置依赖）

// 定义存储LNK信息的结构体（简化版，先实现核心字段）
struct LnkInfo {
    wchar_t target_path[MAX_PATH] = { 0 };  // 目标程序路径（宽字符）
    wchar_t arguments[MAX_PATH] = { 0 };    // 启动参数
    wchar_t work_dir[MAX_PATH] = { 0 };     // 工作目录
    DWORD show_cmd = SW_SHOWNORMAL;       // 窗口显示方式（DWORD类型，避免类型错误）
};

// 辅助函数：转义宽字符字符串中的特殊字符（" → \", \ → \\），解决C1075语法错误
std::wstring escape_wstring(const wchar_t* input) {
    std::wstring output;
    for (const wchar_t* p = input; *p != L'\0'; ++p) {
        switch (*p) {
        case L'"':  output += L"\\\""; break; // 转义双引号
        case L'\\': output += L"\\\\"; break; // 转义反斜杠
        default:    output += *p; break;     // 其他字符原样保留
        }
    }
    return output;
}

// 解析LNK文件的函数（输入：LNK路径；输出：填充后的LnkInfo结构体）
bool parse_lnk_file(const wchar_t* lnk_path, LnkInfo& lnk_info) {
    IShellLinkW* p_shell_link = nullptr;
    IPersistFile* p_persist_file = nullptr;
    HRESULT hr;

    // 1. 创建IShellLinkW实例（COM组件，解析LNK的核心）
    hr = CoCreateInstance(
        CLSID_ShellLink,        // ShellLink组件的唯一标识
        nullptr,                // 无聚合对象
        CLSCTX_INPROC_SERVER,   // 进程内调用（本地组件）
        IID_IShellLinkW,        // IShellLinkW接口的唯一标识
        (void**)&p_shell_link   // 输出创建的实例指针
    );
    if (FAILED(hr)) {
        std::cerr << "创建IShellLinkW失败，错误码：" << hr << std::endl;
        return false;
    }

    // 2. 查询IPersistFile接口（用于加载LNK文件）
    hr = p_shell_link->QueryInterface(IID_IPersistFile, (void**)&p_persist_file);
    if (FAILED(hr)) {
        std::cerr << "查询IPersistFile失败，错误码：" << hr << std::endl;
        p_shell_link->Release(); // 释放已创建的对象，避免内存泄漏
        return false;
    }

    // 3. 加载指定的LNK文件（只读模式）
    hr = p_persist_file->Load(lnk_path, STGM_READ);
    if (FAILED(hr)) {
        std::cerr << "加载LNK文件失败（路径错误/文件损坏），错误码：" << hr << std::endl;
        p_persist_file->Release();
        p_shell_link->Release();
        return false;
    }

    // 4. 解析LNK的核心信息，填充到结构体中
    p_shell_link->GetPath(lnk_info.target_path, MAX_PATH, nullptr, SLGP_UNCPRIORITY); // 目标路径
    p_shell_link->GetArguments(lnk_info.arguments, MAX_PATH);                         // 启动参数
    p_shell_link->GetWorkingDirectory(lnk_info.work_dir, MAX_PATH);                   // 工作目录
    {
        // 修正：GetShowCmd 需要 int* 类型参数，不能直接传 DWORD*，需做类型转换
        int show_cmd = static_cast<int>(lnk_info.show_cmd);
        p_shell_link->GetShowCmd(&show_cmd); // 窗口显示方式
        lnk_info.show_cmd = static_cast<DWORD>(show_cmd);
    }

    // 5. 释放COM对象（必须！否则内存泄漏）
    p_persist_file->Release();
    p_shell_link->Release();

    return true;
}

// 生成启动器C++代码文件（将LNK信息硬编码到代码中，已修复转义问题）
bool generate_launcher_code(const LnkInfo& lnk_info, const wchar_t* code_file_path) {
    // 打开文件（覆盖写入，支持宽字符）
    std::wofstream launcher_code_file(code_file_path);
    if (!launcher_code_file.is_open()) {
        std::cerr << "创建启动器代码文件失败！" << std::endl;
        return false;
    }

    // 关键修复：转义特殊字符，避免破坏C++字符串语法
    std::wstring target_path_escaped = escape_wstring(lnk_info.target_path);
    std::wstring arguments_escaped = escape_wstring(lnk_info.arguments);
    std::wstring work_dir_escaped = escape_wstring(lnk_info.work_dir);

    // 写入启动器模板代码（硬编码LNK解析出的信息）
    launcher_code_file << L"#include <iostream>" << std::endl;
    launcher_code_file << L"#include <Windows.h>" << std::endl;
    launcher_code_file << L"#include <locale>" << std::endl;
    launcher_code_file << L"#pragma comment(lib, \"shlwapi.lib\")" << std::endl;
    launcher_code_file << L"" << std::endl;
    launcher_code_file << L"int main() {" << std::endl;
    launcher_code_file << L"    setlocale(LC_ALL, \"chs\");" << std::endl;
    launcher_code_file << L"" << std::endl;
    launcher_code_file << L"    const wchar_t* target_path = L\"" << target_path_escaped << L"\";" << std::endl;
    launcher_code_file << L"    const wchar_t* arguments = L\"" << arguments_escaped << L"\";" << std::endl;
    launcher_code_file << L"    const wchar_t* work_dir = L\"" << work_dir_escaped << L"\";" << std::endl;
    launcher_code_file << L"    DWORD show_cmd = " << lnk_info.show_cmd << L";" << std::endl;
    launcher_code_file << L"" << std::endl;
    launcher_code_file << L"    wchar_t cmd_line[MAX_PATH * 2] = {0};" << std::endl;
    launcher_code_file << L"    wcscpy_s(cmd_line, target_path);" << std::endl;
    launcher_code_file << L"    if (wcslen(arguments) > 0) {" << std::endl;
    launcher_code_file << L"        wcscat_s(cmd_line, L\" \");" << std::endl;
    launcher_code_file << L"        wcscat_s(cmd_line, arguments);" << std::endl;
    launcher_code_file << L"    }" << std::endl;
    launcher_code_file << L"" << std::endl;
    launcher_code_file << L"    STARTUPINFOW si = {0};" << std::endl;
    launcher_code_file << L"    si.cb = sizeof(si);" << std::endl;
    launcher_code_file << L"    si.wShowWindow = show_cmd;" << std::endl;
    launcher_code_file << L"    PROCESS_INFORMATION pi = {0};" << std::endl;
    launcher_code_file << L"" << std::endl;
    launcher_code_file << L"    BOOL launch_ok = CreateProcessW(" << std::endl;
    launcher_code_file << L"        nullptr," << std::endl;
    launcher_code_file << L"        cmd_line," << std::endl;
    launcher_code_file << L"        nullptr," << std::endl;
    launcher_code_file << L"        nullptr," << std::endl;
    launcher_code_file << L"        FALSE," << std::endl;
    launcher_code_file << L"        0," << std::endl;
    launcher_code_file << L"        nullptr," << std::endl;
    launcher_code_file << L"        work_dir," << std::endl;
    launcher_code_file << L"        &si," << std::endl;
    launcher_code_file << L"        &pi" << std::endl;
    launcher_code_file << L"    );" << std::endl;
    launcher_code_file << L"" << std::endl;
    launcher_code_file << L"    if (launch_ok) {" << std::endl;
    launcher_code_file << L"        CloseHandle(pi.hProcess);" << std::endl;
    launcher_code_file << L"        CloseHandle(pi.hThread);" << std::endl;
    launcher_code_file << L"    } else {" << std::endl;
    launcher_code_file << L"        system(\"pause\");" << std::endl;
    launcher_code_file << L"    }" << std::endl;
    launcher_code_file << L"" << std::endl;
    launcher_code_file << L"    return 0;" << std::endl;
    launcher_code_file << L"}" << std::endl;

    launcher_code_file.close();
    std::wcout << L"\n启动器代码文件生成成功：" << code_file_path << std::endl;
    return true;
}

// 调用VS编译器编译启动器代码，生成独立EXE
bool compile_launcher(const wchar_t* code_file_path, const wchar_t* exe_file_path) {
    // 拼接编译命令（需确保VS的cl.exe在系统环境变量中）
    std::wstring compile_cmd = L"cl /EHsc /W3 \"";
    compile_cmd += code_file_path;
    compile_cmd += L"\" /Fe:\"";
    compile_cmd += exe_file_path;
    compile_cmd += L"\"";

    // 执行编译命令
    std::wcout << L"\n开始编译独立EXE，命令：" << compile_cmd << std::endl;
    int compile_result = _wsystem(compile_cmd.c_str());

    if (compile_result == 0) {
        std::wcout << L"\n独立EXE生成成功：" << exe_file_path << std::endl;
        return true;
    }
    else {
        std::cerr << "\n编译失败！请确保：" << std::endl;
        std::cerr << "1. 已安装Visual Studio并配置环境变量（以\"x64 Native Tools Command Prompt for VS\"运行）" << std::endl;
        std::cerr << "2. cl.exe能在命令行中正常调用" << std::endl;
        return false;
    }
}

int main() {
    // 解决wcout输出中文乱码
    setlocale(LC_ALL, "chs");

    // 1. 初始化COM库（必须！否则解析LNK会失败）
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) {
        std::cerr << "COM库初始化失败，错误码：" << hr << std::endl;
        return -1;
    }

    // ========== 调用parse_lnk_file函数的核心逻辑 ==========
    // 2. 定义要解析的LNK文件路径（替换成你自己的路径！）
    const wchar_t* test_lnk_path = L"D:\\var\\明日方舟-MuMu安卓设备.lnk";

    // 3. 定义结构体变量，用于存储解析结果
    LnkInfo lnk_data;

    // 4. 调用解析函数，传入LNK路径和结构体
    if (parse_lnk_file(test_lnk_path, lnk_data)) {
        // 解析成功：打印结果
        std::wcout << L"LNK文件解析成功！\n" << std::endl;
        std::wcout << L"目标程序路径：" << lnk_data.target_path << std::endl;
        std::wcout << L"启动参数：" << (wcslen(lnk_data.arguments) > 0 ? lnk_data.arguments : L"无") << std::endl;
        std::wcout << L"工作目录：" << (wcslen(lnk_data.work_dir) > 0 ? lnk_data.work_dir : L"默认") << std::endl;
        std::wcout << L"窗口显示方式：" << lnk_data.show_cmd << std::endl;

        // ========== 生成独立EXE的核心逻辑 ==========
        // 自定义生成路径（确保文件夹已存在！）
        const wchar_t* launcher_code_path = L"D:\\var\\launcher.cpp";  // 临时启动器代码
        const wchar_t* final_exe_path = L"D:\\var\\明日方舟启动器.exe"; // 最终独立EXE

        // 步骤1：生成启动器代码文件（已修复转义）
        if (generate_launcher_code(lnk_data, launcher_code_path)) {
            // 步骤2：编译代码生成独立EXE
            compile_launcher(launcher_code_path, final_exe_path);
        }

    }
    else {
        // 解析失败：提示错误
        std::cerr << "LNK文件解析失败！" << std::endl;
    }

    // 最后释放COM库
    CoUninitialize();
    system("pause"); // 暂停窗口，方便查看结果
    return 0;
}