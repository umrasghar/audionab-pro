#include "transcriber.h"
#include "helpers.h"
#include "../third_party/cjson/cJSON.h"
#include <windows.h>
#include <winhttp.h>
#include <cstdio>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace Transcriber {

TranscribeResult Transcribe(const std::wstring& audioPath,
                             const std::string& apiKey,
                             TranscribeCallback progress) {
    TranscribeResult result;

    if (apiKey.empty()) {
        result.error = L"No Deepgram API key configured";
        return result;
    }

    // Read audio file
    if (progress) progress(L"Reading audio file...");

    FILE* f = _wfopen(audioPath.c_str(), L"rb");
    if (!f) {
        result.error = L"Failed to open audio file";
        return result;
    }

    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::vector<BYTE> fileData(fileSize);
    fread(fileData.data(), 1, fileSize, f);
    fclose(f);

    if (progress) progress(L"Connecting to Deepgram...");

    // WinHTTP setup
    HINTERNET hSession = WinHttpOpen(L"AudioNabPro/2.5",
                                      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        result.error = L"Failed to create HTTP session";
        return result;
    }

    HINTERNET hConnect = WinHttpConnect(hSession,
                                         L"api.deepgram.com",
                                         INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        result.error = L"Failed to connect to Deepgram";
        return result;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST",
        L"/v1/listen?model=nova-2&smart_format=true&punctuate=true",
        nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);

    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        result.error = L"Failed to create HTTP request";
        return result;
    }

    // Set headers
    std::wstring authHeader = L"Token " + Helpers::Utf8ToWide(apiKey);
    std::wstring headers = L"Authorization: " + authHeader + L"\r\n"
                           L"Content-Type: audio/mpeg\r\n";

    WinHttpAddRequestHeaders(hRequest, headers.c_str(),
                              static_cast<DWORD>(headers.size()),
                              WINHTTP_ADDREQ_FLAG_ADD);

    if (progress) progress(L"Uploading audio...");

    // Send request
    BOOL sent = WinHttpSendRequest(hRequest,
                                    WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                    fileData.data(), static_cast<DWORD>(fileData.size()),
                                    static_cast<DWORD>(fileData.size()), 0);

    if (!sent || !WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        result.error = L"Failed to send request to Deepgram";
        return result;
    }

    if (progress) progress(L"Processing transcription...");

    // Read response
    std::string response;
    BYTE buf[8192];
    DWORD bytesRead;
    while (WinHttpReadData(hRequest, buf, sizeof(buf), &bytesRead) && bytesRead > 0) {
        response.append(reinterpret_cast<char*>(buf), bytesRead);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    // Parse JSON response
    cJSON* root = cJSON_Parse(response.c_str());
    if (!root) {
        result.error = L"Failed to parse Deepgram response";
        return result;
    }

    // Navigate: results.channels[0].alternatives[0].transcript
    cJSON* results = cJSON_GetObjectItem(root, "results");
    if (results) {
        cJSON* channels = cJSON_GetObjectItem(results, "channels");
        if (channels && cJSON_IsArray(channels) && cJSON_GetArraySize(channels) > 0) {
            cJSON* ch0 = cJSON_GetArrayItem(channels, 0);
            cJSON* alternatives = cJSON_GetObjectItem(ch0, "alternatives");
            if (alternatives && cJSON_IsArray(alternatives) && cJSON_GetArraySize(alternatives) > 0) {
                cJSON* alt0 = cJSON_GetArrayItem(alternatives, 0);
                cJSON* transcript = cJSON_GetObjectItem(alt0, "transcript");
                cJSON* confidence = cJSON_GetObjectItem(alt0, "confidence");

                if (transcript && cJSON_IsString(transcript)) {
                    result.transcript = Helpers::Utf8ToWide(transcript->valuestring);
                    result.success = true;
                }
                if (confidence && cJSON_IsNumber(confidence)) {
                    result.confidence = confidence->valuedouble;
                }
            }
        }
    }

    if (!result.success) {
        // Check for error message
        cJSON* err = cJSON_GetObjectItem(root, "err_msg");
        if (err && cJSON_IsString(err)) {
            result.error = Helpers::Utf8ToWide(err->valuestring);
        } else {
            result.error = L"No transcript found in response";
        }
    }

    cJSON_Delete(root);
    return result;
}

} // namespace Transcriber
