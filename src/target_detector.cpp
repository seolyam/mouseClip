#include "target_detector.hpp"
#include <algorithm>
#include <cwctype>

namespace mouseclip {

TargetDetector::TargetDetector() = default;

void TargetDetector::SetCustomTestTarget(const std::wstring& targetName) {
    m_customTestTarget = targetName;
}

bool TargetDetector::HasCustomTestTarget() const {
    return !m_customTestTarget.empty();
}

const std::wstring& TargetDetector::GetCustomTestTarget() const {
    return m_customTestTarget;
}

std::wstring TargetDetector::ExtractFileName(const std::wstring& fullPath) {
    if (fullPath.empty()) return L"";
    size_t lastSlash = fullPath.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos && lastSlash + 1 < fullPath.length()) {
        return fullPath.substr(lastSlash + 1);
    }
    return fullPath;
}

bool TargetDetector::EqualsIgnoreCase(const std::wstring& a, const std::wstring& b) {
    if (a.length() != b.length()) return false;
    for (size_t i = 0; i < a.length(); ++i) {
        if (std::towlower(a[i]) != std::towlower(b[i])) {
            return false;
        }
    }
    return true;
}

bool TargetDetector::ContainsIgnoreCase(const std::wstring& haystack, const std::wstring& needle) {
    if (needle.empty()) return true;
    if (haystack.length() < needle.length()) return false;

    auto it = std::search(
        haystack.begin(), haystack.end(),
        needle.begin(), needle.end(),
        [](wchar_t ch1, wchar_t ch2) {
            return std::towlower(ch1) == std::towlower(ch2);
        }
    );
    return (it != haystack.end());
}

TargetInfo TargetDetector::InspectWindow(HWND hwnd) const {
    TargetInfo info;
    info.hwnd = hwnd;
    info.isMatch = false;

    if (!hwnd || !IsWindow(hwnd)) {
        return info;
    }

    // Do not clip if the window is minimized or invisible
    if (IsIconic(hwnd) || !IsWindowVisible(hwnd)) {
        return info;
    }

    // 1. Query window class name
    WCHAR szClassName[256] = { 0 };
    if (GetClassNameW(hwnd, szClassName, ARRAYSIZE(szClassName)) > 0) {
        info.className = szClassName;
    }

    // 2. Query process ID
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    info.processId = pid;

    if (pid == 0) {
        return info;
    }

    // 3. Query process image path using PROCESS_QUERY_LIMITED_INFORMATION
    // This is safe, unprivileged, standard user32/kernel32 call compliant with Vanguard
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProcess) {
        WCHAR szPath[1024] = { 0 };
        DWORD dwSize = ARRAYSIZE(szPath);
        if (QueryFullProcessImageNameW(hProcess, 0, szPath, &dwSize)) {
            info.fullPath = szPath;
            info.processName = ExtractFileName(info.fullPath);
        }
        CloseHandle(hProcess);
    }

    // 4. Check for Custom Test Target (if specified via --test-target)
    if (!m_customTestTarget.empty()) {
        bool classMatches = ContainsIgnoreCase(info.className, m_customTestTarget);
        bool processMatches = ContainsIgnoreCase(info.processName, m_customTestTarget);
        if (classMatches || processMatches) {
            info.isMatch = true;
            return info;
        }
    }

    // 5. Explicit Negative Filters (League Client Launcher / CEF / Chromium base)
    // Exclude UxSubprocess, LeagueClient.exe, LeagueClientUx.exe, LeagueClientUxRender.exe
    if (ContainsIgnoreCase(info.processName, L"LeagueClient") ||
        ContainsIgnoreCase(info.processName, L"UxSubprocess")) {
        info.isMatch = false;
        return info;
    }

    // 6. Target Identification for League of Legends In-Match Game Runtime:
    // Primary criterion: Window Class is "RiotWindowClass"
    // Secondary verification: Process is "League of Legends.exe"
    bool isRiotClass = EqualsIgnoreCase(info.className, L"RiotWindowClass");
    bool isLolExe = EqualsIgnoreCase(info.processName, L"League of Legends.exe");

    // Match if class is RiotWindowClass or process is League of Legends.exe
    if (isRiotClass || isLolExe) {
        info.isMatch = true;
    }

    return info;
}

TargetInfo TargetDetector::InspectForeground() const {
    HWND hwndForeground = GetForegroundWindow();
    return InspectWindow(hwndForeground);
}

} // namespace mouseclip
