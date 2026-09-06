#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <iostream>

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int main(int argc, char* argv[]) {
    int staySeconds = 5;
    if (argc > 1) {
        staySeconds = atoi(argv[1]);
    }

    HINSTANCE hInstance = GetModuleHandle(NULL);
    const wchar_t CLASS_NAME[] = L"RiotWindowClass";

    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassExW(&wc)) {
        std::cerr << "Failed to register RiotWindowClass: " << GetLastError() << std::endl;
        return 1;
    }

    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, L"League of Legends (TM) Client [Mock]",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        200, 200, 1280, 720,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        std::cerr << "Failed to create window: " << GetLastError() << std::endl;
        return 1;
    }

    ShowWindow(hwnd, SW_SHOW);
    
    // Windows allows foreground switch if ALT key is pressed
    keybd_event(VK_MENU, 0, 0, 0);
    SetForegroundWindow(hwnd);
    keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);
    BringWindowToTop(hwnd);
    SetFocus(hwnd);

    UpdateWindow(hwnd);

    // Pump initial messages to let Windows process activation
    MSG msgInit;
    for (int i = 0; i < 15; ++i) {
        while (PeekMessageW(&msgInit, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msgInit);
            DispatchMessageW(&msgInit);
        }
        Sleep(20);
    }

    HWND fg = GetForegroundWindow();
    char title[256] = { 0 };
    GetWindowTextA(fg, title, sizeof(title));
    std::cout << "[MOCK_GAME] Foreground HWND: " << fg << " ('" << title << "'), Mock HWND: " << hwnd << std::endl;

    RECT rcClient = { 0 };
    GetClientRect(hwnd, &rcClient);
    POINT ptTL = { rcClient.left, rcClient.top };
    POINT ptBR = { rcClient.right, rcClient.bottom };
    ClientToScreen(hwnd, &ptTL);
    ClientToScreen(hwnd, &ptBR);

    std::cout << "[MOCK_GAME] Created RiotWindowClass window (HWND: " << hwnd << ")\n"
              << "[MOCK_GAME] Client Screen Bounds: ("
              << ptTL.x << ", " << ptTL.y << " - " << ptBR.x << ", " << ptBR.y << ")\n"
              << "[MOCK_GAME] Running for " << staySeconds << " seconds..." << std::endl;

    DWORD startTime = GetTickCount();
    MSG msg;
    while (GetTickCount() - startTime < (DWORD)(staySeconds * 1000)) {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) return 0;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        Sleep(50);
    }

    DestroyWindow(hwnd);
    std::cout << "[MOCK_GAME] Window destroyed, exiting." << std::endl;
    return 0;
}
