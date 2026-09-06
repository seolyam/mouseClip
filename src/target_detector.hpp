#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>

namespace mouseclip {

struct TargetInfo {
    bool isMatch = false;
    HWND hwnd = nullptr;
    std::wstring className;
    std::wstring processName;
    std::wstring fullPath;
    DWORD processId = 0;
};

class TargetDetector {
public:
    TargetDetector();
    ~TargetDetector() = default;

    // Sets an optional custom target name (for testing/diagnostics, e.g. "Notepad.exe" or "Notepad")
    void SetCustomTestTarget(const std::wstring& targetName);
    bool HasCustomTestTarget() const;
    const std::wstring& GetCustomTestTarget() const;

    // Evaluates whether the given HWND matches the League of Legends match runtime
    TargetInfo InspectWindow(HWND hwnd) const;

    // Checks the current active foreground window
    TargetInfo InspectForeground() const;

    // Helper: extracts filename from a full path
    static std::wstring ExtractFileName(const std::wstring& fullPath);

    // Helper: case-insensitive string equality / substring search
    static bool EqualsIgnoreCase(const std::wstring& a, const std::wstring& b);
    static bool ContainsIgnoreCase(const std::wstring& haystack, const std::wstring& needle);

private:
    std::wstring m_customTestTarget;
};

} // namespace mouseclip
