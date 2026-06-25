#pragma once

#include "WindowsCommon.hpp"
#include <string>

struct IToastNotifierState;

class ToastNotifier {
public:
    ToastNotifier();
    ~ToastNotifier();

    ToastNotifier(const ToastNotifier&) = delete;
    ToastNotifier& operator=(const ToastNotifier&) = delete;

    bool Initialize(HINSTANCE instance, const std::wstring& appUserModelId);
    void ShowAlert(const std::wstring& title, const std::wstring& message,
                   const std::wstring& tag);
    void Shutdown();

    static bool IsStopAlertActivation();

private:
    IToastNotifierState* state_ = nullptr;
};
