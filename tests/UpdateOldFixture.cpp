#include <windows.h>
extern "C" __declspec(dllexport) int wubrotUpdateFixture(){return 1;}
BOOL WINAPI DllMain(HINSTANCE,DWORD,LPVOID){return TRUE;}
