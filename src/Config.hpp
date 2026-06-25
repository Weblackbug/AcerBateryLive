#pragma once

#include "WindowsCommon.hpp"
#include <string>

#include "LaptopBrands.hpp"

struct AppConfig {
    int lowThresholdPercent = 20;
    int highThresholdPercent = 80;
    std::wstring soundPath;
    bool soundLoop = true;
    bool startWithWindows = true;
    bool autoApplyChargeLimit = false;
    bool firstRunComplete = false;
    LaptopBrand laptopBrand = LaptopBrand::Acer;
};

class Config {
public:
    static Config& Instance();

    void Load();
    void Save() const;

    AppConfig& Get() { return config_; }
    const AppConfig& Get() const { return config_; }

    std::wstring GetConfigDirectory() const;
    std::wstring GetConfigFilePath() const;

    void ApplyStartupRegistration(bool enable) const;
    bool IsStartupRegistered() const;

private:
    Config() = default;
    AppConfig config_;
};
