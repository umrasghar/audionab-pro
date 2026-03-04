#pragma once
#include <string>
#include <cstdint>

struct AppConfig {
    // Output settings
    int bitrateIndex      = 4;     // Index into AppDef::BITRATE_OPTIONS (192k)
    int formatIndex       = 0;     // 0=MP3, 1=FLAC, 2=WAV, 3=AAC, 4=Opus

    // Watch folder
    bool watchEnabled     = false;
    std::wstring watchDir;

    // Behavior
    bool minimizeToTray   = true;
    bool closeToTray      = false;
    bool autoConvert      = false;
    bool showNotifications= true;
    bool startMinimized   = false;
    bool startWithWindows = false;

    // Context menu
    bool contextMenuInstalled = false;

    // Deepgram API key
    std::string deepgramKey;

    // Window state
    int windowX = -1, windowY = -1;
    int windowW = 960, windowH = 700;
};

namespace Config {
    bool Load(AppConfig& cfg);
    bool Save(const AppConfig& cfg);
    std::wstring GetConfigPath();
}
