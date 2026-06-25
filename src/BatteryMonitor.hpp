#pragma once

#include "WindowsCommon.hpp"
#include <string>

class BatteryMonitor {
public:
    struct Status {
        int percent = -1;
        bool isCharging = false;
        bool isOnAcPower = false;
    bool valid = false;
    bool isPluggedNotCharging = false;
};

    static Status Query();
    static std::wstring FormatTooltip(const Status& status);
};
