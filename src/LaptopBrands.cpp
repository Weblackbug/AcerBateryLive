#include "LaptopBrands.hpp"

#include <iterator>

namespace {

constexpr BrandInfo kBrands[] = {
    {LaptopBrand::Generic, L"Generico / Otra marca", ChargeControlLevel::None,
     L"No compatible con parada automatica de carga por software. Solo aviso sonoro; "
     L"desenchufa el cargador manualmente.",
     L"Desenchufa el cargador manualmente."},
    {LaptopBrand::Acer,
     L"Acer",
     ChargeControlLevel::FixedLimitOem,
     L"Parcialmente compatible. En modelos con firmware Acer WMI se puede activar el "
     L"modo salud (limite fijo ~80%%) via interfaz de hardware. No permite un %% "
     L"personalizado como los moviles.",
     L"Intentar activar modo salud 80%% (Acer WMI) o usar Acer Care Center."},
    {LaptopBrand::Lenovo,
     L"Lenovo",
     ChargeControlLevel::CustomThreshold,
     L"Compatible con umbral personalizable mediante Lenovo Vantage o "
     L"batteryChargeThreshold.exe (segun modelo).",
     L"Configura el umbral en Lenovo Vantage o usa la herramienta CLI de Lenovo."},
    {LaptopBrand::Dell,
     L"Dell",
     ChargeControlLevel::CustomThreshold,
     L"Compatible con Dell Power Manager / dellcommandpower-manager en modelos "
     L"soportados.",
     L"Usa Dell Power Manager para fijar el limite de carga."},
    {LaptopBrand::HP,
     L"HP",
     ChargeControlLevel::FixedLimitOem,
     L"Parcialmente compatible via HP Command Center o BIOS (limite fijo, no %% "
     L"libre en todos los modelos).",
     L"Abre HP Command Center o configura Battery Health Manager en BIOS."},
    {LaptopBrand::Asus,
     L"ASUS",
     ChargeControlLevel::FixedLimitOem,
     L"Parcialmente compatible via MyASUS (limite tipico 80%% en modelos soportados).",
     L"Activa Battery Care Mode en MyASUS."},
    {LaptopBrand::Framework,
     L"Framework",
     ChargeControlLevel::CustomThreshold,
     L"Compatible con framework_tool --charge-limit N (umbral personalizable).",
     L"Ejecuta: framework_tool --charge-limit <porcentaje>"},
    {LaptopBrand::MSI,
     L"MSI",
     ChargeControlLevel::FixedLimitOem,
     L"Parcialmente compatible via MSI Center / Dragon Center (Best for Battery, ~80%%).",
     L"Activa Best for Battery en MSI Center."},
    {LaptopBrand::Samsung,
     L"Samsung",
     ChargeControlLevel::FixedLimitOem,
     L"Parcialmente compatible via Samsung Battery Life Extender (limite fijo ~80%%).",
     L"Activa Battery Life Extender en la app de Samsung."},
};

}  // namespace

const BrandInfo& GetBrandInfo(LaptopBrand brand) {
    for (const auto& info : kBrands) {
        if (info.brand == brand) {
            return info;
        }
    }
    return kBrands[0];
}

int GetBrandCount() {
    return static_cast<int>(std::size(kBrands));
}

LaptopBrand BrandFromIndex(int index) {
    if (index < 0 || index >= GetBrandCount()) {
        return LaptopBrand::Generic;
    }
    return kBrands[index].brand;
}

int IndexFromBrand(LaptopBrand brand) {
    for (int i = 0; i < GetBrandCount(); ++i) {
        if (kBrands[i].brand == brand) {
            return i;
        }
    }
    return 0;
}
