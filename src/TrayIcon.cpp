#include "TrayIcon.hpp"

#include "resource.h"

namespace {

constexpr UINT kTrayIconId = 1;

}  // namespace

TrayIcon::TrayIcon() {
    ZeroMemory(&nid_, sizeof(nid_));
}

TrayIcon::~TrayIcon() {
    Destroy();
}

HMENU TrayIcon::CreateMenu() {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, IDM_TRAY_OPEN_CONFIG, L"Configuracion...");
    AppendMenuW(menu, MF_STRING, IDM_TRAY_STOP_CHARGE, L"Parar carga / Modo salud...");
    AppendMenuW(menu, MF_STRING, IDM_TRAY_STOP_ALERT, L"Detener aviso sonoro");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_TRAY_ABOUT, L"Acerca de...");
    AppendMenuW(menu, MF_STRING, IDM_TRAY_EXIT, L"Salir");
    return menu;
}

bool TrayIcon::Create(HWND hwnd, HINSTANCE instance, UINT callbackMessage,
                      MenuHandler handler) {
    hwnd_ = hwnd;
    instance_ = instance;
    handler_ = std::move(handler);

    nid_.cbSize = sizeof(NOTIFYICONDATAW);
    nid_.hWnd = hwnd_;
    nid_.uID = kTrayIconId;
    nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid_.uCallbackMessage = callbackMessage;
    nid_.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_APP_ICON));
    if (!nid_.hIcon) {
        nid_.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    wcscpy_s(nid_.szTip, L"VidaUtil de la Bateria");

    created_ = Shell_NotifyIconW(NIM_ADD, &nid_) == TRUE;
    if (created_) {
        NOTIFYICONDATAW versionData = nid_;
        versionData.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &versionData);
    }
    return created_;
}

void TrayIcon::Destroy() {
    if (created_) {
        Shell_NotifyIconW(NIM_DELETE, &nid_);
        created_ = false;
    }
    if (nid_.hIcon) {
        DestroyIcon(nid_.hIcon);
        nid_.hIcon = nullptr;
    }
}

void TrayIcon::UpdateTooltip(const std::wstring& text) {
    if (!created_) {
        return;
    }

    wcsncpy_s(nid_.szTip, text.c_str(), _TRUNCATE);
    nid_.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
}

void TrayIcon::ShowBalloon(const std::wstring& title, const std::wstring& message) {
    if (!created_) {
        return;
    }

    nid_.uFlags = NIF_INFO;
    nid_.dwInfoFlags = NIIF_INFO;
    wcsncpy_s(nid_.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(nid_.szInfo, message.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
    nid_.uFlags = NIF_TIP;
}

void TrayIcon::ShowContextMenu() {
    HMENU menu = CreateMenu();
    POINT cursor{};
    GetCursorPos(&cursor);
    SetForegroundWindow(hwnd_);
    const UINT command =
        TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, hwnd_,
                       nullptr);
    DestroyMenu(menu);
    PostMessageW(hwnd_, WM_NULL, 0, 0);

    if (command != 0 && handler_) {
        handler_(command);
    }
}
