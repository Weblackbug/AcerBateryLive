#include "ToastNotifier.hpp"

#include <ShlObj.h>
#include <ShObjIdl.h>
#include <propkey.h>
#include <propvarutil.h>

#include <winrt/base.h>

#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.UI.Notifications.h>

namespace {

constexpr wchar_t kShortcutName[] = L"VidaUtil de la Bateria.lnk";
constexpr wchar_t kToastActionStop[] = L"action=stopAlert";

std::wstring EscapeXml(const std::wstring& value) {
    std::wstring escaped;
    escaped.reserve(value.size());
    for (wchar_t ch : value) {
        switch (ch) {
            case L'&':
                escaped += L"&amp;";
                break;
            case L'<':
                escaped += L"&lt;";
                break;
            case L'>':
                escaped += L"&gt;";
                break;
            case L'"':
                escaped += L"&quot;";
                break;
            case L'\'':
                escaped += L"&apos;";
                break;
            default:
                escaped += ch;
                break;
        }
    }
    return escaped;
}

bool CreateStartMenuShortcut(const std::wstring& exePath, const std::wstring& appUserModelId) {
    wchar_t programsPath[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, SHGFP_TYPE_CURRENT, programsPath))) {
        return false;
    }

    std::wstring shortcutPath = programsPath;
    shortcutPath += L"\\";
    shortcutPath += kShortcutName;

    IShellLinkW* shellLink = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shellLink)))) {
        return false;
    }

    shellLink->SetPath(exePath.c_str());
    shellLink->SetDescription(L"VidaUtil de la Bateria");
    shellLink->SetShowCmd(SW_SHOWMINNOACTIVE);

    IPropertyStore* propertyStore = nullptr;
    if (FAILED(shellLink->QueryInterface(IID_PPV_ARGS(&propertyStore)))) {
        shellLink->Release();
        return false;
    }

    PROPVARIANT appIdValue{};
    PropVariantInit(&appIdValue);
    if (FAILED(InitPropVariantFromString(appUserModelId.c_str(), &appIdValue))) {
        propertyStore->Release();
        shellLink->Release();
        return false;
    }

    propertyStore->SetValue(PKEY_AppUserModel_ID, appIdValue);
    PropVariantClear(&appIdValue);
    propertyStore->Release();

    IPersistFile* persistFile = nullptr;
    if (FAILED(shellLink->QueryInterface(IID_PPV_ARGS(&persistFile)))) {
        shellLink->Release();
        return false;
    }

    const HRESULT saveResult = persistFile->Save(shortcutPath.c_str(), TRUE);
    persistFile->Release();
    shellLink->Release();
    return SUCCEEDED(saveResult);
}

}  // namespace

struct IToastNotifierState {
    std::wstring appUserModelId;
    winrt::Windows::UI::Notifications::ToastNotifier notifier{nullptr};
    bool initialized = false;
};

ToastNotifier::ToastNotifier() = default;

ToastNotifier::~ToastNotifier() {
    Shutdown();
    delete state_;
    state_ = nullptr;
}

bool ToastNotifier::Initialize(HINSTANCE instance, const std::wstring& appUserModelId) {
    if (!state_) {
        state_ = new IToastNotifierState();
    }
    if (state_->initialized) {
        return true;
    }

    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        state_->appUserModelId = appUserModelId;
        SetCurrentProcessExplicitAppUserModelID(appUserModelId.c_str());

        wchar_t exePath[MAX_PATH] = {};
        GetModuleFileNameW(instance, exePath, MAX_PATH);
        CreateStartMenuShortcut(exePath, appUserModelId);

        state_->notifier =
            winrt::Windows::UI::Notifications::ToastNotificationManager::CreateToastNotifier(
                appUserModelId);

        state_->initialized = true;
        return true;
    } catch (...) {
        state_->notifier = nullptr;
        state_->initialized = false;
        return false;
    }
}

void ToastNotifier::ShowAlert(const std::wstring& title, const std::wstring& message,
                              const std::wstring& tag) {
    if (!state_ || !state_->initialized) {
        return;
    }

    try {
        const std::wstring xml =
            L"<toast>"
            L"<visual>"
            L"<binding template=\"ToastGeneric\">"
            L"<text>" +
            EscapeXml(title) +
            L"</text>"
            L"<text>" +
            EscapeXml(message) +
            L"</text>"
            L"</binding>"
            L"</visual>"
            L"<actions>"
            L"<action content=\"Cancelar\" arguments=\"" +
            std::wstring(kToastActionStop) +
            L"\" activationType=\"background\" />"
            L"</actions>"
            L"</toast>";

        winrt::Windows::Data::Xml::Dom::XmlDocument document;
        document.LoadXml(xml);

        winrt::Windows::UI::Notifications::ToastNotification toast(document);
        if (!tag.empty()) {
            toast.Tag(tag);
        }
        state_->notifier.Show(toast);
    } catch (...) {
    }
}

void ToastNotifier::Shutdown() {
    if (!state_) {
        return;
    }

    state_->notifier = nullptr;
    state_->initialized = false;
}

bool ToastNotifier::IsStopAlertActivation() {
    const wchar_t* commandLine = GetCommandLineW();
    return commandLine && wcsstr(commandLine, kToastActionStop) != nullptr;
}
