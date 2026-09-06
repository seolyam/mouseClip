#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <string>
#include "clip_manager.hpp"

namespace mouseclip {

// Custom Windows message for Tray notifications
constexpr UINT WM_TRAYICON = WM_USER + 101;

// Menu Command IDs
constexpr UINT IDM_TRAY_TITLE   = 2001;
constexpr UINT IDM_TRAY_STATUS  = 2002;
constexpr UINT IDM_TRAY_PAUSE   = 2003;
constexpr UINT IDM_TRAY_REFRESH = 2004;
constexpr UINT IDM_TRAY_EXIT    = 2005;

class TrayManager {
public:
    TrayManager();
    ~TrayManager();

    // Initializes the tray icon associated with the given window handle
    bool Initialize(HWND hwndOwner);

    // Updates the tray tooltip and menu status based on current clip state
    void UpdateState(ClipState state, const TargetInfo& target, const ClipBounds& bounds);

    // Displays the popup context menu at cursor position
    void ShowContextMenu(HWND hwndOwner);

    // Removes the icon from the notification area
    void Remove();

    // Check if initialized
    bool IsInitialized() const { return m_isInitialized; }

private:
    HICON CreateDynamicAppIcon();

    HWND m_hwndOwner = nullptr;
    NOTIFYICONDATAW m_nid = { 0 };
    HICON m_hIcon = nullptr;
    bool m_isInitialized = false;
    ClipState m_lastState = ClipState::Idle;
    std::wstring m_statusString;
};

} // namespace mouseclip
