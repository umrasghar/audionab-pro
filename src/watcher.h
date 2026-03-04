#pragma once
#include <windows.h>
#include <string>
#include <functional>

// Callback: (file path)
using WatchCallback = std::function<void(const std::wstring&)>;

class FolderWatcher {
public:
    ~FolderWatcher();

    bool Start(const std::wstring& dir, WatchCallback callback);
    void Stop();
    bool IsRunning() const { return running_; }
    const std::wstring& WatchDir() const { return dir_; }

private:
    static unsigned __stdcall WatchThreadProc(void* param);

    std::wstring dir_;
    WatchCallback callback_;
    HANDLE thread_ = nullptr;
    HANDLE stopEvent_ = nullptr;
    bool running_ = false;
};
