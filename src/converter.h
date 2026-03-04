#pragma once
#include <string>
#include <functional>
#include <cstdint>
#include <windows.h>

// Output format enum
enum class AudioFormat { MP3 = 0, FLAC, WAV, AAC, Opus };

// Probe result
struct ProbeResult {
    bool hasAudio   = false;
    double duration = 0.0;
    int sampleRate  = 0;
    int channels    = 0;
    std::wstring codecName;
    int64_t fileSize = 0;
};

// Conversion progress callback: (progress 0.0-1.0, status message)
using ProgressCallback = std::function<void(float, const std::wstring&)>;

// Conversion result
struct ConvertResult {
    bool success = false;
    std::wstring outputPath;
    std::wstring error;
    int64_t outputSize = 0;
    double duration = 0.0;
};

namespace Converter {
    // Initialize FFmpeg (call once at startup)
    bool Init();

    // Check if FFmpeg libs are available
    bool IsAvailable();

    // Probe a file for audio stream info
    ProbeResult Probe(const std::wstring& inputPath);

    // Convert audio — runs on caller's thread, reports progress via callback
    // cancelEvent: Win32 Event handle, set to signal cancellation
    ConvertResult Convert(const std::wstring& inputPath,
                          const std::wstring& outputDir,
                          AudioFormat format,
                          const std::string& bitrate,
                          ProgressCallback progress = nullptr,
                          HANDLE cancelEvent = nullptr);

    // Get file extension for format
    const wchar_t* FormatExtension(AudioFormat fmt);

    // Get display name for format
    const wchar_t* FormatName(AudioFormat fmt);

    // Check if a file extension is a supported input
    bool IsSupportedInput(const std::wstring& ext);
}
