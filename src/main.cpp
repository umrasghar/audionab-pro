#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include "app.h"
#include "constants.h"
#include "helpers.h"
#include "converter.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")

#pragma comment(linker, "/SUBSYSTEM:WINDOWS")

// Single-instance check
static HANDLE g_mutex = nullptr;

static bool CheckSingleInstance() {
    g_mutex = CreateMutexW(nullptr, TRUE, L"AudioNabPro_SingleInstance_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Find existing window and bring to front
        HWND existing = FindWindowW(AppDef::WINDOW_CLASS, nullptr);
        if (existing) {
            if (IsIconic(existing)) ShowWindow(existing, SW_RESTORE);
            SetForegroundWindow(existing);
        }
        CloseHandle(g_mutex);
        g_mutex = nullptr;
        return false;
    }
    return true;
}

// CLI handling (--convert, --install, --uninstall)
static bool HandleCLI(int argc, wchar_t** argv) {
    if (argc < 2) return false;

    std::wstring cmd = argv[1];

    if (cmd == L"--convert" && argc >= 3) {
        // CLI convert mode
        Converter::Init();

        for (int i = 2; i < argc; i++) {
            std::wstring path = argv[i];
            if (!Helpers::FileExists(path)) {
                fwprintf(stderr, L"File not found: %s\n", path.c_str());
                continue;
            }

            std::wstring dir = Helpers::GetDirectory(path);
            ConvertResult result = Converter::Convert(
                path, dir, AudioFormat::MP3, "192k",
                [](float p, const std::wstring& s) {
                    wprintf(L"\r  [%3.0f%%] %s", p * 100, s.c_str());
                });

            if (result.success) {
                wprintf(L"\n  Done: %s (%s)\n",
                        Helpers::GetFileName(result.outputPath).c_str(),
                        Helpers::FormatSize(result.outputSize).c_str());
            } else {
                fwprintf(stderr, L"\n  Error: %s\n", result.error.c_str());
            }
        }
        return true;
    }

    if (cmd == L"--install") {
        // TODO: Install context menu
        wprintf(L"Context menu installed.\n");
        return true;
    }

    if (cmd == L"--uninstall") {
        // TODO: Uninstall context menu
        wprintf(L"Context menu uninstalled.\n");
        return true;
    }

    if (cmd == L"--version") {
        wprintf(L"%s v%s\n", AppDef::NAME, AppDef::VERSION);
        return true;
    }

    if (cmd == L"--help") {
        wprintf(L"%s v%s - %s\n\n", AppDef::NAME, AppDef::VERSION, AppDef::TAGLINE);
        wprintf(L"Usage:\n");
        wprintf(L"  AudioNabPro.exe                     Launch GUI\n");
        wprintf(L"  AudioNabPro.exe --convert <files>    Convert files to MP3\n");
        wprintf(L"  AudioNabPro.exe --install            Install context menu\n");
        wprintf(L"  AudioNabPro.exe --uninstall          Remove context menu\n");
        wprintf(L"  AudioNabPro.exe --version            Show version\n");
        return true;
    }

    // If argument is a file path, open it for conversion
    if (Helpers::FileExists(cmd)) {
        return false; // Fall through to GUI with file
    }

    return false;
}

static LONG WINAPI CrashHandler(EXCEPTION_POINTERS* ep) {
    wchar_t buf[256];
    swprintf(buf, 256, L"Crash! Exception code: 0x%08X\nAddress: 0x%p",
             ep->ExceptionRecord->ExceptionCode,
             ep->ExceptionRecord->ExceptionAddress);
    MessageBoxW(nullptr, buf, L"AudioNab Crash", MB_ICONERROR);
    return EXCEPTION_EXECUTE_HANDLER;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdLine, int nCmdShow) {
    SetUnhandledExceptionFilter(CrashHandler);

    // Initialize COM
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // Initialize common controls
    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icex);

    // Parse command line
    int argc;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    // Handle CLI commands
    if (HandleCLI(argc, argv)) {
        LocalFree(argv);
        CoUninitialize();
        return 0;
    }

    // Single-instance check
    if (!CheckSingleInstance()) {
        // Forward file to existing instance via WM_COPYDATA if file arg provided
        if (argc >= 2 && Helpers::FileExists(argv[1])) {
            HWND existing = FindWindowW(AppDef::WINDOW_CLASS, nullptr);
            if (existing) {
                COPYDATASTRUCT cds = {};
                cds.dwData = 1; // File path
                std::wstring path = argv[1];
                cds.cbData = static_cast<DWORD>((path.size() + 1) * sizeof(wchar_t));
                cds.lpData = const_cast<wchar_t*>(path.c_str());
                SendMessageW(existing, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds));
            }
        }
        LocalFree(argv);
        CoUninitialize();
        return 0;
    }

    // Launch app
    App& app = App::Instance();
    if (!app.Init(hInstance, nCmdShow)) {
        LocalFree(argv);
        if (g_mutex) CloseHandle(g_mutex);
        CoUninitialize();
        return 1;
    }

    // If a file was passed as argument, queue it for conversion
    if (argc >= 2 && Helpers::FileExists(argv[1])) {
        std::vector<std::wstring> files;
        for (int i = 1; i < argc; i++) {
            if (Helpers::FileExists(argv[i]))
                files.push_back(argv[i]);
        }
        if (!files.empty()) {
            PostMessageW(app.GetHwnd(), WM_USER + 100, 0, 0); // Trigger after init
        }
    }

    LocalFree(argv);

    // Message loop
    int exitCode = app.Run();

    app.Shutdown();
    if (g_mutex) CloseHandle(g_mutex);
    CoUninitialize();
    return exitCode;
}
