#pragma once
#include <string>
#include <functional>

struct TranscribeResult {
    bool success = false;
    std::wstring transcript;
    std::wstring error;
    double confidence = 0.0;
};

// Progress callback: (status message)
using TranscribeCallback = std::function<void(const std::wstring&)>;

namespace Transcriber {
    // Transcribe an audio file using Deepgram API
    // Runs synchronously — call from a worker thread
    TranscribeResult Transcribe(const std::wstring& audioPath,
                                 const std::string& apiKey,
                                 TranscribeCallback progress = nullptr);
}
