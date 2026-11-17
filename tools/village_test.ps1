# In-game village test: restart the game into the save, build the selftest village (mode 4) with the instant build,
# print the build summary and how many footpath ends snapped to something.
# -Snap: footpath snap mode (0 dispatcher, 1 0x3d4a80), -Off: village link end beyond the planned road edge (m)
param([string]$Snap = '1', [string]$Off = '2.0')
$g = 'F:\steam\steamapps\common\SovietRepublic'
$t = "$g\DistrictPlanner\tools"
Set-Content "$g\DistrictPlanner.linkoff" $Off
Stop-Process -Name SOVIET64 -Force -ErrorAction SilentlyContinue; Start-Sleep 3
Copy-Item "$g\DistrictPlanner\build\dinput8.dll" "$g\dinput8.dll" -Force
powershell -NoProfile -File "$t\launch.ps1" -Restart | Out-Null
Start-Sleep 15
Set-Content "$g\DistrictPlanner.selftest" "4"
for ($i = 0; $i -lt 60; $i++) {
    Start-Sleep 2
    if (-not (Test-Path "$g\DistrictPlanner.selftest")) { break }
    if ($i -eq 15 -or $i -eq 35) { & "$t\click.ps1" -X 510 -Y 505 | Out-Null }   # still in the main menu: "Продолжить" again
}
if (Test-Path "$g\DistrictPlanner.selftest") { "selftest not started (save not loaded)"; exit 1 }
Set-Content "$g\DistrictPlanner.snapmode" $Snap
Start-Sleep 6
$s0 = (Get-Content "$g\DistrictPlanner.log").Count
New-Item "$g\DistrictPlanner.instant" -ItemType File -Force | Out-Null
for ($i = 0; $i -lt 100; $i++) { Start-Sleep 3; $c = Get-Content "$g\DistrictPlanner.log"; if ($c[$s0..($c.Count - 1)] -match "summary") { break } }
$c = Get-Content "$g\DistrictPlanner.log"; $r = $c[$s0..($c.Count - 1)]
"== snap $Snap off $Off"
$r | Select-String "summary" | ForEach-Object Line
$sn = $r | Select-String "SNAP mgr\+0xf968"
$hit = $sn | Where-Object { $_.Line -match '\| b.*objs (?!0000000000000000 0000000000000000 0000000000000000 0000000000000000)' }
"footpath ends snapped: $($hit.Count) of $($sn.Count) logged"
$r | Select-String "COMMIT mgr\+0xf968" | Select-Object -First 4 | ForEach-Object Line
