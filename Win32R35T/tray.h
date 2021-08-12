#pragma once
 
#include <windows.h>
#include <shellapi.h>
#include "resource.h"
#include "custommessages.h"
 
#pragma comment( lib, "Shell32.lib" )

#define TRAY_ICONUID 100
 
extern HINSTANCE hInst;
 
void TrayDrawIcon(HWND hWnd);
void TrayDeleteIcon(HWND hWnd);
void TrayLoadPopupMenu(HWND hWnd);