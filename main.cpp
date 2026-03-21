#include <iostream>
#include <string>
#include <windows.h>

int wmain(int argc, wchar_t* argv[]) {
    // 就这一行，解决中文输出乱码
    SetConsoleOutputCP(CP_UTF8);

    // 直接用wmain的参数，不用任何解析
    std::wcout << L"参数总数：" << argc << std::endl;
    for (int i = 0; i < argc; i++) {
        std::wcout << L"argv[" << i << L"]：" << argv[i] << std::endl;
    }
    return 0;
}