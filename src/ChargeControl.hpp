#pragma once

#include "WindowsCommon.hpp"
#include <optional>
#include <string>
#include <vector>

#include "LaptopBrands.hpp"

struct ChargeControlResult {
    bool success = false;
    std::wstring message;
};

struct AcerWmiStatus {
    bool wmiConnected = false;
    bool interfaceFound = false;
    bool healthModeSupported = false;
    std::optional<bool> healthModeActive;
    std::wstring wmiClassName;
    std::vector<std::wstring> discoveredMethods;
    std::wstring summary;
};

class ChargeControl {
public:
    static ChargeControlResult TryStopCharging(LaptopBrand brand, int thresholdPercent);
    static ChargeControlResult TryEnableOemLimit(LaptopBrand brand, int thresholdPercent);
    static AcerWmiStatus ProbeAcerWmi();
    static bool IsAcerWmiAvailable();
    static ChargeControlResult SetAcerHealthMode(bool enable);
    static std::wstring FormatAcerStatusText(const AcerWmiStatus& status);
};
