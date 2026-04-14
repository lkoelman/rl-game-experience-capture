while ($true) {
    # Get current time and convert to Unix timestamp
    # $unixTimestamp = [double]::Parse((Get-Date (Get-Date).ToUniversalTime() -UFormat %s))
    $unixTimestamp = Get-Date -UFormat %s
    
    # Print timestamp
    Write-Host $unixTimestamp
    
    # Wait 5ms 
    Start-Sleep -Milliseconds 5
}