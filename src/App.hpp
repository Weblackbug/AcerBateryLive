#pragma once

#include "WindowsCommon.hpp"

#include "AudioPlayer.hpp"
#include "Config.hpp"
#include "TrayIcon.hpp"

class App {
public:
    static App& Instance();

    int Run(HINSTANCE instance);

private:
    enum class AlertKind { None, LowBattery, HighCharge };

    App() = default;

    bool Initialize(HINSTANCE instance);
    void Shutdown();
    void OnTimer();
    void OnTrayCommand(UINT commandId);
    void OnTrayCallback(WPARAM lParam);
    void EvaluateBatteryLevel();
    void TriggerHighChargeAlert();
    void TriggerLowBatteryAlert();
    void StopAlert();
    void OpenConfig();
    void OpenAbout();
    void AttemptStopCharging();
    void StopAlertSound();
    void UpdateTrayTooltip();

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    UINT_PTR timerId_ = 0;
    AlertKind activeAlert_ = AlertKind::None;
    bool lowThresholdReached_ = false;
    bool highThresholdReached_ = false;

    AudioPlayer audio_;
    TrayIcon tray_;
};
