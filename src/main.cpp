#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <sstream>

#include "target_detector.hpp"
#include "clip_manager.hpp"
#include "tray_manager.hpp"

using namespace mouseclip;

// Global constants
constexpr UINT IDT_HEARTBEAT = 1001;
constexpr int HOTKEY_ID_TOGGLE = 2001; // Ctrl+Alt+C
constexpr int HOTKEY_ID_EXIT_END = 2002; // Ctrl+Alt+End
constexpr int HOTKEY_ID_EXIT_Q   = 2003; // Ctrl+Alt+Q

// Application runtime configuration
struct AppConfig {
    bool enableConsole = false;
    bool enableTray = true;
    int heartbeatIntervalMs = 200;
    std::wstring customTestTarget;
};

// Global state pointers for Win32 callbacks
static HWND g_hwndMsg = nullptr;
static HWINEVENTHOOK g_hHookForeground = nullptr;
static TargetDetector* g_detector = nullptr;
static ClipManager* g_clipManager = nullptr;
static TrayManager* g_trayManager = nullptr;
static AppConfig g_config;
static bool g_running = true;

// Helper: Writes wide text to console or redirected pipe/file
static void WriteToOutput(const std::wstring& text) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!hOut || hOut == INVALID_HANDLE_VALUE) {
        if (AttachConsole(ATTACH_PARENT_PROCESS) != 0) {
            hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        }
    }
    if (hOut && hOut != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        if (!WriteConsoleW(hOut, text.c_str(), static_cast<DWORD>(text.length()), &written, NULL) || written == 0) {
            int utf8Len = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, NULL, 0, NULL, NULL);
            if (utf8Len > 1) {
                std::string utf8Str(utf8Len, '\0');
                WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, &utf8Str[0], utf8Len, NULL, NULL);
                WriteFile(hOut, utf8Str.c_str(), static_cast<DWORD>(utf8Len - 1), &written, NULL);
            }
        }
    }
}

// Helper: Formatted console logging
static void LogMessage(const std::string& level, const std::wstring& msg) {
    if (!g_config.enableConsole) return;

    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tm_buf;
    localtime_s(&tm_buf, &in_time_t);

    std::wstringstream ss;
    ss << L"[" << std::put_time(&tm_buf, L"%H:%M:%S")
       << L"." << std::setfill(L'0') << std::setw(3) << ms.count() << L"] "
       << L"[" << std::wstring(level.begin(), level.end()) << L"] "
       << msg << L"\r\n";

    WriteToOutput(ss.str());
}

// WinEventHook Callback for EVENT_SYSTEM_FOREGROUND
static void CALLBACK WinEventProc(
    HWINEVENTHOOK /*hWinEventHook*/,
    DWORD event,
    HWND /*hwnd*/,
    LONG /*idObject*/,
    LONG /*idChild*/,
    DWORD /*dwEventThread*/,
    DWORD /*dwmsEventTime*/)
{
    if (event == EVENT_SYSTEM_FOREGROUND && g_clipManager) {
        // Trigger instant boundary re-evaluation on the message thread
        g_clipManager->Update();
    }
}

// Console Control Handler (Ctrl+C, Ctrl+Break, Close)
static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) {
    switch (dwCtrlType) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        if (g_clipManager) {
            g_clipManager->Release();
        }
        ClipCursor(NULL);
        if (g_hwndMsg) {
            PostMessageW(g_hwndMsg, WM_CLOSE, 0, 0);
        }
        return TRUE;
    default:
        return FALSE;
    }
}

// Window Procedure for hidden message-only window
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_TIMER: {
        if (wParam == IDT_HEARTBEAT && g_clipManager) {
            g_clipManager->Update();
        }
        return 0;
    }
    case WM_HOTKEY: {
        int hotkeyId = static_cast<int>(wParam);
        if (hotkeyId == HOTKEY_ID_TOGGLE && g_clipManager) {
            g_clipManager->TogglePause();
            LogMessage("HOTKEY", g_clipManager->IsPaused()
                ? L"Boundary locking PAUSED via Ctrl+Alt+C"
                : L"Boundary locking RESUMED via Ctrl+Alt+C");
        } else if (hotkeyId == HOTKEY_ID_EXIT_END || hotkeyId == HOTKEY_ID_EXIT_Q) {
            LogMessage("HOTKEY", L"Kill-switch triggered! Releasing cursor and terminating...");
            if (g_clipManager) {
                g_clipManager->Release();
            }
            ClipCursor(NULL);
            DestroyWindow(hwnd);
        }
        return 0;
    }
    case WM_TRAYICON: {
        if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_CONTEXTMENU || LOWORD(lParam) == WM_LBUTTONUP) {
            if (g_trayManager) {
                g_trayManager->ShowContextMenu(hwnd);
            }
        } else if (LOWORD(lParam) == WM_LBUTTONDBLCLK) {
            if (g_clipManager) {
                g_clipManager->TogglePause();
            }
        }
        return 0;
    }
    case WM_COMMAND: {
        UINT cmdId = LOWORD(wParam);
        switch (cmdId) {
        case IDM_TRAY_PAUSE:
            if (g_clipManager) {
                g_clipManager->TogglePause();
            }
            break;
        case IDM_TRAY_REFRESH:
            if (g_clipManager) {
                g_clipManager->Update();
            }
            break;
        case IDM_TRAY_EXIT:
            if (g_clipManager) {
                g_clipManager->Release();
            }
            ClipCursor(NULL);
            DestroyWindow(hwnd);
            break;
        default:
            break;
        }
        return 0;
    }
    case WM_QUERYENDSESSION:
    case WM_ENDSESSION: {
        if (g_clipManager) {
            g_clipManager->Release();
        }
        ClipCursor(NULL);
        return TRUE;
    }
    case WM_DESTROY: {
        if (g_clipManager) {
            g_clipManager->Release();
        }
        ClipCursor(NULL);
        PostQuitMessage(0);
        return 0;
    }
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// Parses command line arguments
static void ParseCommandLine(int argc, wchar_t* argv[], AppConfig& config) {
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"--console" || arg == L"-c") {
            config.enableConsole = true;
        } else if (arg == L"--headless" || arg == L"--silent" || arg == L"-s") {
            config.enableTray = false;
        } else if ((arg == L"--interval" || arg == L"-i") && i + 1 < argc) {
            int interval = _wtoi(argv[++i]);
            if (interval >= 50 && interval <= 2000) {
                config.heartbeatIntervalMs = interval;
            }
        } else if ((arg == L"--test-target" || arg == L"-t") && i + 1 < argc) {
            config.customTestTarget = argv[++i];
        } else if (arg == L"--help" || arg == L"-h" || arg == L"/?") {
            std::wstring helpText =
                L"mouseClip - League of Legends Multi-Monitor Boundary Locker v1.0\r\n\r\n"
                L"Usage: mouseClip.exe [options]\r\n\r\n"
                L"Options:\r\n"
                L"  --console, -c          Attach console window for live diagnostic logging\r\n"
                L"  --headless, -s         Run without system tray icon\r\n"
                L"  --interval, -i <ms>    Heartbeat poll interval in milliseconds (default: 200)\r\n"
                L"  --test-target, -t <nm> Custom target window class or process for testing (e.g. Notepad)\r\n"
                L"  --help, -h             Show this help screen\r\n\r\n"
                L"Hotkeys:\r\n"
                L"  Ctrl + Alt + C         Toggle boundary locking pause/resume\r\n"
                L"  Ctrl + Alt + End       Emergency Kill-Switch (Release cursor & exit)\r\n"
                L"  Ctrl + Alt + Q         Alternate Kill-Switch\r\n";
            WriteToOutput(helpText);
            exit(0);
        }
    }
}

// Ensures Per-Monitor DPI Awareness v2
static void InitializeDpiAwareness() {
    // Attempt SetProcessDpiAwarenessContext (Windows 10 1703+)
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        typedef BOOL(WINAPI* SetProcessDpiAwarenessContextProc)(DPI_AWARENESS_CONTEXT);
        auto setDpiContext = (SetProcessDpiAwarenessContextProc)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
        if (setDpiContext) {
            if (setDpiContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
                return;
            }
            if (setDpiContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE)) {
                return;
            }
        }
    }
    // Fallback for older systems
    SetProcessDPIAware();
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPWSTR /*lpCmdLine*/, int /*nCmdShow*/) {
    // 0. Single instance check: Prevent duplicate background processes
    HANDLE hSingleInstanceMutex = CreateMutexW(NULL, TRUE, L"mouseClip_SingleInstance_Mutex_9A7B3C");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(NULL,
            L"mouseClip is already active and running in your system tray!\n\n"
            L"Look for the crosshairs icon in the bottom-right of your taskbar (click the '^' arrow if it is hidden).\n\n"
            L"• Click icon: View live status & controls\n"
            L"• Ctrl + Alt + C: Pause / Resume locking\n"
            L"• Ctrl + Alt + End: Exit mouseClip",
            L"mouseClip Already Running",
            MB_OK | MB_ICONINFORMATION);
        if (hSingleInstanceMutex) CloseHandle(hSingleInstanceMutex);
        return 0;
    }

    // 1. Initialize DPI Awareness v2 first
    InitializeDpiAwareness();

#ifdef _CONSOLE
    g_config.enableConsole = true;
#endif

    // 2. Parse command-line flags
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        ParseCommandLine(argc, argv, g_config);
        LocalFree(argv);
    }

    // 3. Attach console if requested or if started in console mode
    if (g_config.enableConsole) {
        SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    }

    LogMessage("INIT", L"mouseClip v1.0 starting up...");
    LogMessage("INIT", L"DPI Awareness context initialized (PerMonitorV2).");

    // 4. Initialize core components
    TargetDetector detector;
    if (!g_config.customTestTarget.empty()) {
        detector.SetCustomTestTarget(g_config.customTestTarget);
        LogMessage("CONFIG", L"Custom test target active: " + g_config.customTestTarget);
    } else {
        LogMessage("CONFIG", L"Target criteria: Window Class 'RiotWindowClass' / Process 'League of Legends.exe'");
        LogMessage("CONFIG", L"Excluded: LeagueClient launcher & UxSubprocess");
    }

    ClipManager clipManager(detector);
    TrayManager trayManager;

    g_detector = &detector;
    g_clipManager = &clipManager;
    g_trayManager = &trayManager;

    // 5. Register state change callback for logging and tray icon updates
    static bool s_hasNotifiedConnect = false;
    clipManager.SetStateChangeCallback([](ClipState state, const TargetInfo& target, const ClipBounds& bounds) {
        if (g_trayManager && g_config.enableTray) {
            g_trayManager->UpdateState(state, target, bounds);
        }

        switch (state) {
        case ClipState::Locked: {
            std::wstring msg = L"BOUND: Locked cursor to [";
            msg += std::to_wstring(bounds.width) + L"x" + std::to_wstring(bounds.height) + L"] at screen (";
            msg += std::to_wstring(bounds.screenRect.left) + L"," + std::to_wstring(bounds.screenRect.top) + L" - ";
            msg += std::to_wstring(bounds.screenRect.right) + L"," + std::to_wstring(bounds.screenRect.bottom) + L") ";
            msg += L"| Process: " + target.processName + L" (PID " + std::to_wstring(target.processId) + L")";
            LogMessage("LOCK", msg);

            if (!s_hasNotifiedConnect && g_trayManager && g_config.enableTray) {
                s_hasNotifiedConnect = true;
                std::wstring toast = L"Boundary locked to League of Legends [" + std::to_wstring(bounds.width) + L"x" + std::to_wstring(bounds.height) + L"]. Press Ctrl+Alt+C to pause.";
                g_trayManager->ShowNotification(L"League of Legends Connected", toast);
            }
            break;
        }
        case ClipState::Idle: {
            LogMessage("RELEASE", L"Target window defocused/closed. Cursor bounds released.");
            if (target.processId == 0) {
                s_hasNotifiedConnect = false;
            }
            break;
        }
        case ClipState::Paused: {
            LogMessage("PAUSE", L"Boundary locking paused. Cursor bounds released.");
            break;
        }
        }
    });

    // 6. Create hidden top-level window (required for Shell_NotifyIcon and broadcast messages)
    const wchar_t CLASS_NAME[] = L"mouseClip_MessageWindow";
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClassExW(&wc)) {
        LogMessage("ERROR", L"Failed to register message window class.");
        return 1;
    }

    g_hwndMsg = CreateWindowExW(
        0, CLASS_NAME, L"mouseClip_MessageReceiver",
        WS_OVERLAPPED, 0, 0, 0, 0,
        NULL, NULL, hInstance, NULL
    );

    if (!g_hwndMsg) {
        LogMessage("ERROR", L"Failed to create message window.");
        return 1;
    }

    // 7. Register Global Hotkeys
    // Ctrl + Alt + C: Toggle Pause/Resume
    RegisterHotKey(g_hwndMsg, HOTKEY_ID_TOGGLE, MOD_CONTROL | MOD_ALT, 'C');
    // Ctrl + Alt + End: Clean Emergency Exit
    RegisterHotKey(g_hwndMsg, HOTKEY_ID_EXIT_END, MOD_CONTROL | MOD_ALT, VK_END);
    // Ctrl + Alt + Q: Alternate Clean Exit
    RegisterHotKey(g_hwndMsg, HOTKEY_ID_EXIT_Q, MOD_CONTROL | MOD_ALT, 'Q');

    LogMessage("HOTKEY", L"Hotkeys registered: Ctrl+Alt+C (Toggle) | Ctrl+Alt+End / Ctrl+Alt+Q (Kill-Switch)");

    // 8. Initialize System Tray
    if (g_config.enableTray) {
        if (trayManager.Initialize(g_hwndMsg)) {
            trayManager.UpdateState(ClipState::Idle, TargetInfo{}, ClipBounds{});
            LogMessage("TRAY", L"System tray icon successfully initialized.");
        } else {
            LogMessage("WARN", L"Failed to initialize system tray icon. Error code: " + std::to_wstring(GetLastError()));
        }
    }

    // 9. Install WinEventHook for EVENT_SYSTEM_FOREGROUND
    // Out-of-context hook: zero DLL injection, zero game thread overhead, completely Vanguard safe!
    g_hHookForeground = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND,
        EVENT_SYSTEM_FOREGROUND,
        NULL,
        WinEventProc,
        0,
        0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS
    );

    if (g_hHookForeground) {
        LogMessage("HOOK", L"WinEventHook (EVENT_SYSTEM_FOREGROUND) installed successfully.");
    } else {
        LogMessage("WARN", L"Failed to install WinEventHook; relying exclusively on timer heartbeat.");
    }

    // 10. Start Heartbeat Timer (handles in-game menu/shop resets, resolution shifts)
    SetTimer(g_hwndMsg, IDT_HEARTBEAT, g_config.heartbeatIntervalMs, NULL);
    LogMessage("TIMER", L"Heartbeat timer started (" + std::to_wstring(g_config.heartbeatIntervalMs) + L"ms interval).");

    // Perform initial state evaluation
    clipManager.Update();

    LogMessage("READY", L"mouseClip is active and monitoring. Press Ctrl+Alt+End or right-click tray icon to exit.");

    // 11. Standard Win32 Message Pump (sleeps efficiently in GetMessage with 0% CPU)
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // 12. Graceful Cleanup
    LogMessage("SHUTDOWN", L"Shutting down cleanly...");

    if (g_hHookForeground) {
        UnhookWinEvent(g_hHookForeground);
        g_hHookForeground = nullptr;
    }

    KillTimer(g_hwndMsg, IDT_HEARTBEAT);
    UnregisterHotKey(g_hwndMsg, HOTKEY_ID_TOGGLE);
    UnregisterHotKey(g_hwndMsg, HOTKEY_ID_EXIT_END);
    UnregisterHotKey(g_hwndMsg, HOTKEY_ID_EXIT_Q);

    if (g_trayManager) {
        g_trayManager->Remove();
    }

    // Absolutely ensure cursor is free
    clipManager.Release();
    ClipCursor(NULL);

    if (hSingleInstanceMutex) {
        ReleaseMutex(hSingleInstanceMutex);
        CloseHandle(hSingleInstanceMutex);
    }

    LogMessage("SHUTDOWN", L"Cursor released. Goodbye!");
    return 0;
}

// Standard main entry point so binary can also be compiled with /SUBSYSTEM:CONSOLE seamlessly
int main(int /*argc*/, char* /*argv*/[]) {
    // Convert to wide arguments and forward to wWinMain
    int wArgc = 0;
    LPWSTR* wArgv = CommandLineToArgvW(GetCommandLineW(), &wArgc);
    HINSTANCE hInst = GetModuleHandleW(NULL);
    int result = wWinMain(hInst, NULL, GetCommandLineW(), SW_SHOWDEFAULT);
    if (wArgv) {
        LocalFree(wArgv);
    }
    return result;
}
