# Start the game and enter the last save in ~50 s. Verified 2026-11-17.
#  - "Continue" needs media_soviet\saved_last; killing the game loses it, so keep a backup
#    (saved_last_bak) and restore it, else copy the newest folder from media_soviet\save.
#  - Main menu is ready ~25 s after start; the top menu item "Продолжить" at window (510,505)
#    exists only when saved_last exists. Loading takes ~25 s more.
#  - tools\click.ps1 must be DPI-aware (Windows scale 125%).
# -Restart kills a running game first. -NoLoad stops at the main menu.
param([switch]$Restart, [switch]$NoLoad)
$root = "F:\steam\steamapps\common\SovietRepublic"
$ms = "$root\media_soviet"
$p = Get-Process SOVIET64 -ErrorAction SilentlyContinue
if ($p -and -not $Restart) { "already running"; exit }
if ($p) { $p | Stop-Process -Force; Start-Sleep 3 }
if (Test-Path "$ms\saved_last\header.bin") {
  Remove-Item "$ms\saved_last_bak" -Recurse -Force -EA 0
  Copy-Item "$ms\saved_last" "$ms\saved_last_bak" -Recurse
} elseif (Test-Path "$ms\saved_last_bak\header.bin") {
  Remove-Item "$ms\saved_last" -Recurse -Force -EA 0
  Copy-Item "$ms\saved_last_bak" "$ms\saved_last" -Recurse; "restored saved_last from backup"
} else {
  $n = Get-ChildItem "$ms\save" -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 1
  Remove-Item "$ms\saved_last" -Recurse -Force -EA 0
  Copy-Item $n.FullName "$ms\saved_last" -Recurse; "saved_last from $($n.Name)"
}
Start-Process -FilePath "$root\SOVIET64.exe" -WorkingDirectory $root
"started"
if ($NoLoad) { exit }
Start-Sleep 28
& "$PSScriptRoot\click.ps1" -X 510 -Y 505
Start-Sleep 25
& "$PSScriptRoot\shot.ps1" -Out "$env:TEMP\wrsr_loaded.png"
