#pragma once
#include <windows.h>
#include <d2d1.h>
#include <cstdint>

namespace App {
    inline constexpr wchar_t NAME[]       = L"AudioNab Pro";
    inline constexpr wchar_t VERSION[]    = L"2.5.0";
    inline constexpr wchar_t TAGLINE[]    = L"Nab the audio from any video.";
    inline constexpr wchar_t REPO[]       = L"https://github.com/umrasghar/audionab-pro";
    inline constexpr wchar_t DB_NAME[]    = L"audionab_history.db";
    inline constexpr wchar_t CONFIG_NAME[]= L"audionab_config.json";
    inline constexpr wchar_t REG_KEY[]    = L"AudioNabPro";
    inline constexpr wchar_t REG_LABEL[]  = L"Nab Audio";
    inline constexpr wchar_t WINDOW_CLASS[] = L"AudioNabProWndClass";

    inline constexpr int INITIAL_WIDTH  = 960;
    inline constexpr int INITIAL_HEIGHT = 700;
    inline constexpr int MIN_WIDTH      = 740;
    inline constexpr int MIN_HEIGHT     = 520;

    // Supported file extensions
    inline constexpr const wchar_t* VIDEO_EXTS[] = {
        L".mp4", L".mkv", L".avi", L".mov", L".wmv", L".flv", L".webm", L".m4v",
        L".ts", L".mts", L".m2ts", L".vob", L".mpg", L".mpeg",
        L".3gp", L".3g2", L".ogv", L".divx", L".asf", L".rm", L".rmvb", L".f4v"
    };
    inline constexpr const wchar_t* AUDIO_EXTS[] = {
        L".m4a", L".aac", L".ogg", L".opus", L".wma", L".flac", L".wav", L".aiff",
        L".ac3", L".dts", L".amr", L".ape", L".wv", L".mka"
    };

    inline constexpr const char* BITRATE_OPTIONS[] = {
        "64k", "96k", "128k", "160k", "192k", "256k", "320k"
    };
    inline constexpr int DEFAULT_BITRATE_IDX = 4; // 192k

    // Custom window messages
    inline constexpr UINT WM_APP_PROGRESS   = WM_APP + 1;
    inline constexpr UINT WM_APP_DONE       = WM_APP + 2;
    inline constexpr UINT WM_APP_WATCH_FILE = WM_APP + 3;
    inline constexpr UINT WM_APP_TRAY       = WM_APP + 4;

    // Timer IDs
    inline constexpr UINT_PTR TIMER_CARET      = 1;
    inline constexpr UINT_PTR TIMER_PROGRESS   = 2;
    inline constexpr UINT_PTR TIMER_TOAST      = 3;
    inline constexpr UINT_PTR TIMER_SCROLL     = 4;
    inline constexpr UINT_PTR TIMER_SEARCH     = 5;
}
