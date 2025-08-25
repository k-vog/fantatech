// Opens a scene select utility I found while reverse engineering the 1997 version.
// cl /utf-8 /DUNICODE /D_UNICODE /W3 /Feopenscene.exe openscene.cc

#include <assert.h>
#include <stdio.h>
#include <windows.h>

#pragma comment(lib, "user32.lib")

int main(int argc, const char* argv[])
{
  HWND hwnd = FindWindowExW(NULL, NULL, L"Evangerion 鋼鉄のガールフレンド.", NULL);
  if (!hwnd) {
    wprintf(L"Could not locate game window\n");
    return 1;
  }

  SendMessageA(hwnd, WM_COMMAND, 0x9C45, 0);
  wprintf(L"Sent command\n");
}
