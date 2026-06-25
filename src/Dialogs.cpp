#include "Dialogs.hpp"

#include "WindowsCommon.hpp"
#include "AudioPlayer.hpp"
#include "ChargeControl.hpp"
#include "Config.hpp"
#include "LaptopBrands.hpp"
#include "resource.h"

namespace {

AudioPlayer g_testPlayer;
HWND g_messageBoxCenterOwner = nullptr;
bool g_updatingThresholdControls = false;

RECT GetWorkAreaForWindow(HWND window) {
    const HMONITOR monitor =
        MonitorFromWindow(window ? window : GetDesktopWindow(), MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(monitor, &info);
    return info.rcWork;
}

void PositionWindowCentered(HWND window, const RECT& bounds) {
    RECT windowRect{};
    GetWindowRect(window, &windowRect);
    const int width = windowRect.right - windowRect.left;
    const int height = windowRect.bottom - windowRect.top;
    const int x = bounds.left + ((bounds.right - bounds.left) - width) / 2;
    const int y = bounds.top + ((bounds.bottom - bounds.top) - height) / 2;
    SetWindowPos(window, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

INT_PTR CALLBACK MessageBoxCenterHook(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HCBT_ACTIVATE) {
        const HWND msgBox = reinterpret_cast<HWND>(wParam);
        if (g_messageBoxCenterOwner && IsWindow(g_messageBoxCenterOwner)) {
            RECT ownerRect{};
            GetWindowRect(g_messageBoxCenterOwner, &ownerRect);
            PositionWindowCentered(msgBox, ownerRect);
        } else {
            PositionWindowCentered(msgBox, GetWorkAreaForWindow(nullptr));
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

int ReadThresholdText(HWND dialog, int controlId) {
    wchar_t thresholdText[8] = {};
    GetDlgItemTextW(dialog, controlId, thresholdText, 8);
    return _wtoi(thresholdText);
}

bool IsLowThresholdControl(int controlId) {
    return controlId == IDC_LOW_THRESHOLD;
}

int ClampThresholdControl(int controlId, int value) {
    return IsLowThresholdControl(controlId) ? ClampLowThreshold(value) : ClampHighThreshold(value);
}

int ThresholdMin(int controlId) {
    return IsLowThresholdControl(controlId) ? kLowThresholdMin : kHighThresholdMin;
}

int ThresholdMax(int controlId) {
    return IsLowThresholdControl(controlId) ? kLowThresholdMax : kHighThresholdMax;
}

void SetThresholdValue(HWND dialog, int editId, int spinId, int value) {
    g_updatingThresholdControls = true;
    SetDlgItemInt(dialog, editId, value, FALSE);
    g_updatingThresholdControls = false;
    SendMessageW(GetDlgItem(dialog, spinId), UDM_SETPOS32, 0, static_cast<LPARAM>(value));
}

void InitThresholdSpin(HWND dialog, int editId, int spinId, int value) {
    const HWND edit = GetDlgItem(dialog, editId);
    const HWND spin = GetDlgItem(dialog, spinId);
    const int clamped = ClampThresholdControl(editId, value);
    SendMessageW(spin, UDM_SETBUDDY, 0, reinterpret_cast<LPARAM>(edit));
    SendMessageW(spin, UDM_SETRANGE32, 0,
                 MAKELPARAM(ThresholdMin(editId), ThresholdMax(editId)));
    SetThresholdValue(dialog, editId, spinId, clamped);
}

bool HandleThresholdSpinChange(HWND dialog, const NMUPDOWN* change) {
    int editId = 0;
    int spinId = 0;
    if (change->hdr.idFrom == IDC_LOW_THRESHOLD_SPIN) {
        editId = IDC_LOW_THRESHOLD;
        spinId = IDC_LOW_THRESHOLD_SPIN;
    } else if (change->hdr.idFrom == IDC_HIGH_THRESHOLD_SPIN) {
        editId = IDC_HIGH_THRESHOLD;
        spinId = IDC_HIGH_THRESHOLD_SPIN;
    } else {
        return false;
    }

    int value = ReadThresholdText(dialog, editId);
    if (value == 0 && GetWindowTextLengthW(GetDlgItem(dialog, editId)) == 0) {
        value = ThresholdMin(editId);
    }
    value += change->iDelta;
    value = ClampThresholdControl(editId, value);
    SetThresholdValue(dialog, editId, spinId, value);
    return true;
}

void CommitThresholdEdit(HWND dialog, int editId, int spinId) {
    if (g_updatingThresholdControls) {
        return;
    }

    const HWND edit = GetDlgItem(dialog, editId);
    if (GetWindowTextLengthW(edit) == 0) {
        SetThresholdValue(dialog, editId, spinId, ThresholdMin(editId));
        return;
    }

    const int value = ClampThresholdControl(editId, ReadThresholdText(dialog, editId));
    SetThresholdValue(dialog, editId, spinId, value);
}

void UpdateCompatibilityText(HWND dialog, LaptopBrand brand) {
    const auto& info = GetBrandInfo(brand);
    std::wstring text = info.compatibilityDescription;

    if (brand == LaptopBrand::Acer) {
        text += L"\n\nPulsa 'Probar WMI / modo salud Acer' para diagnosticar.";
    }

    SetDlgItemTextW(dialog, IDC_COMPAT_INFO, text.c_str());

    const bool showAcerButtons = brand == LaptopBrand::Acer;
    ShowWindow(GetDlgItem(dialog, IDC_TEST_ACER_WMI), showAcerButtons ? SW_SHOW : SW_HIDE);
}

void PopulateBrandCombo(HWND combo) {
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    for (int i = 0; i < GetBrandCount(); ++i) {
        const auto brand = BrandFromIndex(i);
        SendMessageW(combo, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(GetBrandInfo(brand).displayName));
    }
}

void SetTestSoundButtonState(HWND dialog, bool playing) {
    SetDlgItemTextW(dialog, IDC_TEST_SOUND, playing ? L"&STOP" : L"Probar sonido");
}

INT_PTR CALLBACK ConfigDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam) {
    static AppConfig* config = nullptr;

    switch (message) {
        case WM_INITDIALOG: {
            config = reinterpret_cast<AppConfig*>(lParam);
            InitThresholdSpin(dialog, IDC_LOW_THRESHOLD, IDC_LOW_THRESHOLD_SPIN,
                              config->lowThresholdPercent);
            InitThresholdSpin(dialog, IDC_HIGH_THRESHOLD, IDC_HIGH_THRESHOLD_SPIN,
                              config->highThresholdPercent);

            SetDlgItemTextW(dialog, IDC_SOUND_PATH, config->soundPath.c_str());
            CheckDlgButton(dialog, IDC_SOUND_LOOP, config->soundLoop ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(dialog, IDC_STARTUP, config->startWithWindows ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(dialog, IDC_AUTO_APPLY,
                           config->autoApplyChargeLimit ? BST_CHECKED : BST_UNCHECKED);

            const HWND combo = GetDlgItem(dialog, IDC_LAPTOP_BRAND);
            PopulateBrandCombo(combo);
            SendMessageW(combo, CB_SETCURSEL, IndexFromBrand(config->laptopBrand), 0);
            UpdateCompatibilityText(dialog, config->laptopBrand);
            SetTestSoundButtonState(dialog, false);
            CenterDialogOnScreen(dialog);
            return TRUE;
        }
        case WM_NOTIFY: {
            const auto* header = reinterpret_cast<LPNMHDR>(lParam);
            if (header->code == UDN_DELTAPOS) {
                const auto* change = reinterpret_cast<LPNMUPDOWN>(lParam);
                if (HandleThresholdSpinChange(dialog, change)) {
                    return TRUE;
                }
            }
            break;
        }
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_LOW_THRESHOLD:
                case IDC_HIGH_THRESHOLD:
                    if (HIWORD(wParam) == EN_KILLFOCUS) {
                        const int editId = LOWORD(wParam);
                        const int spinId = editId == IDC_LOW_THRESHOLD ? IDC_LOW_THRESHOLD_SPIN
                                                                         : IDC_HIGH_THRESHOLD_SPIN;
                        CommitThresholdEdit(dialog, editId, spinId);
                    }
                    return TRUE;
                case IDC_LAPTOP_BRAND:
                    if (HIWORD(wParam) == CBN_SELCHANGE) {
                        const int index =
                            static_cast<int>(SendMessageW(GetDlgItem(dialog, IDC_LAPTOP_BRAND),
                                                          CB_GETCURSEL, 0, 0));
                        UpdateCompatibilityText(dialog, BrandFromIndex(index));
                    }
                    return TRUE;
                case IDC_BROWSE_SOUND: {
                    wchar_t filePath[MAX_PATH] = {};
                    OPENFILENAMEW ofn{};
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = dialog;
                    ofn.lpstrFilter = L"Audio MP3/WAV\0*.mp3;*.wav\0MP3\0*.mp3\0WAV\0*.wav\0Todos\0*.*\0";
                    ofn.lpstrFile = filePath;
                    ofn.nMaxFile = MAX_PATH;
                    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
                    if (GetOpenFileNameW(&ofn)) {
                        SetDlgItemTextW(dialog, IDC_SOUND_PATH, filePath);
                    }
                    return TRUE;
                }
                case IDC_TEST_SOUND: {
                    if (g_testPlayer.IsPlaying()) {
                        g_testPlayer.Stop();
                        SetTestSoundButtonState(dialog, false);
                        return TRUE;
                    }

                    wchar_t path[MAX_PATH] = {};
                    GetDlgItemTextW(dialog, IDC_SOUND_PATH, path, MAX_PATH);
                    g_testPlayer.Play(path,
                                      IsDlgButtonChecked(dialog, IDC_SOUND_LOOP) == BST_CHECKED);
                    if (g_testPlayer.IsPlaying()) {
                        SetTestSoundButtonState(dialog, true);
                    } else {
                        std::wstring errorText = g_testPlayer.GetLastError();
                        if (errorText.empty()) {
                            errorText = L"No se pudo reproducir el archivo seleccionado.";
                        }
                        ShowMessageBoxCentered(dialog, errorText.c_str(), L"Probar sonido",
                                               MB_ICONWARNING | MB_OK);
                    }
                    return TRUE;
                }
                case IDC_TEST_ACER_WMI: {
                    const auto status = ChargeControl::ProbeAcerWmi();
                    std::wstring infoText = ChargeControl::FormatAcerStatusText(status);
                    if (!status.discoveredMethods.empty()) {
                        infoText += L"\n\nMetodos WMI:\n";
                        for (const auto& method : status.discoveredMethods) {
                            infoText += L"- ";
                            infoText += method;
                            infoText += L"\n";
                        }
                    }
                    infoText += L"\n";
                    infoText += status.summary;
                    ShowMessageBoxCentered(dialog, infoText.c_str(), L"Diagnostico WMI Acer",
                                           MB_ICONINFORMATION | MB_OK);
                    UpdateCompatibilityText(dialog, LaptopBrand::Acer);
                    return TRUE;
                }
                case IDC_STOP_CHARGE: {
                    CommitThresholdEdit(dialog, IDC_HIGH_THRESHOLD, IDC_HIGH_THRESHOLD_SPIN);
                    const int brandIndex =
                        static_cast<int>(SendMessageW(GetDlgItem(dialog, IDC_LAPTOP_BRAND),
                                                      CB_GETCURSEL, 0, 0));
                    const int threshold =
                        ClampHighThreshold(ReadThresholdText(dialog, IDC_HIGH_THRESHOLD));

                    const auto result = ChargeControl::TryStopCharging(
                        BrandFromIndex(brandIndex), threshold);
                    ShowMessageBoxCentered(dialog, result.message.c_str(),
                                           result.success ? L"Parar carga" : L"No se pudo aplicar",
                                           result.success ? MB_ICONINFORMATION : MB_ICONWARNING);
                    return TRUE;
                }
                case IDOK: {
                    if (!config) {
                        EndDialog(dialog, IDCANCEL);
                        return TRUE;
                    }

                    CommitThresholdEdit(dialog, IDC_LOW_THRESHOLD, IDC_LOW_THRESHOLD_SPIN);
                    CommitThresholdEdit(dialog, IDC_HIGH_THRESHOLD, IDC_HIGH_THRESHOLD_SPIN);

                    const int lowThreshold =
                        ClampLowThreshold(ReadThresholdText(dialog, IDC_LOW_THRESHOLD));
                    const int highThreshold =
                        ClampHighThreshold(ReadThresholdText(dialog, IDC_HIGH_THRESHOLD));
                    if (lowThreshold >= highThreshold) {
                        ShowMessageBoxCentered(
                            dialog,
                            L"El limite bajo debe ser menor que el limite alto.\n"
                            L"Bajo: 0-60% descarga. Alto: 60-90% carga.\n"
                            L"Ejemplo: 20% y 80%.",
                            L"Limites invalidos", MB_ICONWARNING | MB_OK);
                        return TRUE;
                    }

                    config->lowThresholdPercent = lowThreshold;
                    config->highThresholdPercent = highThreshold;

                    wchar_t soundPath[MAX_PATH] = {};
                    GetDlgItemTextW(dialog, IDC_SOUND_PATH, soundPath, MAX_PATH);
                    config->soundPath = soundPath;
                    config->soundLoop =
                        IsDlgButtonChecked(dialog, IDC_SOUND_LOOP) == BST_CHECKED;
                    config->startWithWindows =
                        IsDlgButtonChecked(dialog, IDC_STARTUP) == BST_CHECKED;
                    config->autoApplyChargeLimit =
                        IsDlgButtonChecked(dialog, IDC_AUTO_APPLY) == BST_CHECKED;
                    config->firstRunComplete = true;

                    const int brandIndex =
                        static_cast<int>(SendMessageW(GetDlgItem(dialog, IDC_LAPTOP_BRAND),
                                                      CB_GETCURSEL, 0, 0));
                    config->laptopBrand = BrandFromIndex(brandIndex);

                    g_testPlayer.Stop();
                    EndDialog(dialog, IDOK);
                    return TRUE;
                }
                case IDCANCEL:
                    g_testPlayer.Stop();
                    EndDialog(dialog, IDCANCEL);
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

INT_PTR CALLBACK AboutDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM) {
    switch (message) {
        case WM_INITDIALOG:
            CenterDialogOnScreen(dialog);
            return TRUE;
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                EndDialog(dialog, IDOK);
                return TRUE;
            }
            break;
    }
    return FALSE;
}

}  // namespace

void CenterDialogOnScreen(HWND dialog) {
    PositionWindowCentered(dialog, GetWorkAreaForWindow(nullptr));
}

void CenterDialogOnOwner(HWND dialog, HWND owner) {
    if (owner && IsWindowVisible(owner)) {
        RECT ownerRect{};
        GetWindowRect(owner, &ownerRect);
        PositionWindowCentered(dialog, ownerRect);
        return;
    }
    CenterDialogOnScreen(dialog);
}

INT_PTR ShowMessageBoxCentered(HWND owner, LPCWSTR text, LPCWSTR caption, UINT type) {
    g_messageBoxCenterOwner = owner;
    const HHOOK hook =
        SetWindowsHookExW(WH_CBT, MessageBoxCenterHook, nullptr, GetCurrentThreadId());
    const INT_PTR result = MessageBoxW(owner, text, caption, type);
    if (hook) {
        UnhookWindowsHookEx(hook);
    }
    g_messageBoxCenterOwner = nullptr;
    return result;
}

INT_PTR ShowConfigDialog(HWND owner, AppConfig& config) {
    return DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_CONFIG), owner,
                           ConfigDialogProc, reinterpret_cast<LPARAM>(&config));
}

INT_PTR ShowAboutDialog(HWND owner) {
    return DialogBoxW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_ABOUT), owner,
                      AboutDialogProc);
}
