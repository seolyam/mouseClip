#include "tray_manager.hpp"

namespace mouseclip {

TrayManager::TrayManager() = default;

TrayManager::~TrayManager() {
    Remove();
}

HICON TrayManager::CreateDynamicAppIcon() {
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    if (cx <= 0) cx = 16;
    if (cy <= 0) cy = 16;

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HDC hdcMask = CreateCompatibleDC(hdcScreen);

    HBITMAP hbmColor = CreateCompatibleBitmap(hdcScreen, cx, cy);
    HBITMAP hbmMask = CreateBitmap(cx, cy, 1, 1, NULL);

    HBITMAP hOldColor = (HBITMAP)SelectObject(hdcMem, hbmColor);
    HBITMAP hOldMask = (HBITMAP)SelectObject(hdcMask, hbmMask);

    RECT rc = { 0, 0, cx, cy };
    FillRect(hdcMem, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
    FillRect(hdcMask, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH)); // 1 is transparent

    // Draw boundary box (0 is opaque on mask)
    HBRUSH hBrushMaskOpaque = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RECT rcBox = { 1, 1, cx - 1, cy - 1 };
    FrameRect(hdcMask, &rcBox, hBrushMaskOpaque);

    // Crosshairs on mask
    int midX = cx / 2;
    int midY = cy / 2;
    for (int i = 3; i < cx - 3; ++i) {
        SetPixel(hdcMask, i, midY, RGB(0, 0, 0));
        SetPixel(hdcMask, midX, i, RGB(0, 0, 0));
    }

    // Color: Cyan boundary with Gold reticle
    COLORREF boundaryColor = RGB(0, 195, 255);
    COLORREF reticleColor = RGB(255, 200, 50);

    HBRUSH hColorBrush = CreateSolidBrush(boundaryColor);
    FrameRect(hdcMem, &rcBox, hColorBrush);
    DeleteObject(hColorBrush);

    for (int i = 3; i < cx - 3; ++i) {
        SetPixel(hdcMem, i, midY, reticleColor);
        SetPixel(hdcMem, midX, i, reticleColor);
    }

    SelectObject(hdcMem, hOldColor);
    SelectObject(hdcMask, hOldMask);
    DeleteDC(hdcMem);
    DeleteDC(hdcMask);
    ReleaseDC(NULL, hdcScreen);

    ICONINFO ii = { 0 };
    ii.fIcon = TRUE;
    ii.hbmColor = hbmColor;
    ii.hbmMask = hbmMask;
    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(hbmColor);
    DeleteObject(hbmMask);

    if (!hIcon) {
        hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    }
    return hIcon;
}

bool TrayManager::Initialize(HWND hwndOwner) {
    if (m_isInitialized) return true;

    m_hwndOwner = hwndOwner;
    m_hIcon = CreateDynamicAppIcon();

    ZeroMemory(&m_nid, sizeof(m_nid));
    m_nid.cbSize = sizeof(NOTIFYICONDATAW);
    m_nid.hWnd = hwndOwner;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;
    m_nid.hIcon = m_hIcon;

    wcscpy_s(m_nid.szTip, L"mouseClip: Initializing...");

    if (Shell_NotifyIconW(NIM_ADD, &m_nid)) {
        m_isInitialized = true;
        // Ensure standard tooltip behavior
        m_nid.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &m_nid);
        ShowNotification(L"mouseClip Active", L"Running in system tray. Monitoring for League of Legends match...");
        return true;
    }

    DWORD err = GetLastError();
    // Try fallback with standard application icon and basic size
    m_nid.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    m_nid.cbSize = NOTIFYICONDATAW_V2_SIZE;
    if (Shell_NotifyIconW(NIM_ADD, &m_nid)) {
        m_isInitialized = true;
        ShowNotification(L"mouseClip Active", L"Running in system tray. Monitoring for League of Legends match...");
        return true;
    }

    SetLastError(err);
    return false;
}

void TrayManager::ShowNotification(const std::wstring& title, const std::wstring& message, DWORD infoFlags) {
    if (!m_isInitialized) return;
    NOTIFYICONDATAW nid = m_nid;
    nid.uFlags |= NIF_INFO;
    nid.dwInfoFlags = infoFlags;
    wcsncpy_s(nid.szInfoTitle, title.c_str(), ARRAYSIZE(nid.szInfoTitle) - 1);
    wcsncpy_s(nid.szInfo, message.c_str(), ARRAYSIZE(nid.szInfo) - 1);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void TrayManager::UpdateState(ClipState state, const TargetInfo& /*target*/, const ClipBounds& bounds) {
    m_lastState = state;

    std::wstring tip;
    switch (state) {
    case ClipState::Locked: {
        tip = L"mouseClip: Bound to League of Legends [";
        tip += std::to_wstring(bounds.width) + L"x" + std::to_wstring(bounds.height) + L"]";
        m_statusString = L"Status: Active (Cursor Confined)";
        break;
    }
    case ClipState::Paused: {
        tip = L"mouseClip: Paused (Manual Override)";
        m_statusString = L"Status: Paused (Hotkey / Tray)";
        break;
    }
    case ClipState::Idle:
    default: {
        tip = L"mouseClip: Idle (Waiting for League of Legends)";
        m_statusString = L"Status: Idle (Game in background or inactive)";
        break;
    }
    }

    if (m_isInitialized) {
        wcsncpy_s(m_nid.szTip, tip.c_str(), ARRAYSIZE(m_nid.szTip) - 1);
        Shell_NotifyIconW(NIM_MODIFY, &m_nid);
    }
}

void TrayManager::ShowContextMenu(HWND hwndOwner) {
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    // Header
    AppendMenuW(hMenu, MF_STRING | MF_GRAYED | MF_DISABLED, IDM_TRAY_TITLE, L"mouseClip - Boundary Locker v1.0");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    // Current status item
    std::wstring statusLabel = m_statusString.empty() ? L"Status: Idle" : m_statusString;
    AppendMenuW(hMenu, MF_STRING | MF_GRAYED | MF_DISABLED, IDM_TRAY_STATUS, statusLabel.c_str());

    // Pause/Resume toggle
    std::wstring pauseLabel = (m_lastState == ClipState::Paused)
        ? L"Resume Boundary Lock (Ctrl+Alt+C)"
        : L"Pause Boundary Lock (Ctrl+Alt+C)";
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_PAUSE, pauseLabel.c_str());

    // Manual Refresh
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_REFRESH, L"Re-evaluate Bounds Now");

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    // Exit
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_EXIT, L"Exit (Ctrl+Alt+End)");

    // Required for Windows tray menus to dismiss properly on outside clicks
    SetForegroundWindow(hwndOwner);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwndOwner, NULL);
    PostMessageW(hwndOwner, WM_NULL, 0, 0);

    DestroyMenu(hMenu);
}

void TrayManager::Remove() {
    if (m_isInitialized) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        m_isInitialized = false;
    }
    if (m_hIcon) {
        DestroyIcon(m_hIcon);
        m_hIcon = nullptr;
    }
}

} // namespace mouseclip
