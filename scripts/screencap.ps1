# Add DPI awareness at the start of the script
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public class DPI {
    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool SetProcessDPIAware();
}
"@
[DPI]::SetProcessDPIAware()

# Add Windows API functions
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Win32 {
    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
    public struct RECT
    {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }
}
"@

# Get all windows with titles and handles
$windows = Get-Process | Where-Object {$_.MainWindowTitle} | Select-Object MainWindowTitle, MainWindowHandle

# Create numbered list with coordinates
$windowList = @{}
for ($i=1; $i -le $windows.Count; $i++) {
    $rect = New-Object Win32+RECT
    $handle = $windows[$i-1].MainWindowHandle
    [Win32]::GetWindowRect($handle, [ref]$rect) | Out-Null
    
    # Adjust for window decorations (approximately 8px border, 31px titlebar)
    $windowInfo = @{
        'Title' = $windows[$i-1].MainWindowTitle
        'X' = $rect.Left + 8
        'Y' = $rect.Top + 31
        'Width' = ($rect.Right - $rect.Left) - 16  # Subtract left and right borders
        'Height' = ($rect.Bottom - $rect.Top) - 39  # Subtract top and bottom borders + titlebar
    }
    
    $windowList[$i] = $windowInfo
    Write-Host "$i : $($windowInfo.Title) [X:$($windowInfo.X), Y:$($windowInfo.Y), W:$($windowInfo.Width), H:$($windowInfo.Height)]"
}

# Get user selection
do {
    $selection = Read-Host "Enter the number of the window to select (1-$($windows.Count))"
    $valid = $selection -match '^\d+$' -and [int]$selection -ge 1 -and [int]$selection -le $windows.Count
    if (-not $valid) {
        Write-Host "Invalid selection. Please enter a number between 1 and $($windows.Count)"
    }
} while (-not $valid)

# Store selected window info
$selectedWindow = $windowList[[int]$selection]
$windowTitle = $selectedWindow.Title
$windowX = $selectedWindow.X
$windowY = $selectedWindow.Y
$windowWidth = $selectedWindow.Width
$windowHeight = $selectedWindow.Height

Write-Host "Selected window: $windowTitle"
Write-Host "Position: X=$windowX, Y=$windowY"
Write-Host "Size: Width=$windowWidth, Height=$windowHeight"

# Start screen capture
$capture_start_iso = Get-Date -Format "yyyy-MM-ddTHH:mm:ss.fffK"
$capture_start_unix = Get-Date -UFormat %s
ffmpeg -f gdigrab -framerate 30 -offset_x $windowX -offset_y $windowY `
    -video_size "${windowWidth}x${windowHeight}" `
    -i desktop -vf "scale=1280:-1" -movflags use_metadata_tags `
    -metadata creation_time_iso8601="$capture_start_iso" `
    -metadata creation_time_unix="$capture_start_unix" `
    "${capture_start_unix}_capture.mkv"



# If you want to limit to a region, and show the area being grabbed:
# (method='region')
# >>> ffmpeg -f gdigrab -framerate 30 -offset_x 10 -offset_y 20 -video_size 640x480 -show_region 1 -i desktop output.mkv

# To grab the contents of the window named "Calculator":
# (method=window-contents)
# >>> ffmpeg -f gdigrab -framerate 30 -i title=Calculator output.mkv

# Use Windows 8+ Desktop Duplication API
# (method=desktop-gpu)
# >>> ffmpeg -init_hw_device d3d11va -filter_complex ddagrab=0 -c:v h264_nvenc -cq:v 20 output.mkv

# Or downloaded and processed as usual, though with a bit of overhead compared to pure on-GPU processing:
# (method=desktop-cpu)
# >>> ffmpeg -filter_complex ddagrab=0,hwdownload,format=bgra -c:v libx264 -crf 20 output.mkv
