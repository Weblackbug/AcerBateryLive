#include "App.hpp"



#include "WindowsCommon.hpp"

#include "BatteryMonitor.hpp"

#include "ChargeControl.hpp"

#include "Config.hpp"

#include "Dialogs.hpp"

#include "LaptopBrands.hpp"

#include "resource.h"



namespace {



constexpr wchar_t kWindowClass[] = L"VidaUtilBateriaHiddenWindow";

constexpr wchar_t kMutexName[] = L"Global\\VidaUtilBateria_SingleInstance";

constexpr UINT kTrayCallbackMessage = WM_APP + 1;

constexpr UINT kPollIntervalMs = 5000;



App* g_appInstance = nullptr;



}  // namespace



App& App::Instance() {

    static App instance;

    return instance;

}



LRESULT CALLBACK App::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {

    if (msg == kTrayCallbackMessage && g_appInstance) {

        g_appInstance->OnTrayCallback(lParam);

        return 0;

    }



    if (g_appInstance) {

        switch (msg) {

            case WM_TIMER:

                g_appInstance->OnTimer();

                return 0;

            case WM_CLOSE:

                DestroyWindow(hwnd);

                return 0;

            case WM_DESTROY:

                PostQuitMessage(0);

                return 0;

        }

    }



    return DefWindowProcW(hwnd, msg, wParam, lParam);

}



bool App::Initialize(HINSTANCE instance) {

    instance_ = instance;

    g_appInstance = this;



    Config::Instance().Load();



    WNDCLASSEXW wc{};

    wc.cbSize = sizeof(wc);

    wc.lpfnWndProc = WindowProc;

    wc.hInstance = instance_;

    wc.lpszClassName = kWindowClass;

    RegisterClassExW(&wc);



    hwnd_ = CreateWindowExW(0, kWindowClass, L"VidaUtil de la Bateria", 0, 0, 0, 0, 0, HWND_MESSAGE,

                              nullptr, instance_, nullptr);

    if (!hwnd_) {

        return false;

    }



    if (!tray_.Create(

            hwnd_, instance_, kTrayCallbackMessage,

            [this](UINT commandId) { OnTrayCommand(commandId); })) {

        return false;

    }



    UpdateTrayTooltip();

    timerId_ = SetTimer(hwnd_, 1, kPollIntervalMs, nullptr);

    OnTimer();

    return timerId_ != 0;

}



void App::Shutdown() {

    if (timerId_ != 0) {

        KillTimer(hwnd_, timerId_);

        timerId_ = 0;

    }

    StopAlert();

    tray_.Destroy();

    if (hwnd_) {

        DestroyWindow(hwnd_);

        hwnd_ = nullptr;

    }

    g_appInstance = nullptr;

}



void App::UpdateTrayTooltip() {

    const auto status = BatteryMonitor::Query();

    std::wstring tooltip = BatteryMonitor::FormatTooltip(status);

    const auto& config = Config::Instance().Get();

    tooltip += L" | Bajo ";

    tooltip += std::to_wstring(config.lowThresholdPercent);

    tooltip += L"% / Alto ";

    tooltip += std::to_wstring(config.highThresholdPercent);

    tooltip += L"%";

    if (activeAlert_ != AlertKind::None) {

        tooltip += L" | AVISO ACTIVO";

    }

    tray_.UpdateTooltip(tooltip);

}



void App::OnTimer() {

    EvaluateBatteryLevel();

    UpdateTrayTooltip();

}



void App::EvaluateBatteryLevel() {

    const auto status = BatteryMonitor::Query();

    if (!status.valid || status.percent < 0) {

        return;

    }



    const auto& config = Config::Instance().Get();



    if (status.isOnAcPower && status.percent >= config.highThresholdPercent) {

        if (!highThresholdReached_) {

            highThresholdReached_ = true;

            TriggerHighChargeAlert();

        }

    } else {

        highThresholdReached_ = false;

        if (activeAlert_ == AlertKind::HighCharge) {

            StopAlert();

        }

    }



    if (!status.isOnAcPower && status.percent <= config.lowThresholdPercent) {

        if (!lowThresholdReached_) {

            lowThresholdReached_ = true;

            TriggerLowBatteryAlert();

        }

    } else {

        lowThresholdReached_ = false;

        if (activeAlert_ == AlertKind::LowBattery) {

            StopAlert();

        }

    }

}



void App::TriggerHighChargeAlert() {

    if (activeAlert_ != AlertKind::None) {

        return;

    }



    activeAlert_ = AlertKind::HighCharge;

    const auto& config = Config::Instance().Get();

    audio_.Play(config.soundPath, config.soundLoop);



    std::wstring message = L"La bateria ha alcanzado el ";

    message += std::to_wstring(config.highThresholdPercent);

    message += L"%. Puedes desenchufar el cargador";



    const auto& brandInfo = GetBrandInfo(config.laptopBrand);

    if (brandInfo.controlLevel != ChargeControlLevel::None) {

        message += L" o usar 'Parar carga' desde el menu de la bandeja.";

    } else {

        message += L".";

    }



    tray_.ShowBalloon(L"Umbral alto de carga alcanzado", message);



    if (config.autoApplyChargeLimit &&

        GetBrandInfo(config.laptopBrand).controlLevel != ChargeControlLevel::None) {

        const auto chargeResult =

            ChargeControl::TryStopCharging(config.laptopBrand, config.highThresholdPercent);

        tray_.ShowBalloon(chargeResult.success ? L"Limite de carga aplicado"

                                               : L"Limite de carga no aplicado",

                          chargeResult.message);

    }

}



void App::TriggerLowBatteryAlert() {

    if (activeAlert_ != AlertKind::None) {

        return;

    }



    activeAlert_ = AlertKind::LowBattery;

    const auto& config = Config::Instance().Get();

    audio_.Play(config.soundPath, config.soundLoop);



    std::wstring message = L"La bateria ha bajado al ";

    message += std::to_wstring(config.lowThresholdPercent);

    message += L"%. Conecta el cargador.";



    tray_.ShowBalloon(L"Umbral bajo de descarga alcanzado", message);

}



void App::StopAlert() {

    audio_.Stop();

    activeAlert_ = AlertKind::None;

    UpdateTrayTooltip();

}



void App::StopAlertSound() {

    StopAlert();

    lowThresholdReached_ = false;

    highThresholdReached_ = false;

    tray_.ShowBalloon(L"Aviso detenido", L"El sonido de alerta se ha detenido.");

}



void App::OpenConfig() {

    auto& config = Config::Instance().Get();

    if (ShowConfigDialog(hwnd_, config) == IDOK) {

        Config::Instance().Save();

        lowThresholdReached_ = false;

        highThresholdReached_ = false;

        StopAlert();

        UpdateTrayTooltip();

    }

}



void App::OpenAbout() {

    ShowAboutDialog(hwnd_);

}



void App::AttemptStopCharging() {

    const auto& config = Config::Instance().Get();

    const auto result =

        ChargeControl::TryStopCharging(config.laptopBrand, config.highThresholdPercent);



    tray_.ShowBalloon(result.success ? L"Carga / modo salud" : L"No se pudo aplicar",

                      result.message);

    StopAlert();

}



void App::OnTrayCommand(UINT commandId) {

    switch (commandId) {

        case IDM_TRAY_OPEN_CONFIG:

            OpenConfig();

            break;

        case IDM_TRAY_STOP_CHARGE:

            AttemptStopCharging();

            break;

        case IDM_TRAY_STOP_ALERT:

            StopAlertSound();

            break;

        case IDM_TRAY_ABOUT:

            OpenAbout();

            break;

        case IDM_TRAY_EXIT:

            PostMessageW(hwnd_, WM_CLOSE, 0, 0);

            break;

    }

}



void App::OnTrayCallback(WPARAM lParam) {

    switch (LOWORD(lParam)) {

        case WM_LBUTTONDBLCLK:

            OpenConfig();

            break;

        case WM_RBUTTONUP:

            tray_.ShowContextMenu();

            break;

    }

}



int App::Run(HINSTANCE instance) {

    const HANDLE mutex = CreateMutexW(nullptr, TRUE, kMutexName);

    if (GetLastError() == ERROR_ALREADY_EXISTS) {

        ShowMessageBoxCentered(nullptr,

                               L"VidaUtil de la Bateria ya esta en ejecucion.\nBusca el icono en la bandeja "

                               L"del sistema.",

                               L"VidaUtil de la Bateria", MB_ICONINFORMATION | MB_OK);

        if (mutex) {

            CloseHandle(mutex);

        }

        return 0;

    }



    INITCOMMONCONTROLSEX controls{};

    controls.dwSize = sizeof(controls);

    controls.dwICC = ICC_UPDOWN_CLASS;

    InitCommonControlsEx(&controls);



    HRESULT comHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    const bool comInitialized = SUCCEEDED(comHr);



    if (!Initialize(instance)) {

        ShowMessageBoxCentered(nullptr, L"No se pudo iniciar la aplicacion.", L"Error",

                               MB_ICONERROR | MB_OK);

        if (comInitialized) {

            CoUninitialize();

        }

        if (mutex) {

            CloseHandle(mutex);

        }

        return 1;

    }



    if (!Config::Instance().Get().firstRunComplete) {

        OpenConfig();

        Config::Instance().Get().firstRunComplete = true;

        Config::Instance().Save();

    }



    MSG msg{};

    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {

        if (!IsDialogMessageW(GetActiveWindow(), &msg)) {

            TranslateMessage(&msg);

            DispatchMessageW(&msg);

        }

    }



    Shutdown();

    if (comInitialized) {

        CoUninitialize();

    }

    if (mutex) {

        CloseHandle(mutex);

    }

    return static_cast<int>(msg.wParam);

}


