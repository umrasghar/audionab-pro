#include "converter.h"
#include "helpers.h"
#include "constants.h"

// When HAS_FFMPEG_LIBS is defined, we use the FFmpeg C API directly.
// Otherwise, we fall back to spawning ffmpeg.exe as a subprocess.
#ifdef HAS_FFMPEG_LIBS

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}

#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swresample.lib")

namespace Converter {

bool Init() {
    // FFmpeg 4.x+ doesn't need av_register_all()
    return true;
}

bool IsAvailable() {
    return true; // Static linked — always available
}

// TODO: Implement Probe using avformat_open_input + avformat_find_stream_info
ProbeResult Probe(const std::wstring& inputPath) {
    ProbeResult result;
    result.fileSize = Helpers::GetFileSize(inputPath);

    std::string path = Helpers::WideToUtf8(inputPath);
    AVFormatContext* ctx = nullptr;

    if (avformat_open_input(&ctx, path.c_str(), nullptr, nullptr) < 0)
        return result;

    if (avformat_find_stream_info(ctx, nullptr) < 0) {
        avformat_close_input(&ctx);
        return result;
    }

    for (unsigned i = 0; i < ctx->nb_streams; i++) {
        if (ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            result.hasAudio = true;
            result.duration = ctx->duration / (double)AV_TIME_BASE;
            result.sampleRate = ctx->streams[i]->codecpar->sample_rate;
            result.channels = ctx->streams[i]->codecpar->ch_layout.nb_channels;
            const AVCodecDescriptor* desc = avcodec_descriptor_get(ctx->streams[i]->codecpar->codec_id);
            if (desc) result.codecName = Helpers::Utf8ToWide(desc->name);
            break;
        }
    }

    avformat_close_input(&ctx);
    return result;
}

// TODO: Full 3-pass conversion implementation
ConvertResult Convert(const std::wstring& inputPath,
                      const std::wstring& outputDir,
                      AudioFormat format,
                      const std::string& bitrate,
                      ProgressCallback progress,
                      HANDLE cancelEvent) {
    ConvertResult result;
    // Implementation will follow in Phase 2
    result.error = L"FFmpeg conversion not yet implemented";
    return result;
}

} // namespace Converter

#else // !HAS_FFMPEG_LIBS — Fallback: spawn ffmpeg.exe

namespace Converter {

static std::wstring FindFFmpeg() {
    // Check PATH
    wchar_t buf[MAX_PATH];
    if (SearchPathW(nullptr, L"ffmpeg.exe", nullptr, MAX_PATH, buf, nullptr))
        return buf;
    // Check next to our exe
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring dir = Helpers::GetDirectory(exePath);
    std::wstring local = dir + L"\\ffmpeg.exe";
    if (Helpers::FileExists(local)) return local;
    return {};
}

bool Init() { return true; }

bool IsAvailable() {
    return !FindFFmpeg().empty();
}

ProbeResult Probe(const std::wstring& inputPath) {
    ProbeResult result;
    result.fileSize = Helpers::GetFileSize(inputPath);

    std::wstring ffmpeg = FindFFmpeg();
    if (ffmpeg.empty()) return result;

    // Use ffprobe or ffmpeg -i to get info
    std::wstring cmd = L"\"" + ffmpeg + L"\" -i \"" + inputPath +
                       L"\" -hide_banner -f null NUL 2>&1";

    // Create pipe for reading output
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE readPipe, writePipe;
    CreatePipe(&readPipe, &writePipe, &sa, 0);
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = writePipe;
    si.hStdError  = writePipe;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    std::wstring cmdLine = cmd;
    if (CreateProcessW(nullptr, cmdLine.data(), nullptr, nullptr, TRUE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(writePipe);

        std::string output;
        char buf[4096];
        DWORD read;
        while (ReadFile(readPipe, buf, sizeof(buf) - 1, &read, nullptr) && read > 0) {
            buf[read] = '\0';
            output += buf;
        }

        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        // Parse "Audio:" line
        auto pos = output.find("Audio:");
        if (pos != std::string::npos) {
            result.hasAudio = true;
            // Parse duration
            auto durPos = output.find("Duration:");
            if (durPos != std::string::npos) {
                int h, m, s;
                if (sscanf(output.c_str() + durPos, "Duration: %d:%d:%d", &h, &m, &s) == 3) {
                    result.duration = h * 3600.0 + m * 60.0 + s;
                }
            }
        }
    } else {
        CloseHandle(writePipe);
    }

    CloseHandle(readPipe);
    return result;
}

ConvertResult Convert(const std::wstring& inputPath,
                      const std::wstring& outputDir,
                      AudioFormat format,
                      const std::string& bitrate,
                      ProgressCallback progress,
                      HANDLE cancelEvent) {
    ConvertResult result;

    std::wstring ffmpeg = FindFFmpeg();
    if (ffmpeg.empty()) {
        result.error = L"FFmpeg not found. Please install FFmpeg or place ffmpeg.exe next to AudioNab.";
        return result;
    }

    // Build output path
    std::wstring baseName = Helpers::GetFileNameNoExt(inputPath);
    std::wstring ext = FormatExtension(format);
    std::wstring outputPath = outputDir + L"\\" + baseName + ext;

    // Avoid overwriting
    int counter = 1;
    while (Helpers::FileExists(outputPath)) {
        wchar_t buf[32];
        swprintf(buf, 32, L" (%d)", counter++);
        outputPath = outputDir + L"\\" + baseName + buf + ext;
    }

    // Build ffmpeg command
    std::wstring cmd = L"\"" + ffmpeg + L"\" -i \"" + inputPath + L"\" -vn -y";

    // Format-specific options
    std::wstring brWide = Helpers::Utf8ToWide(bitrate);
    switch (format) {
        case AudioFormat::MP3:
            cmd += L" -codec:a libmp3lame -b:a " + brWide;
            break;
        case AudioFormat::FLAC:
            cmd += L" -codec:a flac";
            break;
        case AudioFormat::WAV:
            cmd += L" -codec:a pcm_s16le";
            break;
        case AudioFormat::AAC:
            cmd += L" -codec:a aac -b:a " + brWide;
            break;
        case AudioFormat::Opus:
            cmd += L" -codec:a libopus -b:a " + brWide;
            break;
    }

    cmd += L" -progress pipe:1 \"" + outputPath + L"\"";

    // Run ffmpeg
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE readPipe, writePipe;
    CreatePipe(&readPipe, &writePipe, &sa, 0);
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = writePipe;
    si.hStdError  = writePipe;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    std::wstring cmdLine = cmd;

    if (!CreateProcessW(nullptr, cmdLine.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(readPipe);
        CloseHandle(writePipe);
        result.error = L"Failed to start FFmpeg process";
        return result;
    }

    CloseHandle(writePipe);

    // Read progress output
    ProbeResult probe = Probe(inputPath);
    std::string buffer;
    char buf[1024];
    DWORD bytesRead;

    while (ReadFile(readPipe, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        // Check cancel
        if (cancelEvent && WaitForSingleObject(cancelEvent, 0) == WAIT_OBJECT_0) {
            TerminateProcess(pi.hProcess, 1);
            break;
        }

        buf[bytesRead] = '\0';
        buffer += buf;

        // Parse progress lines (out_time_us=XXXXX)
        size_t pos;
        while ((pos = buffer.find("out_time_us=")) != std::string::npos) {
            size_t end = buffer.find('\n', pos);
            if (end == std::string::npos) break;
            std::string val = buffer.substr(pos + 12, end - pos - 12);
            double us = atof(val.c_str());
            if (probe.duration > 0 && progress) {
                float p = static_cast<float>(us / 1000000.0 / probe.duration);
                if (p > 1.0f) p = 1.0f;
                progress(p, L"Converting...");
            }
            buffer.erase(0, end + 1);
        }
        // Keep last partial line
        auto lastNl = buffer.rfind('\n');
        if (lastNl != std::string::npos && lastNl < buffer.size() - 1) {
            buffer = buffer.substr(lastNl + 1);
        }
    }

    CloseHandle(readPipe);
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (cancelEvent && WaitForSingleObject(cancelEvent, 0) == WAIT_OBJECT_0) {
        DeleteFileW(outputPath.c_str());
        result.error = L"Conversion cancelled";
        return result;
    }

    if (exitCode == 0 && Helpers::FileExists(outputPath)) {
        result.success = true;
        result.outputPath = outputPath;
        result.outputSize = Helpers::GetFileSize(outputPath);
        result.duration = probe.duration;
    } else {
        result.error = L"FFmpeg exited with error code " + std::to_wstring(exitCode);
        DeleteFileW(outputPath.c_str()); // Clean up partial file
    }

    return result;
}

} // namespace Converter

#endif // HAS_FFMPEG_LIBS

// --- Common functions (format tables) ---

namespace Converter {

const wchar_t* FormatExtension(AudioFormat fmt) {
    switch (fmt) {
        case AudioFormat::MP3:  return L".mp3";
        case AudioFormat::FLAC: return L".flac";
        case AudioFormat::WAV:  return L".wav";
        case AudioFormat::AAC:  return L".m4a";
        case AudioFormat::Opus: return L".opus";
    }
    return L".mp3";
}

const wchar_t* FormatName(AudioFormat fmt) {
    switch (fmt) {
        case AudioFormat::MP3:  return L"MP3";
        case AudioFormat::FLAC: return L"FLAC";
        case AudioFormat::WAV:  return L"WAV";
        case AudioFormat::AAC:  return L"AAC";
        case AudioFormat::Opus: return L"Opus";
    }
    return L"MP3";
}

bool IsSupportedInput(const std::wstring& ext) {
    for (auto e : AppDef::VIDEO_EXTS)
        if (ext == e) return true;
    for (auto e : AppDef::AUDIO_EXTS)
        if (ext == e) return true;
    return ext == L".mp3"; // MP3 is also a valid input
}

} // namespace Converter
