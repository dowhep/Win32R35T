
#include "tray.h"
#include <stdlib.h>
#include <string>

//
//  FUNCTION: TrayDrawIcon(HWND)
//
//  PURPOSE:  Draws application icon in a system tray
//
//
void TrayDrawIcon(HWND hWnd) {
	NOTIFYICONDATA nid;
	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.hWnd = hWnd;
	nid.uID = TRAY_ICONUID;
	nid.uVersion = NOTIFYICON_VERSION;
	nid.uCallbackMessage = WM_TRAYMESSAGE;
	nid.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_MAINICON));
	LoadString(hInst, IDS_NOTIFICATIONTIP, nid.szTip, 128);
	nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	Shell_NotifyIcon(NIM_ADD, &nid);
}
//
//  FUNCTION: TrayDeleteIcon(HWND)
//
//  PURPOSE:  Deletes application icon from system tray
//
//
void TrayDeleteIcon(HWND hWnd) {
	NOTIFYICONDATA nid;
	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.hWnd = hWnd;
	nid.uID = TRAY_ICONUID;
	Shell_NotifyIcon(NIM_DELETE, &nid);
}
//
//  FUNCTION: TrayLoadPopupMenu(HWND)
//
//  PURPOSE:  Load tray specific popup menu
//
//
void TrayLoadPopupMenu(HWND hWnd) {
	POINT cursor;
	HMENU hMenu;
	GetCursorPos(&cursor);
	hMenu = GetSubMenu(LoadMenu(hInst, MAKEINTRESOURCE(IDR_TRAYMENU)), 0);
	SetMenuDefaultItem(hMenu, ID_TRAY_SHOW, false);
	TrackPopupMenu(hMenu, TPM_LEFTALIGN, cursor.x, cursor.y, 0, hWnd, NULL);
	DestroyMenu(hMenu);
	//std::wstring dbstr = std::to_wstring(cursor.x) + L", " + std::to_wstring(cursor.y);
	//OutputDebugString((dbstr + L"\n").c_str());
	
	
}