#include "clip_manager.hpp"

namespace mouseclip {

ClipManager::ClipManager(const TargetDetector& detector)
    : m_detector(detector)
{
}

ClipManager::~ClipManager() {
    Release();
}

void ClipManager::SetStateChangeCallback(StateChangeCallback callback) {
    m_onStateChange = callback;
}

bool ClipManager::AreRectsEqual(const RECT& r1, const RECT& r2) {
    return (r1.left == r2.left &&
            r1.top == r2.top &&
            r1.right == r2.right &&
            r1.bottom == r2.bottom);
}

bool ClipManager::GetWindowClientScreenRect(HWND hwnd, RECT* outRect) {
    if (!hwnd || !IsWindow(hwnd) || !outRect) {
        return false;
    }

    RECT rcClient = { 0, 0, 0, 0 };
    if (!GetClientRect(hwnd, &rcClient)) {
        return false;
    }

    // Convert client points to screen coordinates
    POINT ptTopLeft = { rcClient.left, rcClient.top };
    POINT ptBottomRight = { rcClient.right, rcClient.bottom };

    if (!ClientToScreen(hwnd, &ptTopLeft) || !ClientToScreen(hwnd, &ptBottomRight)) {
        return false;
    }

    outRect->left = ptTopLeft.x;
    outRect->top = ptTopLeft.y;
    outRect->right = ptBottomRight.x;
    outRect->bottom = ptBottomRight.y;

    return (outRect->right > outRect->left && outRect->bottom > outRect->top);
}

bool ClipManager::ApplyClip(const RECT& targetRect) {
    if (ClipCursor(&targetRect)) {
        m_appliedClipRect = targetRect;
        return true;
    }
    return false;
}

void ClipManager::Release() {
    ClipCursor(NULL);
    m_appliedClipRect = { 0, 0, 0, 0 };
}

void ClipManager::TogglePause() {
    SetPaused(!m_isPaused);
}

void ClipManager::SetPaused(bool paused) {
    if (m_isPaused == paused) return;

    m_isPaused = paused;
    if (m_isPaused) {
        Release();
        m_state = ClipState::Paused;
        if (m_onStateChange) {
            m_onStateChange(m_state, m_currentTarget, m_currentBounds);
        }
    } else {
        // Re-evaluate immediately upon unpause
        Update();
    }
}

bool ClipManager::IsPaused() const {
    return m_isPaused;
}

ClipState ClipManager::GetState() const {
    return m_state;
}

const TargetInfo& ClipManager::GetCurrentTarget() const {
    return m_currentTarget;
}

const ClipBounds& ClipManager::GetCurrentBounds() const {
    return m_currentBounds;
}

void ClipManager::Update(HWND hwndOverride) {
    if (m_isPaused) {
        if (m_state != ClipState::Paused) {
            Release();
            m_state = ClipState::Paused;
            if (m_onStateChange) {
                m_onStateChange(m_state, m_currentTarget, m_currentBounds);
            }
        }
        return;
    }

    TargetInfo target = (hwndOverride != NULL) ? m_detector.InspectWindow(hwndOverride) : m_detector.InspectForeground();

    if (!target.isMatch) {
        // Target is not in foreground (user alt-tabbed, game closed, or launcher active)
        bool stateChanged = (m_state != ClipState::Idle);
        if (stateChanged) {
            Release();
            m_state = ClipState::Idle;
            m_currentTarget = target;
            m_currentBounds = ClipBounds{};
            if (m_onStateChange) {
                m_onStateChange(m_state, m_currentTarget, m_currentBounds);
            }
        }
        return;
    }

    // Target IS in foreground: query current client rect in screen space
    RECT rcScreen = { 0, 0, 0, 0 };
    if (!GetWindowClientScreenRect(target.hwnd, &rcScreen)) {
        if (m_state == ClipState::Locked) {
            Release();
            m_state = ClipState::Idle;
            if (m_onStateChange) {
                m_onStateChange(m_state, m_currentTarget, m_currentBounds);
            }
        }
        return;
    }

    // Query OS-level active cursor clipping rectangle
    RECT rcCurrentOSClip = { 0, 0, 0, 0 };
    GetClipCursor(&rcCurrentOSClip);

    bool boundsChanged = !AreRectsEqual(m_appliedClipRect, rcScreen);
    bool osLostClip = !AreRectsEqual(rcCurrentOSClip, rcScreen);
    bool stateChanged = (m_state != ClipState::Locked);

    // Apply or reinforce boundary if bounds moved, state changed, or LoL/Windows dropped clip
    if (stateChanged || boundsChanged || osLostClip) {
        if (ApplyClip(rcScreen)) {
            m_state = ClipState::Locked;
            m_currentTarget = target;
            m_currentBounds.screenRect = rcScreen;
            m_currentBounds.width = rcScreen.right - rcScreen.left;
            m_currentBounds.height = rcScreen.bottom - rcScreen.top;

            if (m_onStateChange) {
                m_onStateChange(m_state, m_currentTarget, m_currentBounds);
            }
        }
    }
}

} // namespace mouseclip
