#include "Config.hpp"

#include "WindowsCommon.hpp"
#include <ShlObj.h>
#include <Shlwapi.h>

#include <fstream>
#include <optional>
#include <regex>
#include <sstream>

namespace {

constexpr wchar_t kAppName[] = L"VidaUtilBateria";
constexpr wchar_t kConfigFileName[] = L"config.json";
constexpr wchar_t kLegacyConfigFileName[] = L"config.ini";
constexpr wchar_t kRunKeyPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int size =
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0,
                            nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(),
                        size, nullptr, nullptr);
    return result;
}

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
                                         nullptr, 0);
    if (size <= 0) {
        return {};
    }
    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(),
                        size);
    return result;
}

std::wstring ReadTextFileUtf8(const std::wstring& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        return {};
    }
    std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (bytes.size() >= 3 &&
        static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF) {
        bytes.erase(0, 3);
    }
    return Utf8ToWide(bytes);
}

bool WriteTextFileUtf8(const std::wstring& filePath, const std::wstring& content) {
    const std::string bytes = WideToUtf8(content);
    std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(file);
}

std::wstring JsonEscape(const std::wstring& value) {
    std::wstring escaped;
    escaped.reserve(value.size() + 8);
    for (wchar_t ch : value) {
        switch (ch) {
            case L'\\':
                escaped += L"\\\\";
                break;
            case L'"':
                escaped += L"\\\"";
                break;
            case L'\n':
                escaped += L"\\n";
                break;
            case L'\r':
                escaped += L"\\r";
                break;
            case L'\t':
                escaped += L"\\t";
                break;
            default:
                escaped += ch;
                break;
        }
    }
    return escaped;
}

std::wstring JsonUnescape(const std::wstring& value) {
    std::wstring unescaped;
    unescaped.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == L'\\' && i + 1 < value.size()) {
            switch (value[i + 1]) {
                case L'\\':
                    unescaped += L'\\';
                    break;
                case L'"':
                    unescaped += L'"';
                    break;
                case L'n':
                    unescaped += L'\n';
                    break;
                case L'r':
                    unescaped += L'\r';
                    break;
                case L't':
                    unescaped += L'\t';
                    break;
                default:
                    unescaped += value[i + 1];
                    break;
            }
            ++i;
        } else {
            unescaped += value[i];
        }
    }
    return unescaped;
}

std::wstring ReadIniValue(const std::wstring& filePath, const std::wstring& section,
                          const std::wstring& key, const std::wstring& defaultValue) {
    wchar_t buffer[1024] = {};
    GetPrivateProfileStringW(section.c_str(), key.c_str(), defaultValue.c_str(), buffer,
                             static_cast<DWORD>(std::size(buffer)), filePath.c_str());
    return buffer;
}

void ClampThreshold(int& value) {
    if (value < 1) {
        value = 1;
    } else if (value > 100) {
        value = 100;
    }
}

std::optional<std::wstring> ExtractJsonString(const std::wstring& json, const std::wstring& key) {
    const std::wregex pattern(L"\"" + key + L"\"\\s*:\\s*\"((?:\\\\.|[^\"\\\\])*)\"");
    std::wsmatch match;
    if (std::regex_search(json, match, pattern) && match.size() > 1) {
        return JsonUnescape(match[1].str());
    }
    return std::nullopt;
}

std::optional<int> ExtractJsonInt(const std::wstring& json, const std::wstring& key) {
    const std::wregex pattern(L"\"" + key + L"\"\\s*:\\s*(-?[0-9]+)");
    std::wsmatch match;
    if (std::regex_search(json, match, pattern) && match.size() > 1) {
        return _wtoi(match[1].str().c_str());
    }
    return std::nullopt;
}

std::optional<bool> ExtractJsonBool(const std::wstring& json, const std::wstring& key) {
    const std::wregex pattern(L"\"" + key + L"\"\\s*:\\s*(true|false)");
    std::wsmatch match;
    if (std::regex_search(json, match, pattern) && match.size() > 1) {
        return match[1].str() == L"true";
    }
    return std::nullopt;
}

}  // namespace

Config& Config::Instance() {
    static Config instance;
    return instance;
}

std::wstring Config::GetConfigDirectory() const {
    wchar_t path[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path))) {
        return L".";
    }

    std::wstring directory = path;
    directory += L"\\";
    directory += kAppName;
    CreateDirectoryW(directory.c_str(), nullptr);
    return directory;
}

std::wstring Config::GetConfigFilePath() const {
    return GetConfigDirectory() + L"\\" + kConfigFileName;
}

void Config::Load() {
    const auto filePath = GetConfigFilePath();
    const auto legacyPath = GetConfigDirectory() + L"\\" + kLegacyConfigFileName;

    if (PathFileExistsW(filePath.c_str())) {
        const std::wstring json = ReadTextFileUtf8(filePath);

        if (const auto value = ExtractJsonInt(json, L"lowThresholdPercent")) {
            config_.lowThresholdPercent = *value;
        }
        if (const auto value = ExtractJsonInt(json, L"highThresholdPercent")) {
            config_.highThresholdPercent = *value;
        }
        if (const auto legacyThreshold = ExtractJsonInt(json, L"thresholdPercent")) {
            config_.highThresholdPercent = *legacyThreshold;
        }
        if (const auto value = ExtractJsonString(json, L"soundPath")) {
            config_.soundPath = *value;
        }
        if (const auto value = ExtractJsonBool(json, L"soundLoop")) {
            config_.soundLoop = *value;
        }
        if (const auto value = ExtractJsonBool(json, L"startWithWindows")) {
            config_.startWithWindows = *value;
        }
        if (const auto value = ExtractJsonBool(json, L"autoApplyChargeLimit")) {
            config_.autoApplyChargeLimit = *value;
        }
        if (const auto value = ExtractJsonBool(json, L"firstRunComplete")) {
            config_.firstRunComplete = *value;
        }
        if (const auto value = ExtractJsonInt(json, L"laptopBrand")) {
            if (*value >= 0 && *value <= static_cast<int>(LaptopBrand::Samsung)) {
                config_.laptopBrand = static_cast<LaptopBrand>(*value);
            }
        }
    } else if (PathFileExistsW(legacyPath.c_str())) {
        config_.highThresholdPercent =
            _wtoi(ReadIniValue(legacyPath, L"General", L"Threshold", L"80").c_str());
        config_.lowThresholdPercent = 20;
        config_.soundPath = ReadIniValue(legacyPath, L"General", L"SoundPath", L"");
        config_.soundLoop = ReadIniValue(legacyPath, L"General", L"SoundLoop", L"1") == L"1";
        config_.startWithWindows =
            ReadIniValue(legacyPath, L"General", L"StartWithWindows", L"1") == L"1";
        config_.autoApplyChargeLimit =
            ReadIniValue(legacyPath, L"General", L"AutoApplyChargeLimit", L"0") == L"1";
        config_.firstRunComplete =
            ReadIniValue(legacyPath, L"General", L"FirstRunComplete", L"0") == L"1";

        const int brandValue =
            _wtoi(ReadIniValue(legacyPath, L"General", L"LaptopBrand", L"1").c_str());
        if (brandValue >= 0 && brandValue <= static_cast<int>(LaptopBrand::Samsung)) {
            config_.laptopBrand = static_cast<LaptopBrand>(brandValue);
        }

        Save();
    } else {
        Save();
    }

    ClampThreshold(config_.lowThresholdPercent);
    ClampThreshold(config_.highThresholdPercent);
    if (config_.lowThresholdPercent >= config_.highThresholdPercent) {
        config_.lowThresholdPercent = 20;
        config_.highThresholdPercent = 80;
    }
}

void Config::Save() const {
    const auto filePath = GetConfigFilePath();

    std::wstring json;
    json += L"{\n";
    json += L"  \"lowThresholdPercent\": ";
    json += std::to_wstring(config_.lowThresholdPercent);
    json += L",\n";
    json += L"  \"highThresholdPercent\": ";
    json += std::to_wstring(config_.highThresholdPercent);
    json += L",\n";
    json += L"  \"soundPath\": \"";
    json += JsonEscape(config_.soundPath);
    json += L"\",\n";
    json += L"  \"soundLoop\": ";
    json += config_.soundLoop ? L"true" : L"false";
    json += L",\n";
    json += L"  \"startWithWindows\": ";
    json += config_.startWithWindows ? L"true" : L"false";
    json += L",\n";
    json += L"  \"autoApplyChargeLimit\": ";
    json += config_.autoApplyChargeLimit ? L"true" : L"false";
    json += L",\n";
    json += L"  \"firstRunComplete\": ";
    json += config_.firstRunComplete ? L"true" : L"false";
    json += L",\n";
    json += L"  \"laptopBrand\": ";
    json += std::to_wstring(static_cast<int>(config_.laptopBrand));
    json += L"\n";
    json += L"}\n";

    WriteTextFileUtf8(filePath, json);
    ApplyStartupRegistration(config_.startWithWindows);
}

void Config::ApplyStartupRegistration(bool enable) const {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
        return;
    }

    if (enable) {
        wchar_t modulePath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
        RegSetValueExW(key, kAppName, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(modulePath),
                       static_cast<DWORD>((wcslen(modulePath) + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(key, kAppName);
    }

    RegCloseKey(key);
}

bool Config::IsStartupRegistered() const {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return false;
    }

    wchar_t buffer[MAX_PATH] = {};
    DWORD size = sizeof(buffer);
    const LSTATUS status = RegQueryValueExW(key, kAppName, nullptr, nullptr,
                                            reinterpret_cast<LPBYTE>(buffer), &size);
    RegCloseKey(key);
    return status == ERROR_SUCCESS;
}
