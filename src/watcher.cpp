#include "watcher.h"
#include "converter.h"
#include "helpers.h"
#include <process.h>

FolderWatcher::~FolderWatcher() {
    Stop();
}

bool FolderWatcher::Start(const std::wstring& dir, WatchCallback callback) {
    if (running_) Stop();

    dir_ = dir;
    callback_ = callback;
    stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    thread_ = reinterpret_cast<HANDLE>(_beginthreadex(
        nullptr, 0, WatchThreadProc, this, 0, nullptr));

    if (!thread_) {
        CloseHandle(stopEvent_);
        stopEvent_ = nullptr;
        return false;
    }

    running_ = true;
    return true;
}

void FolderWatcher::Stop() {
    if (!running_) return;

    SetEvent(stopEvent_);
    if (thread_) {
        WaitForSingleObject(thread_, 5000);
        CloseHandle(thread_);
        thread_ = nullptr;
    }
    CloseHandle(stopEvent_);
    stopEvent_ = nullptr;
    running_ = false;
}

unsigned __stdcall FolderWatcher::WatchThreadProc(void* param) {
    auto* self = static_cast<FolderWatcher*>(param);

    HANDLE hDir = CreateFileW(
        self->dir_.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr);

    if (hDir == INVALID_HANDLE_VALUE) return 1;

    OVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    BYTE buffer[4096];
    HANDLE events[2] = { self->stopEvent_, overlapped.hEvent };

    while (true) {
        ResetEvent(overlapped.hEvent);

        BOOL ok = ReadDirectoryChangesW(
            hDir, buffer, sizeof(buffer), FALSE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE,
            nullptr, &overlapped, nullptr);

        if (!ok) break;

        DWORD waitResult = WaitForMultipleObjects(2, events, FALSE, INFINITE);

        if (waitResult == WAIT_OBJECT_0) {
            // Stop event signaled
            CancelIoEx(hDir, &overlapped);
            break;
        }

        if (waitResult == WAIT_OBJECT_0 + 1) {
            DWORD bytesReturned;
            if (!GetOverlappedResult(hDir, &overlapped, &bytesReturned, FALSE))
                continue;

            auto* fni = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer);
            while (true) {
                if (fni->Action == FILE_ACTION_ADDED || fni->Action == FILE_ACTION_MODIFIED) {
                    std::wstring fileName(fni->FileName, fni->FileNameLength / sizeof(wchar_t));
                    std::wstring fullPath = self->dir_ + L"\\" + fileName;
                    std::wstring ext = Helpers::GetFileExtension(fullPath);

                    if (Converter::IsSupportedInput(ext)) {
                        // Wait for file to stabilize (5 seconds of no size changes)
                        int64_t lastSize = -1;
                        int stableCount = 0;
                        for (int i = 0; i < 50; i++) { // Max 25 seconds
                            if (WaitForSingleObject(self->stopEvent_, 500) == WAIT_OBJECT_0)
                                goto done;

                            int64_t size = Helpers::GetFileSize(fullPath);
                            if (size == lastSize && size > 0) {
                                stableCount++;
                                if (stableCount >= 10) break; // 5 seconds stable
                            } else {
                                stableCount = 0;
                            }
                            lastSize = size;
                        }

                        if (self->callback_) {
                            self->callback_(fullPath);
                        }
                    }
                }

                if (fni->NextEntryOffset == 0) break;
                fni = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                    reinterpret_cast<BYTE*>(fni) + fni->NextEntryOffset);
            }
        }
    }

done:
    CloseHandle(overlapped.hEvent);
    CloseHandle(hDir);
    return 0;
}
