# Get all windows with titles
$windows = Get-Process | Where-Object {$_.MainWindowTitle} | Select-Object MainWindowTitle

# Create numbered list
$windowList = @{}
for ($i=1; $i -le $windows.Count; $i++) {
    $windowList[$i] = $windows[$i-1].MainWindowTitle
    Write-Host "$i : $($windows[$i-1].MainWindowTitle)"
}

# Get user selection
do {
    $selection = Read-Host "Enter the number of the window to select (1-$($windows.Count))"
    $valid = $selection -match '^\d+$' -and [int]$selection -ge 1 -and [int]$selection -le $windows.Count
    if (-not $valid) {
        Write-Host "Invalid selection. Please enter a number between 1 and $($windows.Count)"
    }
} while (-not $valid)

# Store selected window name
$selectedWindow = $windowList[[int]$selection]
Write-Host "Selected window: $selectedWindow"