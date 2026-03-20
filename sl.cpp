#include <iostream>
#include <Windows.h>
#include <shlobj_core.h>  // IShellLinkW的定义
#include <shlwapi.h>      // 辅助函数
#pragma comment(lib, "shlwapi.lib") // 链接shlwapi库（避免手动配置依赖）

// 定义存储LNK信息的结构体（简化版，先实现核心字段）
struct LnkInfo {
    wchar_t target_path[MAX_PATH] = { 0 };  // 目标程序路径（宽字符）
    wchar_t arguments[MAX_PATH] = { 0 };    // 启动参数
    wchar_t work_dir[MAX_PATH] = { 0 };     // 工作目录
    DWORD show_cmd = SW_SHOWNORMAL;       // 窗口显示方式（DWORD类型，避免类型错误）
};
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
int main() {
    // 新增：解决wcout输出中文乱码（必须！否则宽字符打印乱码）
    setlocale(LC_ALL, "chs");

    // 1. 初始化COM库（必须！否则解析LNK会失败）
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) {
        std::cerr << "COM库初始化失败，错误码：" << hr << std::endl;
        return -1;
    }

    // ========== 新增：调用parse_lnk_file函数的核心逻辑 ==========
    // 2. 定义要解析的LNK文件路径（替换成你自己的测试路径！）
    // 注意：路径中的反斜杠要写两个\\，宽字符要加L前缀
    const wchar_t* test_lnk_path = L"D:\\var\\明日方舟-MuMu安卓设备.lnk";
    
    // 3. 定义结构体变量，用于存储解析结果
    LnkInfo lnk_data;

    // 4. 调用解析函数，传入LNK路径和结构体
    if (parse_lnk_file(test_lnk_path, lnk_data)) {
        // 解析成功：打印结果（宽字符用wcout输出）
        std::wcout << L"LNK文件解析成功！\n" << std::endl;
        std::wcout << L"目标程序路径：" << lnk_data.target_path << std::endl;
        std::wcout << L"启动参数：" << (wcslen(lnk_data.arguments) > 0 ? lnk_data.arguments : L"无") << std::endl;
        std::wcout << L"工作目录：" << (wcslen(lnk_data.work_dir) > 0 ? lnk_data.work_dir : L"默认") << std::endl;
        std::wcout << L"窗口显示方式：" << lnk_data.show_cmd << std::endl;
    } else {
        // 解析失败：提示错误
        std::cerr << "LNK文件解析失败！" << std::endl;
    }
    // ========== 新增结束 ==========

    // 最后释放COM库
    CoUninitialize();
    return 0;
}