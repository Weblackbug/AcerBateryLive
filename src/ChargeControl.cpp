#include "ChargeControl.hpp"

#include "WindowsCommon.hpp"
#include <Shlwapi.h>
#include <Wbemidl.h>

#include <comdef.h>
#include <optional>
#include <sstream>
#include <vector>

#pragma comment(lib, "wbemuuid.lib")

namespace {

constexpr wchar_t kAcerWmiGuidUnderscore[] = L"79772EC5_04B1_4bfd_843C_61E7F77B6CC9";
constexpr wchar_t kAcerWmiGuidHyphen[] = L"79772EC5-04B1-4bfd-843C-61E7F77B6CC9";
constexpr uint8_t kAcerBatteryIndex = 0x01;
constexpr uint8_t kHealthMode = 0x01;

const wchar_t* kCandidateClassNames[] = {
    L"BatteryControl",
    L"AcerGenericMethod",
    kAcerWmiGuidUnderscore,
    kAcerWmiGuidHyphen,
    L"AcerBatteryHealthControl",
};

const wchar_t* kCandidateMethodNames[] = {
    L"SetBatteryHealthControl",
    L"SetHealthMode",
    L"WMIMethodID21",
    L"Write",
};

struct WmiSession {
    IWbemLocator* locator = nullptr;
    IWbemServices* services = nullptr;

    ~WmiSession() {
        if (services) {
            services->Release();
        }
        if (locator) {
            locator->Release();
        }
    }
};

bool ConnectWmiRoot(WmiSession& session) {
    if (CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator,
                         reinterpret_cast<void**>(&session.locator)) != S_OK) {
        return false;
    }

    BSTR namespacePath = SysAllocString(L"ROOT\\WMI");
    const HRESULT hr = session.locator->ConnectServer(namespacePath, nullptr, nullptr, nullptr, 0,
                                                      nullptr, nullptr, &session.services);
    SysFreeString(namespacePath);

    if (FAILED(hr) || !session.services) {
        return false;
    }

    CoSetProxyBlanket(session.services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                      RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
    return true;
}

bool FileExists(const std::wstring& path) {
    return !path.empty() && PathFileExistsW(path.c_str()) == TRUE;
}

bool LaunchFirstExisting(const std::vector<std::wstring>& paths) {
    for (const auto& path : paths) {
        if (FileExists(path)) {
            HINSTANCE result =
                ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            if (reinterpret_cast<INT_PTR>(result) > 32) {
                return true;
            }
        }
    }
    return false;
}

ChargeControlResult LaunchAcerCareApps() {
    const std::vector<std::wstring> paths = {
        L"C:\\Program Files\\Acer\\Acer Care Center\\Care Center.exe",
        L"C:\\Program Files (x86)\\Acer\\Acer Care Center\\Care Center.exe",
        L"C:\\Program Files\\Acer\\AcerSense\\AcerSense.exe",
        L"C:\\Program Files (x86)\\Acer\\AcerSense\\AcerSense.exe",
        L"C:\\Program Files\\Acer\\Quick Access Service\\QAS.exe",
        L"C:\\Program Files\\Acer\\Acer System Manager\\System Manager.exe",
    };

    if (LaunchFirstExisting(paths)) {
        return {true,
                L"Se abrio la aplicacion Acer.\nVe a Checkup > Battery Charge Limit y activalo "
                L"(limite ~80%%)."};
    }

    return {false,
            L"No se encontro Acer Care Center / AcerSense.\nBuscalos en el menu Inicio y activa "
            L"'Battery Charge Limit'."};
}

bool ClassExists(IWbemServices* services, const wchar_t* className) {
    IEnumWbemClassObject* enumerator = nullptr;
    const std::wstring query = L"SELECT * FROM " + std::wstring(className);
    BSTR language = SysAllocString(L"WQL");
    BSTR queryBstr = SysAllocString(query.c_str());

    const HRESULT hr = services->ExecQuery(
        language, queryBstr, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr,
        &enumerator);
    SysFreeString(language);
    SysFreeString(queryBstr);

    if (FAILED(hr) || !enumerator) {
        return false;
    }

    IWbemClassObject* object = nullptr;
    ULONG returned = 0;
    const HRESULT nextHr = enumerator->Next(WBEM_INFINITE, 1, &object, &returned);
    if (object) {
        object->Release();
    }
    enumerator->Release();
    return nextHr == S_OK && returned > 0;
}

std::wstring FindAcerClassName(IWbemServices* services) {
    for (const auto* candidate : kCandidateClassNames) {
        if (ClassExists(services, candidate)) {
            return candidate;
        }
    }

    IEnumWbemClassObject* enumerator = nullptr;
    BSTR language = SysAllocString(L"WQL");
    BSTR query = SysAllocString(L"SELECT __CLASS FROM meta_class WHERE __CLASS LIKE '%79772%'");

    if (SUCCEEDED(services->ExecQuery(language, query,
                                      WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                                      nullptr, &enumerator))) {
        while (enumerator) {
            IWbemClassObject* object = nullptr;
            ULONG returned = 0;
            if (enumerator->Next(WBEM_INFINITE, 1, &object, &returned) != S_OK ||
                returned == 0) {
                break;
            }

            VARIANT classVar{};
            if (SUCCEEDED(object->Get(L"__CLASS", 0, &classVar, nullptr, nullptr)) &&
                classVar.vt == VT_BSTR && classVar.bstrVal != nullptr) {
                std::wstring found = classVar.bstrVal;
                VariantClear(&classVar);
                object->Release();
                enumerator->Release();
                SysFreeString(language);
                SysFreeString(query);
                return found;
            }
            VariantClear(&classVar);
            object->Release();
        }
        enumerator->Release();
    }

    SysFreeString(language);
    SysFreeString(query);
    return L"";
}

void CollectMethodNames(IWbemServices* services, const std::wstring& className,
                        std::vector<std::wstring>& methodsOut) {
    IWbemClassObject* classObject = nullptr;
    BSTR classBstr = SysAllocString(className.c_str());
    if (services->GetObject(classBstr, WBEM_FLAG_PROTOTYPE, nullptr, &classObject, nullptr) !=
        WBEM_S_NO_ERROR) {
        SysFreeString(classBstr);
        return;
    }

    SAFEARRAY* names = nullptr;
    if (SUCCEEDED(classObject->GetNames(L"", WBEM_FLAG_NONSYSTEM_ONLY, nullptr, &names)) &&
        names) {
        LONG lower = 0;
        LONG upper = -1;
        SafeArrayGetLBound(names, 1, &lower);
        SafeArrayGetUBound(names, 1, &upper);
        for (LONG i = lower; i <= upper; ++i) {
            BSTR methodName = nullptr;
            if (SUCCEEDED(SafeArrayGetElement(names, &i, &methodName)) && methodName) {
                methodsOut.emplace_back(methodName);
                SysFreeString(methodName);
            }
        }
        SafeArrayDestroy(names);
    }

    classObject->Release();
    SysFreeString(classBstr);
}

bool PutByte(IWbemClassObject* params, const wchar_t* name, uint8_t value) {
    VARIANT var{};
    var.vt = VT_UI1;
    var.bVal = value;
    return SUCCEEDED(params->Put(name, 0, &var, 0));
}

bool TryExecHealthMethod(IWbemServices* services, const std::wstring& className,
                         const wchar_t* methodName, bool enable, std::wstring& errorOut) {
    IWbemClassObject* classObject = nullptr;
    BSTR classBstr = SysAllocString(className.c_str());
    BSTR methodBstr = SysAllocString(methodName);
    if (services->GetObject(classBstr, 0, nullptr, &classObject, nullptr) != WBEM_S_NO_ERROR) {
        SysFreeString(methodBstr);
        SysFreeString(classBstr);
        return false;
    }

    IWbemClassObject* inSignature = nullptr;
    if (classObject->GetMethod(methodBstr, 0, &inSignature, nullptr) != WBEM_S_NO_ERROR) {
        SysFreeString(methodBstr);
        SysFreeString(classBstr);
        classObject->Release();
        return false;
    }

    IWbemClassObject* inParams = nullptr;
    if (!inSignature || inSignature->SpawnInstance(0, &inParams) != WBEM_S_NO_ERROR) {
        if (inSignature) {
            inSignature->Release();
        }
        SysFreeString(methodBstr);
        SysFreeString(classBstr);
        classObject->Release();
        return false;
    }

    PutByte(inParams, L"uBatteryNo", kAcerBatteryIndex);
    PutByte(inParams, L"uFunctionMask", kHealthMode);
    PutByte(inParams, L"uFunctionStatus", enable ? 1 : 0);

    IWbemClassObject* outParams = nullptr;
    const HRESULT hr =
        services->ExecMethod(classBstr, methodBstr, 0, nullptr, inParams, &outParams, nullptr);

    if (outParams) {
        outParams->Release();
    }
    inParams->Release();
    inSignature->Release();
    classObject->Release();
    SysFreeString(methodBstr);
    SysFreeString(classBstr);

    if (FAILED(hr)) {
        _com_error err(hr);
        errorOut = err.ErrorMessage();
        return false;
    }

    return true;
}

bool TrySetAcerHealthModeInternal(IWbemServices* services, const std::wstring& className,
                                  bool enable, std::wstring& errorOut) {
    for (const auto* methodName : kCandidateMethodNames) {
        if (TryExecHealthMethod(services, className, methodName, enable, errorOut)) {
            return true;
        }
    }
    return false;
}

std::optional<bool> QueryAcerHealthModeActive(IWbemServices* services,
                                              const std::wstring& className) {
    IWbemClassObject* classObject = nullptr;
    BSTR classBstr = SysAllocString(className.c_str());
    BSTR methodName = SysAllocString(L"GetBatteryHealthControlStatus");
    if (services->GetObject(classBstr, 0, nullptr, &classObject, nullptr) != WBEM_S_NO_ERROR) {
        SysFreeString(methodName);
        SysFreeString(classBstr);
        return std::nullopt;
    }

    IWbemClassObject* inSignature = nullptr;
    if (classObject->GetMethod(methodName, 0, &inSignature, nullptr) != WBEM_S_NO_ERROR) {
        SysFreeString(methodName);
        SysFreeString(classBstr);
        classObject->Release();
        return std::nullopt;
    }

    IWbemClassObject* inParams = nullptr;
    if (!inSignature || inSignature->SpawnInstance(0, &inParams) != WBEM_S_NO_ERROR) {
        if (inSignature) {
            inSignature->Release();
        }
        SysFreeString(methodName);
        SysFreeString(classBstr);
        classObject->Release();
        return std::nullopt;
    }

    PutByte(inParams, L"uBatteryNo", kAcerBatteryIndex);
    PutByte(inParams, L"uFunctionQuery", 0x01);

    IWbemClassObject* outParams = nullptr;
    const HRESULT hr =
        services->ExecMethod(classBstr, methodName, 0, nullptr, inParams, &outParams, nullptr);

    std::optional<bool> active;
    if (SUCCEEDED(hr) && outParams) {
        VARIANT var{};
        if (SUCCEEDED(outParams->Get(L"uFunctionStatus", 0, &var, nullptr, nullptr))) {
            if (var.vt == VT_UI1) {
                active = var.bVal > 0;
            } else if (var.vt == (VT_ARRAY | VT_UI1) && var.parray) {
                uint8_t first = 0;
                LONG index = 0;
                SafeArrayGetElement(var.parray, &index, &first);
                active = first > 0;
            }
            VariantClear(&var);
        }
    }

    if (outParams) {
        outParams->Release();
    }
    inParams->Release();
    inSignature->Release();
    classObject->Release();
    SysFreeString(methodName);
    SysFreeString(classBstr);
    return active;
}

bool RunHiddenCommand(const std::wstring& executable, const std::wstring& args) {
    std::wstring commandLine = L"\"" + executable + L"\" " + args;
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};

    std::vector<wchar_t> buffer(commandLine.begin(), commandLine.end());
    buffer.push_back(L'\0');

    if (!CreateProcessW(nullptr, buffer.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                        nullptr, &si, &pi)) {
        return false;
    }

    WaitForSingleObject(pi.hProcess, 15000);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exitCode == 0;
}

ChargeControlResult TryLenovoThreshold(int thresholdPercent) {
    const std::vector<std::wstring> tools = {
        L"C:\\Program Files\\Lenovo\\Power Manager\\batteryChargeThreshold.exe",
        L"C:\\Program Files (x86)\\Lenovo\\Power Manager\\batteryChargeThreshold.exe",
    };

    for (const auto& tool : tools) {
        if (FileExists(tool)) {
            const std::wstring args =
                L"/setStopThreshold " + std::to_wstring(thresholdPercent);
            if (RunHiddenCommand(tool, args)) {
                return {true, L"Lenovo: limite de carga fijado al " +
                                   std::to_wstring(thresholdPercent) + L"%."};
            }
        }
    }
    return {false, L"Instala Lenovo Vantage o batteryChargeThreshold.exe."};
}

ChargeControlResult TryFrameworkLimit(int thresholdPercent) {
    const std::vector<std::wstring> tools = {
        L"C:\\Program Files\\Framework\\framework_tool\\framework_tool.exe",
        L"C:\\Program Files (x86)\\Framework\\framework_tool\\framework_tool.exe",
    };

    for (const auto& tool : tools) {
        if (FileExists(tool)) {
            const std::wstring args =
                L"--charge-limit " + std::to_wstring(thresholdPercent);
            if (RunHiddenCommand(tool, args)) {
                return {true, L"Framework: limite de carga fijado al " +
                                   std::to_wstring(thresholdPercent) + L"%."};
            }
        }
    }
    return {false, L"Instala framework_tool: winget install framework_tool"};
}

}  // namespace

AcerWmiStatus ChargeControl::ProbeAcerWmi() {
    AcerWmiStatus status{};
    WmiSession session;
    status.wmiConnected = ConnectWmiRoot(session);
    if (!status.wmiConnected) {
        status.summary = L"WMI hardware no accesible.";
        return status;
    }

    status.wmiClassName = FindAcerClassName(session.services);
    status.interfaceFound = !status.wmiClassName.empty();
    if (!status.interfaceFound) {
        status.summary = L"Interfaz Acer no encontrada en ROOT\\WMI.";
        return status;
    }

    CollectMethodNames(session.services, status.wmiClassName, status.discoveredMethods);
    status.healthModeSupported = !status.discoveredMethods.empty();
    status.healthModeActive = QueryAcerHealthModeActive(session.services, status.wmiClassName);

    std::wstringstream ss;
    ss << L"Clase WMI: " << status.wmiClassName;
    if (status.healthModeActive.has_value()) {
        ss << L" | Modo salud: " << (*status.healthModeActive ? L"ACTIVO" : L"inactivo");
    } else {
        ss << L" | Modo salud: desconocido";
    }
    status.summary = ss.str();
    return status;
}

std::wstring ChargeControl::FormatAcerStatusText(const AcerWmiStatus& status) {
    if (!status.wmiConnected) {
        return L"WMI Acer: no accesible. Usa Acer Care Center.";
    }
    if (!status.interfaceFound) {
        return L"WMI Acer: interfaz no detectada en este modelo. Usa Acer Care Center.";
    }

    std::wstring text = L"WMI Acer: SI (" + status.wmiClassName + L")";
    if (status.healthModeActive.has_value()) {
        text += *status.healthModeActive ? L" | Modo salud ACTIVO (~80%%)"
                                         : L" | Modo salud inactivo";
    }
    return text;
}

bool ChargeControl::IsAcerWmiAvailable() {
    return ProbeAcerWmi().interfaceFound;
}

ChargeControlResult ChargeControl::SetAcerHealthMode(bool enable) {
    WmiSession session;
    if (!ConnectWmiRoot(session)) {
        return LaunchAcerCareApps();
    }

    const std::wstring className = FindAcerClassName(session.services);
    if (className.empty()) {
        return LaunchAcerCareApps();
    }

    std::wstring error;
    if (TrySetAcerHealthModeInternal(session.services, className, enable, error)) {
        return {true, enable ? L"Modo salud Acer activado (limite ~80%% en firmware)."
                             : L"Modo salud Acer desactivado."};
    }

    if (!error.empty()) {
        auto fallback = LaunchAcerCareApps();
        fallback.message = L"WMI fallo: " + error + L"\n\n" + fallback.message;
        return fallback;
    }

    return LaunchAcerCareApps();
}

ChargeControlResult ChargeControl::TryEnableOemLimit(LaptopBrand brand, int thresholdPercent) {
    switch (brand) {
        case LaptopBrand::Acer:
            return SetAcerHealthMode(true);
        case LaptopBrand::Framework:
            return TryFrameworkLimit(thresholdPercent);
        default:
            break;
    }

    const auto& info = GetBrandInfo(brand);
    return {false, info.stopChargeHint};
}

ChargeControlResult ChargeControl::TryStopCharging(LaptopBrand brand, int thresholdPercent) {
    if (thresholdPercent < 1) {
        thresholdPercent = 1;
    } else if (thresholdPercent > 100) {
        thresholdPercent = 100;
    }

    const auto& info = GetBrandInfo(brand);

    switch (info.controlLevel) {
        case ChargeControlLevel::None:
            return {false,
                    L"Tu portatil no admite parada automatica de carga por software.\n\n" +
                        std::wstring(info.stopChargeHint)};

        case ChargeControlLevel::FixedLimitOem:
            if (brand == LaptopBrand::Acer) {
                auto result = SetAcerHealthMode(true);
                if (result.success) {
                    result.message +=
                        L"\n\nNota: Acer limita a ~80%% fijo (firmware), no al " +
                        std::to_wstring(thresholdPercent) + L"% de aviso.";
                }
                return result;
            }
            return TryEnableOemLimit(brand, thresholdPercent);

        case ChargeControlLevel::CustomThreshold:
            if (brand == LaptopBrand::Lenovo) {
                return TryLenovoThreshold(thresholdPercent);
            }
            if (brand == LaptopBrand::Framework) {
                return TryFrameworkLimit(thresholdPercent);
            }
            return {false, std::wstring(info.stopChargeHint) +
                               L"\n\nConfigura el umbral (" +
                               std::to_wstring(thresholdPercent) +
                               L"%) en la herramienta del fabricante."};
    }

    return {false, L"Operacion no soportada."};
}
