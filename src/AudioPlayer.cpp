#include "AudioPlayer.hpp"

#include <Shlwapi.h>
#include <dshow.h>
#include <mmsystem.h>

#pragma comment(lib, "strmiids.lib")

namespace {

bool EndsWithIgnoreCase(const std::wstring& value, const wchar_t* suffix) {
    const size_t suffixLen = wcslen(suffix);
    if (value.size() < suffixLen) {
        return false;
    }
    return _wcsicmp(value.c_str() + value.size() - suffixLen, suffix) == 0;
}

}  // namespace

AudioPlayer::AudioPlayer() = default;

AudioPlayer::~AudioPlayer() {
    Stop();
}

bool AudioPlayer::IsWavFile(const std::wstring& path) const {
    return EndsWithIgnoreCase(path, L".wav");
}

void AudioPlayer::ReleaseDirectShow() {
    if (eventThread_.joinable()) {
        eventThread_.join();
    }

    if (control_) {
        control_->Stop();
    }

    if (events_) {
        events_->Release();
        events_ = nullptr;
    }
    if (seeking_) {
        seeking_->Release();
        seeking_ = nullptr;
    }
    if (control_) {
        control_->Release();
        control_ = nullptr;
    }
    if (graph_) {
        graph_->Release();
        graph_ = nullptr;
    }
}

void AudioPlayer::Stop() {
    stopRequested_ = true;

    if (usingPlaySound_) {
        PlaySoundW(nullptr, nullptr, SND_PURGE);
        usingPlaySound_ = false;
    }

    ReleaseDirectShow();
    playing_ = false;
    loop_ = false;
    stopRequested_ = false;
}

bool AudioPlayer::PlayWithPlaySound(const std::wstring& path, bool loop) {
    DWORD flags = SND_FILENAME | SND_ASYNC;
    if (loop) {
        flags |= SND_LOOP;
    }

    if (!PlaySoundW(path.c_str(), nullptr, flags)) {
        lastError_ = L"No se pudo reproducir el archivo WAV.";
        return false;
    }

    usingPlaySound_ = true;
    playing_ = true;
    return true;
}

void AudioPlayer::EventLoop() {
    while (!stopRequested_) {
        if (!events_) {
            break;
        }

        long eventCode = 0;
        LONG_PTR param1 = 0;
        LONG_PTR param2 = 0;
        const HRESULT hr = events_->GetEvent(&eventCode, &param1, &param2, 100);
        if (hr == S_OK) {
            events_->FreeEventParams(eventCode, param1, param2);
            if (eventCode == EC_COMPLETE) {
                if (loop_ && !stopRequested_) {
                    REFERENCE_TIME start = 0;
                    if (seeking_) {
                        seeking_->SetPositions(&start, AM_SEEKING_AbsolutePositioning, nullptr,
                                               AM_SEEKING_NoPositioning);
                    }
                    if (control_) {
                        control_->Run();
                    }
                } else {
                    playing_ = false;
                    break;
                }
            }
        }
    }
}

bool AudioPlayer::PlayWithDirectShow(const std::wstring& path, bool loop) {
    HRESULT hr = CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&graph_));
    if (FAILED(hr) || !graph_) {
        lastError_ = L"No se pudo iniciar el reproductor de audio (DirectShow).";
        return false;
    }

    hr = graph_->QueryInterface(IID_PPV_ARGS(&control_));
    if (FAILED(hr) || !control_) {
        lastError_ = L"No se pudo crear el control de reproduccion.";
        ReleaseDirectShow();
        return false;
    }

    hr = graph_->QueryInterface(IID_PPV_ARGS(&events_));
    if (FAILED(hr) || !events_) {
        lastError_ = L"No se pudo crear el canal de eventos de audio.";
        ReleaseDirectShow();
        return false;
    }

    hr = graph_->QueryInterface(IID_PPV_ARGS(&seeking_));
    if (FAILED(hr) || !seeking_) {
        lastError_ = L"No se pudo crear el control de posicion de audio.";
        ReleaseDirectShow();
        return false;
    }

    hr = events_->SetNotifyWindow(static_cast<OAHWND>(NULL), 0, 0);

    hr = graph_->RenderFile(path.c_str(), nullptr);
    if (FAILED(hr)) {
        lastError_ =
            L"No se pudo abrir el archivo de audio.\n"
            L"Prueba con un MP3 o WAV valido, o convierte el archivo a WAV.";
        ReleaseDirectShow();
        return false;
    }

    hr = control_->Run();
    if (FAILED(hr)) {
        lastError_ = L"No se pudo iniciar la reproduccion del archivo.";
        ReleaseDirectShow();
        return false;
    }

    loop_ = loop;
    stopRequested_ = false;
    playing_ = true;
    eventThread_ = std::thread([this]() { EventLoop(); });
    return true;
}

void AudioPlayer::Play(const std::wstring& path, bool loop) {
    Stop();
    lastError_.clear();

    if (path.empty()) {
        lastError_ = L"No hay ningun archivo de sonido seleccionado.";
        MessageBeep(MB_ICONEXCLAMATION);
        return;
    }

    if (PathFileExistsW(path.c_str()) != TRUE) {
        lastError_ = L"No se encontro el archivo:\n" + path;
        MessageBeep(MB_ICONEXCLAMATION);
        return;
    }

    if (IsWavFile(path)) {
        if (PlayWithPlaySound(path, loop)) {
            return;
        }
        MessageBeep(MB_ICONEXCLAMATION);
        return;
    }

    if (PlayWithDirectShow(path, loop)) {
        return;
    }

    MessageBeep(MB_ICONEXCLAMATION);
}
