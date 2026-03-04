#include "config.h"
#include "helpers.h"
#include "constants.h"
#include "../third_party/cjson/cJSON.h"
#include <cstdio>

namespace Config {

std::wstring GetConfigPath() {
    return Helpers::GetAppDataDir() + L"\\" + AppDef::CONFIG_NAME;
}

bool Load(AppConfig& cfg) {
    std::wstring path = GetConfigPath();

    FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::string buf(size, '\0');
    fread(buf.data(), 1, size, f);
    fclose(f);

    cJSON* root = cJSON_Parse(buf.c_str());
    if (!root) return false;

    auto getInt = [&](const char* key, int& val) {
        cJSON* item = cJSON_GetObjectItem(root, key);
        if (item && cJSON_IsNumber(item)) val = item->valueint;
    };
    auto getBool = [&](const char* key, bool& val) {
        cJSON* item = cJSON_GetObjectItem(root, key);
        if (item) val = cJSON_IsTrue(item);
    };
    auto getStr = [&](const char* key, std::string& val) {
        cJSON* item = cJSON_GetObjectItem(root, key);
        if (item && cJSON_IsString(item)) val = item->valuestring;
    };
    auto getWStr = [&](const char* key, std::wstring& val) {
        cJSON* item = cJSON_GetObjectItem(root, key);
        if (item && cJSON_IsString(item)) val = Helpers::Utf8ToWide(item->valuestring);
    };

    getInt("bitrate_index", cfg.bitrateIndex);
    getInt("format_index",  cfg.formatIndex);
    getBool("watch_enabled", cfg.watchEnabled);
    getWStr("watch_dir",     cfg.watchDir);
    getBool("minimize_to_tray",  cfg.minimizeToTray);
    getBool("close_to_tray",     cfg.closeToTray);
    getBool("auto_convert",      cfg.autoConvert);
    getBool("show_notifications", cfg.showNotifications);
    getBool("start_minimized",   cfg.startMinimized);
    getBool("start_with_windows", cfg.startWithWindows);
    getBool("context_menu_installed", cfg.contextMenuInstalled);
    getStr("deepgram_key", cfg.deepgramKey);
    getInt("window_x", cfg.windowX);
    getInt("window_y", cfg.windowY);
    getInt("window_w", cfg.windowW);
    getInt("window_h", cfg.windowH);

    cJSON_Delete(root);
    return true;
}

bool Save(const AppConfig& cfg) {
    cJSON* root = cJSON_CreateObject();
    if (!root) return false;

    cJSON_AddNumberToObject(root, "bitrate_index", cfg.bitrateIndex);
    cJSON_AddNumberToObject(root, "format_index",  cfg.formatIndex);
    cJSON_AddBoolToObject(root, "watch_enabled",   cfg.watchEnabled);
    cJSON_AddStringToObject(root, "watch_dir",     Helpers::WideToUtf8(cfg.watchDir).c_str());
    cJSON_AddBoolToObject(root, "minimize_to_tray",  cfg.minimizeToTray);
    cJSON_AddBoolToObject(root, "close_to_tray",     cfg.closeToTray);
    cJSON_AddBoolToObject(root, "auto_convert",      cfg.autoConvert);
    cJSON_AddBoolToObject(root, "show_notifications", cfg.showNotifications);
    cJSON_AddBoolToObject(root, "start_minimized",   cfg.startMinimized);
    cJSON_AddBoolToObject(root, "start_with_windows", cfg.startWithWindows);
    cJSON_AddBoolToObject(root, "context_menu_installed", cfg.contextMenuInstalled);
    cJSON_AddStringToObject(root, "deepgram_key", cfg.deepgramKey.c_str());
    cJSON_AddNumberToObject(root, "window_x", cfg.windowX);
    cJSON_AddNumberToObject(root, "window_y", cfg.windowY);
    cJSON_AddNumberToObject(root, "window_w", cfg.windowW);
    cJSON_AddNumberToObject(root, "window_h", cfg.windowH);

    char* json = cJSON_Print(root);
    cJSON_Delete(root);
    if (!json) return false;

    std::wstring path = GetConfigPath();
    FILE* f = _wfopen(path.c_str(), L"wb");
    if (!f) { free(json); return false; }

    fwrite(json, 1, strlen(json), f);
    fclose(f);
    free(json);
    return true;
}

} // namespace Config
