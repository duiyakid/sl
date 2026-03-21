#include <windows.h>
#include <shlobj.h>
#include <objbase.h>
#include <string>
#include <fstream>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cwchar>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")

// 定义COM接口ID的GUID值
extern "C" {
    const IID IID_IShellLinkW = {0x000214F9, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
    const IID IID_IPersistFile = {0x0000010b, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
}
// 获取程序自身的目录（返回std::wstring）
std::wstring GetExeDir()
{
    WCHAR szExePath[MAX_PATH] = { 0 };
    // 获取当前exe的完整路径
    DWORD dwRet = GetModuleFileNameW(NULL, szExePath, MAX_PATH);
    if (dwRet == 0 || dwRet >= MAX_PATH)
    {
        wprintf(L"[ERROR] Failed to get exe path (error: %d)\n", GetLastError());
        return L"";
    }
    // 截取路径，去掉exe文件名，只保留目录
    std::wstring exePath = szExePath;
    size_t pos = exePath.find_last_of(L"\\/");
    if (pos == std::wstring::npos)
    {
        return L"";
    }
    std::wstring exeDir = exePath.substr(0, pos + 1); // 保留末尾的反斜杠
    return exeDir;
}
// 读取lnk的目标路径和参数
bool GetShortcutInfo(const std::wstring& lnkPath, std::wstring& targetPath, std::wstring& args)
{
    wprintf(L"[DEBUG] Entering GetShortcutInfo...\n");
    
    IShellLinkW* pShellLink = NULL;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&pShellLink);
    if (FAILED(hr)) {
        wprintf(L"[ERROR] Failed to create IShellLink instance (0x%08X)\n", hr);
        return false;
    }
    wprintf(L"[DEBUG] Created IShellLink instance\n");

    IPersistFile* pPersistFile = NULL;
    hr = pShellLink->QueryInterface(IID_IPersistFile, (void**)&pPersistFile);
    if (FAILED(hr)) {
        wprintf(L"[ERROR] Failed to get IPersistFile (0x%08X)\n", hr);
        pShellLink->Release();
        return false;
    }
    wprintf(L"[DEBUG] Got IPersistFile interface\n");

    hr = pPersistFile->Load(lnkPath.c_str(), STGM_READ);
    if (FAILED(hr)) {
        wprintf(L"[ERROR] Failed to load lnk file (0x%08X)\n", hr);
        pPersistFile->Release();
        pShellLink->Release();
        return false;
    }
    wprintf(L"[DEBUG] Loaded lnk file\n");

    // 解析快捷方式
    hr = pShellLink->Resolve(NULL, SLR_NO_UI);
    if (FAILED(hr)) {
        wprintf(L"[WARNING] Failed to resolve shortcut (0x%08X)\n", hr);
        // 继续执行，即使解析失败
    } else {
        wprintf(L"[DEBUG] Resolved shortcut successfully\n");
    }

    // 获取目标路径
    WCHAR szTarget[MAX_PATH] = { 0 };
    WIN32_FIND_DATAW wfd = { 0 };
    hr = pShellLink->GetPath(szTarget, MAX_PATH, &wfd, SLGP_UNCPRIORITY);
    if (FAILED(hr)) {
        wprintf(L"[ERROR] Failed to get lnk target (0x%08X)\n", hr);
        pPersistFile->Release();
        pShellLink->Release();
        return false;
    }
    // 不输出包含中文字符的路径
    wprintf(L"[DEBUG] Got target path (length: %d)\n", wcslen(szTarget));
    targetPath = szTarget;

    // 获取参数
    WCHAR szArgs[MAX_PATH] = { 0 };
    hr = pShellLink->GetArguments(szArgs, MAX_PATH);
    if (FAILED(hr)) {
        wprintf(L"[WARNING] Failed to get lnk arguments (0x%08X)\n", hr);
        args = L"";
    } else {
        // 不输出包含中文字符的参数
        wprintf(L"[DEBUG] Got arguments (length: %d)\n", wcslen(szArgs));
        args = szArgs;
    }

    pPersistFile->Release();
    pShellLink->Release();
    wprintf(L"[DEBUG] Exiting GetShortcutInfo\n");
    return true;
}

// 生成静默运行的cpp代码（最终exe用）
bool GenerateCppFile(const std::wstring& target, const std::wstring& args, const std::wstring& cppPath)
{
    // 使用ofstream写入文件
    std::ofstream fout(cppPath.c_str());
    if (!fout.is_open()) {
        wprintf(L"[ERROR] Failed to create cpp file: %s\n", cppPath.c_str());
        return false;
    }
    
    // 将wstring转换为string
    auto wstring_to_string = [](const std::wstring& wstr) {
        int len = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
        std::string str(len, 0);
        WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, &str[0], len, NULL, NULL);
        return str;
    };

    // 转义路径中的反斜杠（生成cpp用）
    auto escape_backslash = [](const std::string& str) {
        std::string res;
        for (char c : str) {
            if (c == '\\') res += "\\\\";
            else res += c;
        }
        return res;
    };

    // 处理目标路径和参数
    std::string targetStr = escape_backslash(wstring_to_string(target));
    std::string argsStr = escape_backslash(wstring_to_string(args));
    
    // 移除字符串中的null字符
    auto remove_null_chars = [](std::string& str) {
        str.erase(std::remove_if(str.begin(), str.end(), [](char c) { return c == '\0'; }), str.end());
    };
    
    remove_null_chars(targetStr);
    remove_null_chars(argsStr);

    // 生成代码
    std::string code;
    code += "#include <windows.h>\n";
    code += "#include <shlobj.h>\n";
    code += "#include <objbase.h>\n";
    code += "#include <string>\n";
    code += "\n";
    code += "#pragma comment(lib, \"shell32\")\n";
    code += "#pragma comment(lib, \"ole32\")\n";
    code += "#pragma comment(lib, \"version\")\n";
    code += "#pragma comment(lib, \"user32\")\n";
    code += "\n";
    code += "// 定义COM接口ID的GUID值\n";
    code += "extern \"C\" {\n";
    code += "    const IID IID_IShellLinkW = {0x000214F9, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};\n";
    code += "    const IID IID_IPersistFile = {0x0000010b, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};\n";
    code += "}\n";
    code += "\n";
    code += "BOOL IsExeRequireAdmin(const std::wstring& exePath)\n";
    code += "{\n";
    code += "    DWORD dwSize = GetFileVersionInfoSizeW(exePath.c_str(), NULL);\n";
    code += "    if (dwSize == 0) return FALSE;\n";
    code += "\n";
    code += "    BYTE* pData = new BYTE[dwSize];\n";
    code += "    if (!GetFileVersionInfoW(exePath.c_str(), 0, dwSize, pData)) {\n";
    code += "        delete[] pData;\n";
    code += "        return FALSE;\n";
    code += "    }\n";
    code += "\n";
    code += "    VS_FIXEDFILEINFO* pFixedInfo = NULL;\n";
    code += "    UINT uLen = 0;\n";
    code += "    if (!VerQueryValueW(pData, L\"\\\\\", (void**)&pFixedInfo, &uLen)) {\n";
    code += "        delete[] pData;\n";
    code += "        return FALSE;\n";
    code += "    }\n";
    code += "\n";
    code += "    BOOL bRequireAdmin = FALSE;\n";
    code += "    wchar_t szSubBlock[256];\n";
    code += "    WORD langCode = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);\n";
    code += "    WORD codepage = 1200;\n";
    code += "\n";
    code += "    wsprintfW(szSubBlock, L\"\\\\StringFileInfo\\\\%%04x%%04x\\\\RequestedExecutionLevel\", langCode, codepage);\n";
    code += "    wchar_t* pValue = NULL;\n";
    code += "    if (VerQueryValueW(pData, szSubBlock, (void**)&pValue, &uLen) && pValue) {\n";
    code += "        if (_wcsicmp(pValue, L\"requireAdministrator\") == 0) bRequireAdmin = TRUE;\n";
    code += "    }\n";
    code += "\n";
    code += "    delete[] pData;\n";
    code += "    return bRequireAdmin;\n";
    code += "}\n";
    code += "\n";
    code += "BOOL RunProcess(const std::wstring& path, const std::wstring& args, BOOL requireAdmin)\n";
    code += "{\n";
    code += "    SHELLEXECUTEINFOW sei = { 0 };\n";
    code += "    sei.cbSize = sizeof(SHELLEXECUTEINFOW);\n";
    code += "    sei.fMask = SEE_MASK_NOCLOSEPROCESS;\n";
    code += "    sei.hwnd = NULL;\n";
    code += "    sei.lpVerb = requireAdmin ? L\"runas\" : NULL;\n";
    code += "    sei.lpFile = path.c_str();\n";
    code += "    sei.lpParameters = args.c_str();\n";
    code += "    sei.lpDirectory = NULL;\n";
    code += "    sei.nShow = SW_SHOWNORMAL;\n";
    code += "    sei.hInstApp = NULL;\n";
    code += "    return ShellExecuteExW(&sei);\n";
    code += "}\n";
    code += "\n";
    code += "int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)\n";
    code += "{\n";
    code += "    HRESULT hrCoInit = CoInitialize(NULL);\n";
    code += "    if (FAILED(hrCoInit)) {\n";
    code += "        MessageBoxW(NULL, L\"COM initialization failed\", L\"Error\", MB_ICONERROR);\n";
    code += "        return 1;\n";
    code += "    }\n";
    code += "\n";
    code += "    const std::wstring targetPath = L\"" + targetStr + "\";\n";
    code += "    const std::wstring arguments = L\"" + argsStr + "\";\n";
    code += "\n";
    code += "    if (targetPath.empty()) {\n";
    code += "        MessageBoxW(NULL, L\"Target path is empty\", L\"Error\", MB_ICONERROR);\n";
    code += "        CoUninitialize();\n";
    code += "        return 1;\n";
    code += "    }\n";
    code += "\n";
    code += "    BOOL bNeedAdmin = IsExeRequireAdmin(targetPath);\n";
    code += "    if (!RunProcess(targetPath, arguments, bNeedAdmin)) {\n";
    code += "        DWORD err = GetLastError();\n";
    code += "        if (err != ERROR_CANCELLED) {\n";
    code += "            WCHAR errMsg[256];\n";
    code += "            wsprintfW(errMsg, L\"Failed to launch, error: %d\", err);\n";
    code += "            MessageBoxW(NULL, errMsg, L\"Error\", MB_ICONERROR);\n";
    code += "        }\n";
    code += "    }\n";
    code += "\n";
    code += "    CoUninitialize();\n";
    code += "    return 0;\n";
    code += "}\n";

    fout << code;
    fout.close();
    wprintf(L"[INFO] Generated cpp file: %s\n", cppPath.c_str());
    return true;
}

// 调用g++编译（命令行可见，方便调试）
bool CompileExe(const std::wstring& cppPath, const std::wstring& exePath)
{
    // 拼接编译命令（路径加引号，兼容空格）
    std::wstring cmd = L"g++ -o \"" + exePath + L"\" \"" + cppPath + L"\" -lshell32 -lole32 -luser32 -lversion -static -mwindows";

    wprintf(L"[INFO] Compiling... Command: %s\n", cmd.c_str());
    int ret = _wsystem(cmd.c_str()); // 命令行执行，直接显示编译日志

    if (ret != 0) {
        wprintf(L"[ERROR] Compile failed! (code: %d)\n", ret);
        return false;
    }
    wprintf(L"[INFO] Compiled successfully: %s\n", exePath.c_str());
    return true;
}

// 显示使用帮助
void ShowHelp()
{
    wprintf(L"Usage: lnk2exe.exe <shortcut_path>\n");
    wprintf(L"Example: lnk2exe.exe D:\\var\\Bandicam.lnk\n");
    wprintf(L"Note: g++ must be in system PATH (MinGW-w64 recommended)\n");
}

int wmain(int argc, wchar_t* argv[])
{
    // 检查参数
    //if (argc != 2) {
    //    ShowHelp();
    //    return 1;
    //}

    // 使用命令行参数
    std::wstring lnkPath = argv[1];
    wprintf(L"[DEBUG] Using command line path: %s\n", lnkPath.c_str());

    // 第一步：先截取带扩展名的文件名（你原来的代码）
	std::wstring new_name;
    if (argc >= 3) {
        new_name = argv[2];
    }
    else {
        new_name = lnkPath.substr(lnkPath.find_last_of(L"\\/") + 1);
    }
    // 第二步：找到最后一个"."的位置，删掉后面的扩展名
    size_t dot_pos = new_name.rfind(L'.');  // 找最后一个点
    if (dot_pos != std::wstring::npos)      // 如果找到点
        new_name = new_name.substr(0, dot_pos);  // 只保留点前面的部分


    // 校验lnk文件是否存在
    DWORD attributes = GetFileAttributesW(lnkPath.c_str());
    wprintf(L"[DEBUG] GetFileAttributes returned: %d\n", attributes);
    
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        DWORD error = GetLastError();
        wprintf(L"[ERROR] Lnk file not found. Error: %d\n", error);
        return 1;
    }
    wprintf(L"[DEBUG] File exists!\n");


    // 初始化COM
    CoInitialize(NULL);

    // 1. 读取lnk信息
    std::wstring targetPath, args;
    // 不输出lnkPath，避免编码问题
    wprintf(L"[DEBUG] Calling GetShortcutInfo...\n");
    if (!GetShortcutInfo(lnkPath, targetPath, args)) {
        CoUninitialize();
        return 1;
    }
    wprintf(L"[INFO] Lnk target: %s\n", targetPath.c_str());
    wprintf(L"[INFO] Lnk arguments: %s\n", args.empty() ? L"none" : args.c_str());

    // 2. 生成cpp文件（和lnk同名）
    std::wstring cppPath = L"tmp.cpp";
    wprintf(L"[DEBUG] Generated cpp path: tmp.cpp\n");
    if (!GenerateCppFile(targetPath, args, cppPath)) {
        CoUninitialize();
        return 1;
    }

    // 3. 编译生成exe（和lnk同名，静默无黑框）
    std::wstring exePath = new_name+L".exe";
    wprintf(L"[DEBUG] Generated exe path: test_output.exe\n");
    if (!CompileExe(cppPath, exePath)) {
        CoUninitialize();
        return 1;
    }

    // 检查生成的文件是否存在
    if (GetFileAttributesW(exePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        wprintf(L"\n[SUCCESS] All done! Final exe created successfully.\n");
        wprintf(L"[TIP] Put the exe to environment variable directory, run with Win+R\n");
    } else {
        wprintf(L"\n[ERROR] Failed to create final exe.\n");
    }

    CoUninitialize();
    return 0;
}

// WinMain入口点，调用wmain
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // 打印原始命令行
    wchar_t* cmdLine = GetCommandLineW();
    wprintf(L"[DEBUG] Raw command line: %s\n", cmdLine);
    
    int argc;
    wchar_t** argv = CommandLineToArgvW(cmdLine, &argc);
    if (argv) {
        // 打印解析后的参数
        wprintf(L"[DEBUG] Parsed argc: %d\n", argc);
        for (int i = 0; i < argc; i++) {
            wprintf(L"[DEBUG] Parsed argv[%d]: %s\n", i, argv[i]);
        }
        
        int result = wmain(argc, argv);
        LocalFree(argv);
        return result;
    }
    return 1;
}
