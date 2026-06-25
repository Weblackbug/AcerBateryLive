#pragma once

#include "WindowsCommon.hpp"
#include <atomic>
#include <string>
#include <thread>

struct IGraphBuilder;
struct IMediaControl;
struct IMediaEventEx;
struct IMediaSeeking;

class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();

    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;

    void Play(const std::wstring& path, bool loop);
    void Stop();
    bool IsPlaying() const { return playing_; }
    std::wstring GetLastError() const { return lastError_; }

private:
    bool IsWavFile(const std::wstring& path) const;
    bool PlayWithPlaySound(const std::wstring& path, bool loop);
    bool PlayWithDirectShow(const std::wstring& path, bool loop);
    void ReleaseDirectShow();
    void EventLoop();

    std::atomic<bool> playing_{false};
    std::atomic<bool> stopRequested_{false};
    bool loop_ = false;
    bool usingPlaySound_ = false;

    IGraphBuilder* graph_ = nullptr;
    IMediaControl* control_ = nullptr;
    IMediaEventEx* events_ = nullptr;
    IMediaSeeking* seeking_ = nullptr;
    std::thread eventThread_;

    std::wstring lastError_;
};
