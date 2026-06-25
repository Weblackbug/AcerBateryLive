#pragma once

#include "WindowsCommon.hpp"

#include "Config.hpp"

void CenterDialogOnScreen(HWND dialog);
void CenterDialogOnOwner(HWND dialog, HWND owner);
INT_PTR ShowMessageBoxCentered(HWND owner, LPCWSTR text, LPCWSTR caption, UINT type);

INT_PTR ShowConfigDialog(HWND owner, AppConfig& config);
INT_PTR ShowAboutDialog(HWND owner);
