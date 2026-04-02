#include <windows.h>
BOOL NeedAdmin(const WCHAR* path){
    DWORD sz=GetFileVersionInfoSizeW(path,NULL);if(!sz)return 0;
    BYTE buf[2048];GetFileVersionInfoW(path,0,sz,buf);
    WCHAR* val;UINT len;
    return VerQueryValueW(buf,L"\\StringFileInfo\\040904B0\\RequestedExecutionLevel",(void**)&val,&len)
        &&!_wcsicmp(val,L"requireAdministrator");
}
int WINAPI WinMain(HINSTANCE h,HINSTANCE p,LPSTR c,int n){
    CoInitialize(NULL);
    const WCHAR* t=L"D:\\Software\\Bandicam\\Bandicam 4.6.2.1699\\BandicamPortable\\Bandicam_Portable.exe";
    const WCHAR* a=L"";
    SHELLEXECUTEINFOW s={0};s.cbSize=sizeof(s);s.lpFile=t;s.lpParameters=a;
    s.nShow=SW_SHOWNORMAL;s.fMask=SEE_MASK_NOCLOSEPROCESS;
    s.lpVerb=NeedAdmin(t)?L"runas":NULL;
    ShellExecuteExW(&s);CoUninitialize();return 0;
}
