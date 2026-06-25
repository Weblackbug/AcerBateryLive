#pragma once

enum class LaptopBrand {
    Generic = 0,
    Acer,
    Lenovo,
    Dell,
    HP,
    Asus,
    Framework,
    MSI,
    Samsung
};

enum class ChargeControlLevel {
    None,
    FixedLimitOem,
    CustomThreshold
};

struct BrandInfo {
    LaptopBrand brand;
    const wchar_t* displayName;
    ChargeControlLevel controlLevel;
    const wchar_t* compatibilityDescription;
    const wchar_t* stopChargeHint;
};

const BrandInfo& GetBrandInfo(LaptopBrand brand);
int GetBrandCount();
LaptopBrand BrandFromIndex(int index);
int IndexFromBrand(LaptopBrand brand);
