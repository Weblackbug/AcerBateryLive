#pragma once

#include "WindowsCommon.hpp"
#include <functional>
#include <string>

class TrayIcon {
public:
    using MenuHandler = std::function<void(UINT commandId)>;

    TrayIcon();
    ~TrayIcon();

    bool Create(HWND hwnd, HINSTANCE instance, UINT callbackMessage, MenuHandler handler);
    void Destroy();
    void UpdateTooltip(const std::wstring& text);
    void ShowBalloon(const std::wstring& title, const std::wstring& message);
    void ShowContextMenu();

private:
    static HMENU CreateMenu();

    HWND hwnd_ = nullptr;
    HINSTANCE instance_ = nullptr;
    NOTIFYICONDATAW nid_{};
    MenuHandler handler_;
    bool created_ = false;
};
