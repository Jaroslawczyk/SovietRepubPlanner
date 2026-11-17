# Starts the game; the last save autoloads by itself (no clicks needed).
# -Restart kills a running game first. Wait ~120 s before testing (loading screen ~1-2 min).
param([switch]$Restart)
$root = "F:\steam\steamapps\common\SovietRepublic"
$p = Get-Process SOVIET64 -ErrorAction SilentlyContinue
if ($p -and $Restart) { $p | Stop-Process -Force; Start-Sleep 3; $p = $null }
if ($p) { "already running"; exit }
Start-Process -FilePath "$root\SOVIET64.exe" -WorkingDirectory $root
"started"
