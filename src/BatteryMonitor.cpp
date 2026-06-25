#include "BatteryMonitor.hpp"

BatteryMonitor::Status BatteryMonitor::Query() {
    Status status{};

    SYSTEM_POWER_STATUS powerStatus{};
    if (!GetSystemPowerStatus(&powerStatus)) {
        return status;
    }

    status.valid = true;
    status.percent = static_cast<int>(powerStatus.BatteryLifePercent);
    if (status.percent > 100) {
        status.percent = -1;
    }

    status.isOnAcPower = powerStatus.ACLineStatus == 1;
    status.isCharging = status.isOnAcPower && (powerStatus.BatteryFlag & 8) != 0;
    status.isPluggedNotCharging =
        status.isOnAcPower && !status.isCharging && status.percent >= 0 &&
        status.percent < 100;

    return status;
}

std::wstring BatteryMonitor::FormatTooltip(const Status& status) {
    if (!status.valid || status.percent < 0) {
        return L"VidaUtil de la Bateria - Sin datos";
    }

    std::wstring text = L"VidaUtil de la Bateria - ";
    text += std::to_wstring(status.percent);
    text += L"%";

    if (status.isCharging) {
        text += L" (Cargando)";
    } else if (status.isPluggedNotCharging) {
        text += L" (Conectado, sin cargar)";
    } else if (status.isOnAcPower) {
        text += L" (Conectado)";
    } else {
        text += L" (Bateria)";
    }

    return text;
}
