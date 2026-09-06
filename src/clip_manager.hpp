#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <functional>
#include <string>
#include "target_detector.hpp"

namespace mouseclip {

enum class ClipState {
    Idle,    // Target window not in foreground; cursor is unconfined
    Locked,  // Target window is active foreground; cursor is confined to client rect
    Paused   // User manually paused confinement via hotkey or tray
};

struct ClipBounds {
    RECT screenRect = { 0, 0, 0, 0 };
    int width = 0;
    int height = 0;
};

using StateChangeCallback = std::function<void(ClipState newState, const TargetInfo& target, const ClipBounds& bounds)>;

class ClipManager {
public:
    ClipManager(const TargetDetector& detector);
    ~ClipManager();

    // Sets callback invoked when state or bounds change
    void SetStateChangeCallback(StateChangeCallback callback);

    // Updates state based on current foreground window (or window override)
    // Call this on foreground change events and on timer ticks
    void Update(HWND hwndOverride = NULL);

    // Forces an immediate release of cursor clipping (ClipCursor(NULL))
    void Release();

    // Toggles pause mode (manual override)
    void TogglePause();
    void SetPaused(bool paused);
    bool IsPaused() const;

    // Current status queries
    ClipState GetState() const;
    const TargetInfo& GetCurrentTarget() const;
    const ClipBounds& GetCurrentBounds() const;

    // Direct helper: calculates screen-space client bounds for a window
    static bool GetWindowClientScreenRect(HWND hwnd, RECT* outRect);

    // Compares two RECT structs for equality
    static bool AreRectsEqual(const RECT& r1, const RECT& r2);

private:
    bool ApplyClip(const RECT& targetRect);

    const TargetDetector& m_detector;
    ClipState m_state = ClipState::Idle;
    bool m_isPaused = false;
    TargetInfo m_currentTarget;
    ClipBounds m_currentBounds;
    RECT m_appliedClipRect = { 0, 0, 0, 0 };

    StateChangeCallback m_onStateChange;
};

} // namespace mouseclip
