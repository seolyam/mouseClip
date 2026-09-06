# Automated Functional Verification Test for mouseClip
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "      mouseClip Automated Verification & Boundary Test       " -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

$logFile = "$PSScriptRoot\test_run.log"
if (Test-Path $logFile) { Remove-Item $logFile -Force }

# Win32 Native coordinate query helper
Add-Type @"
    using System;
    using System.Runtime.InteropServices;
    public class WinOps {
        [DllImport("user32.dll")]
        public static extern bool GetClipCursor(out RECT lpRect);
        [DllImport("user32.dll")]
        public static extern bool SetForegroundWindow(IntPtr hWnd);
        [StructLayout(LayoutKind.Sequential)]
        public struct RECT {
            public int Left;
            public int Top;
            public int Right;
            public int Bottom;
            public override string ToString() {
                return string.Format("({0}, {1} - {2}, {3}) [Width: {4}, Height: {5}]", 
                    Left, Top, Right, Bottom, Right - Left, Bottom - Top);
            }
        }
    }
"@

$initialClip = New-Object WinOps+RECT
[WinOps]::GetClipCursor([ref]$initialClip)
Write-Host "[1] Initial OS Cursor Clip: $initialClip" -ForegroundColor Yellow

# 1. Start mouseClip_debug (targeting RiotWindowClass)
Write-Host "[2] Launching mouseClip_debug.exe..." -ForegroundColor Yellow
$mouseClip = Start-Process -FilePath "$PSScriptRoot\bin\mouseClip_debug.exe" `
    -RedirectStandardOutput $logFile `
    -PassThru

Start-Sleep -Milliseconds 800

# 2. Launch mock_game.exe (creates RiotWindowClass window for 4 seconds)
Write-Host "[3] Launching mock_game.exe with RiotWindowClass..." -ForegroundColor Yellow
$mockGame = Start-Process -FilePath "$PSScriptRoot\bin\mock_game.exe" -ArgumentList "4" -PassThru
Start-Sleep -Milliseconds 1200

# 3. Query OS cursor clipping boundary while game is active
$gameClip = New-Object WinOps+RECT
[WinOps]::GetClipCursor([ref]$gameClip)
Write-Host "[4] OS Cursor Clip during game active: $gameClip" -ForegroundColor Green

# 4. Wait for mock_game to finish and close
Write-Host "[5] Waiting for mock_game to exit..." -ForegroundColor Yellow
$mockGame.WaitForExit()
Start-Sleep -Milliseconds 800

# 5. Query OS cursor clipping boundary after game has closed
$afterClip = New-Object WinOps+RECT
[WinOps]::GetClipCursor([ref]$afterClip)
Write-Host "[6] OS Cursor Clip after game closed: $afterClip" -ForegroundColor Green

# 6. Terminate mouseClip cleanly
Write-Host "[7] Stopping mouseClip..." -ForegroundColor Yellow
Stop-Process -Id $mouseClip.Id -Force
Start-Sleep -Milliseconds 400

# 7. Print diagnostic logs
Write-Host "`n============================================================" -ForegroundColor Cyan
Write-Host "                mouseClip Diagnostic Logs                    " -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
if (Test-Path $logFile) {
    Get-Content $logFile
} else {
    Write-Host "No log file found."
}

# 8. Assertions
Write-Host "`n============================================================" -ForegroundColor Cyan
Write-Host "                     Test Verification                      " -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

$gameWidth = $gameClip.Right - $gameClip.Left
$gameHeight = $gameClip.Bottom - $gameClip.Top

$initialWidth = $initialClip.Right - $initialClip.Left
$initialHeight = $initialClip.Bottom - $initialClip.Top

$afterWidth = $afterClip.Right - $afterClip.Left
$afterHeight = $afterClip.Bottom - $afterClip.Top

$lockedSuccess = ($gameWidth -lt $initialWidth) -and ($gameHeight -lt $initialHeight)
$releasedSuccess = ($afterWidth -eq $initialWidth) -and ($afterHeight -eq $initialHeight)

if ($lockedSuccess) {
    Write-Host "[PASS] Cursor bounds were successfully locked to the RiotWindowClass window!" -ForegroundColor Green
} else {
    Write-Host "[FAIL] Cursor bounds were not restricted during game runtime." -ForegroundColor Red
}

if ($releasedSuccess) {
    Write-Host "[PASS] Cursor bounds were immediately and cleanly released upon game exit!" -ForegroundColor Green
} else {
    Write-Host "[FAIL] Cursor bounds were not restored after game closed." -ForegroundColor Red
}

if ($lockedSuccess -and $releasedSuccess) {
    Write-Host "`n*** ALL FUNCTIONAL CHECKS PASSED ***" -ForegroundColor Green
} else {
    Write-Host "`n*** CHECKS FAILED ***" -ForegroundColor Red
}
