#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <iostream>
#include <cassert>
#include "../src/target_detector.hpp"
#include "../src/clip_manager.hpp"

using namespace mouseclip;

static int g_passCount = 0;
static int g_failCount = 0;

#define TEST_ASSERT(expr, msg) \
    do { \
        if (expr) { \
            std::wcout << L"  [PASS] " << msg << std::endl; \
            g_passCount++; \
        } else { \
            std::wcout << L"  [FAIL] " << msg << std::endl; \
            g_failCount++; \
        } \
    } while (0)

LRESULT CALLBACK DummyWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void Test_DpiAwareness() {
    std::wcout << L"\n--- Test Suite 1: Per-Monitor DPI Awareness v2 ---" << std::endl;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    DPI_AWARENESS_CONTEXT ctx = GetThreadDpiAwarenessContext();
    DPI_AWARENESS awareness = GetAwarenessFromDpiAwarenessContext(ctx);
    
    TEST_ASSERT(awareness == DPI_AWARENESS_PER_MONITOR_AWARE,
        L"Process awareness context is PER_MONITOR_AWARE");
}

void Test_TargetDetector() {
    std::wcout << L"\n--- Test Suite 2: Target Identification & Launcher Isolation ---" << std::endl;

    HINSTANCE hInst = GetModuleHandle(NULL);
    TargetDetector detector;

    // 1. Register and create RiotWindowClass window
    const wchar_t RIOT_CLASS[] = L"RiotWindowClass";
    WNDCLASSEXW wcRiot = { 0 };
    wcRiot.cbSize = sizeof(wcRiot);
    wcRiot.lpfnWndProc = DummyWndProc;
    wcRiot.hInstance = hInst;
    wcRiot.lpszClassName = RIOT_CLASS;
    RegisterClassExW(&wcRiot);

    HWND hwndRiot = CreateWindowExW(
        0, RIOT_CLASS, L"League of Legends (TM) Client",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        100, 100, 800, 600,
        NULL, NULL, hInst, NULL
    );

    TEST_ASSERT(hwndRiot != NULL, L"Created RiotWindowClass test window");

    TargetInfo infoRiot = detector.InspectWindow(hwndRiot);
    TEST_ASSERT(infoRiot.isMatch == true, L"TargetDetector successfully identified RiotWindowClass");
    TEST_ASSERT(infoRiot.className == L"RiotWindowClass", L"Window class name correctly extracted as RiotWindowClass");

    // 2. Register and create generic/launcher window
    const wchar_t OTHER_CLASS[] = L"CefBrowserWindow";
    WNDCLASSEXW wcOther = { 0 };
    wcOther.cbSize = sizeof(wcOther);
    wcOther.lpfnWndProc = DummyWndProc;
    wcOther.hInstance = hInst;
    wcOther.lpszClassName = OTHER_CLASS;
    RegisterClassExW(&wcOther);

    HWND hwndOther = CreateWindowExW(
        0, OTHER_CLASS, L"LeagueClientUx",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        100, 100, 800, 600,
        NULL, NULL, hInst, NULL
    );

    TEST_ASSERT(hwndOther != NULL, L"Created CefBrowserWindow (client launcher) test window");

    TargetInfo infoOther = detector.InspectWindow(hwndOther);
    TEST_ASSERT(infoOther.isMatch == false, L"TargetDetector correctly ignored non-Riot client window");

    DestroyWindow(hwndRiot);
    DestroyWindow(hwndOther);
}

void Test_CoordinateConversion() {
    std::wcout << L"\n--- Test Suite 3: Coordinate Conversion & Bounds Math ---" << std::endl;

    HINSTANCE hInst = GetModuleHandle(NULL);
    const wchar_t TEST_CLASS[] = L"TestCoordClass";
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DummyWndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = TEST_CLASS;
    RegisterClassExW(&wc);

    int posX = 250, posY = 180, width = 640, height = 480;
    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST, TEST_CLASS, L"CoordTest",
        WS_POPUP | WS_VISIBLE, // borderless popup for exact 1:1 client coordinates
        posX, posY, width, height,
        NULL, NULL, hInst, NULL
    );

    TEST_ASSERT(hwnd != NULL, L"Created borderless popup test window");

    RECT rcScreen = { 0 };
    bool ok = ClipManager::GetWindowClientScreenRect(hwnd, &rcScreen);

    TEST_ASSERT(ok == true, L"GetWindowClientScreenRect succeeded");
    TEST_ASSERT(rcScreen.left == posX, L"Screen left matches physical coordinate");
    TEST_ASSERT(rcScreen.top == posY, L"Screen top matches physical coordinate");
    TEST_ASSERT(rcScreen.right - rcScreen.left == width, L"Client width matches requested width");
    TEST_ASSERT(rcScreen.bottom - rcScreen.top == height, L"Client height matches requested height");

    DestroyWindow(hwnd);
}

void Test_CursorClippingAndRelease() {
    std::wcout << L"\n--- Test Suite 4: Win32 ClipCursor Boundary Locking & Graceful Release ---" << std::endl;

    RECT initialClip = { 0 };
    GetClipCursor(&initialClip);

    // Apply a constrained boundary
    RECT targetBounds = { 200, 200, 600, 500 };
    BOOL clipResult = ClipCursor(&targetBounds);
    TEST_ASSERT(clipResult == TRUE, L"ClipCursor(&targetBounds) returned TRUE");

    RECT activeClip = { 0 };
    GetClipCursor(&activeClip);
    TEST_ASSERT(ClipManager::AreRectsEqual(activeClip, targetBounds),
        L"GetClipCursor matches exact restricted target boundary");

    // Release cursor
    BOOL releaseResult = ClipCursor(NULL);
    TEST_ASSERT(releaseResult == TRUE, L"ClipCursor(NULL) returned TRUE");

    RECT releasedClip = { 0 };
    GetClipCursor(&releasedClip);
    TEST_ASSERT(ClipManager::AreRectsEqual(releasedClip, initialClip),
        L"GetClipCursor restored to full virtual desktop rectangle");
}

void Test_ClipManagerLifecycle() {
    std::wcout << L"\n--- Test Suite 5: ClipManager State Machine & Pause Mode ---" << std::endl;

    TargetDetector detector;
    ClipManager manager(detector);

    TEST_ASSERT(manager.GetState() == ClipState::Idle, L"Initial state is ClipState::Idle");
    TEST_ASSERT(!manager.IsPaused(), L"Initial pause state is false");

    manager.TogglePause();
    TEST_ASSERT(manager.IsPaused(), L"IsPaused is true after TogglePause()");
    TEST_ASSERT(manager.GetState() == ClipState::Paused, L"State transitioned to ClipState::Paused");

    manager.TogglePause();
    TEST_ASSERT(!manager.IsPaused(), L"IsPaused is false after second TogglePause()");

    manager.Release();
    TEST_ASSERT(manager.GetState() != ClipState::Locked, L"State is not locked after Release()");
}

void Test_DynamicBoundaryLockingAndRelease() {
    std::wcout << L"\n--- Test Suite 6: End-to-End Boundary Locking & Auto-Release ---" << std::endl;

    RECT initialClip = { 0 };
    GetClipCursor(&initialClip);

    HINSTANCE hInst = GetModuleHandle(NULL);
    const wchar_t RIOT_CLASS[] = L"RiotWindowClass";
    HWND hwndRiot = CreateWindowExW(
        0, RIOT_CLASS, L"League of Legends (TM) Client",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        150, 150, 800, 600,
        NULL, NULL, hInst, NULL
    );

    TEST_ASSERT(hwndRiot != NULL, L"Created RiotWindowClass game window for end-to-end test");

    TargetDetector detector;
    ClipManager manager(detector);

    // 1. Simulate target window active
    manager.Update(hwndRiot);
    TEST_ASSERT(manager.GetState() == ClipState::Locked, L"ClipManager entered Locked state on target window");

    RECT lockedClip = { 0 };
    GetClipCursor(&lockedClip);
    TEST_ASSERT(ClipManager::AreRectsEqual(lockedClip, manager.GetCurrentBounds().screenRect),
        L"OS cursor successfully clamped to RiotWindowClass client coordinates");

    // 2. Simulate focus loss (alt-tab or foreground switch)
    manager.Update(NULL);
    TEST_ASSERT(manager.GetState() == ClipState::Idle, L"ClipManager transitioned to Idle on focus loss");

    RECT unclipped = { 0 };
    GetClipCursor(&unclipped);
    TEST_ASSERT(ClipManager::AreRectsEqual(unclipped, initialClip),
        L"OS cursor automatically and cleanly released back to full desktop");

    DestroyWindow(hwndRiot);
}

int main() {
    std::wcout << L"============================================================" << std::endl;
    std::wcout << L"            mouseClip Systems & Win32 Unit Tests            " << std::endl;
    std::wcout << L"============================================================" << std::endl;

    Test_DpiAwareness();
    Test_TargetDetector();
    Test_CoordinateConversion();
    Test_CursorClippingAndRelease();
    Test_ClipManagerLifecycle();
    Test_DynamicBoundaryLockingAndRelease();

    std::wcout << L"\n============================================================" << std::endl;
    std::wcout << L"RESULTS: " << g_passCount << L" passed, " << g_failCount << L" failed." << std::endl;
    std::wcout << L"============================================================" << std::endl;

    return (g_failCount == 0) ? 0 : 1;
}
