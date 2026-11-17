# Footpath-to-road join experiment: restart the game into the save, set the selftest area, build the test road
# (DistrictPlanner.jointest1), wait -Wait seconds, build the test footpaths (jointest2), print the results.
param([int]$Wait = 20, [string]$RoadKind = '2', [switch]$RunGame, [switch]$StartPlan)
$g = 'F:\steam\steamapps\common\SovietRepublic'
$t = "$g\DistrictPlanner\tools"
Add-Type @"
using System; using System.Runtime.InteropServices; using System.Text;
public class FW2 { [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow(); [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n); }
"@
function Front { $sb = New-Object System.Text.StringBuilder 256; [FW2]::GetWindowText([FW2]::GetForegroundWindow(), $sb, 256) | Out-Null; $sb.ToString().StartsWith("Workers") }
Set-Content "$g\DistrictPlanner.roadkind" $RoadKind
try { Stop-Process -Name SOVIET64 -Force -ErrorAction Stop } catch {}
Start-Sleep 4
Copy-Item "$g\DistrictPlanner\build\dinput8.dll" "$g\dinput8.dll" -Force
powershell -NoProfile -File "$t\launch.ps1" -Restart | Out-Null
Start-Sleep 10
Set-Content "$g\DistrictPlanner.selftest" "4"
for ($i = 0; $i -lt 150; $i++) {
    Start-Sleep 2
    if (-not (Test-Path "$g\DistrictPlanner.selftest")) { break }
    if ($i % 12 -eq 6 -and (Front)) { & "$t\click.ps1" -X 510 -Y 505 | Out-Null }
}
if (Test-Path "$g\DistrictPlanner.selftest") { "save not loaded (game window was not in front)"; exit 1 }
Start-Sleep 4
New-Item "$g\DistrictPlanner.jointest1" -ItemType File -Force | Out-Null
if ($StartPlan) { Start-Sleep 8; if (Front) { & "$t\click.ps1" -X 1840 -Y 912 | Out-Null; "pressed: start construction of planned infrastructure" } else { "game not in front: plan button not pressed" } }
Start-Sleep $Wait
& "$t\shot.ps1" | Out-Null
$s0 = (Get-Content "$g\DistrictPlanner.log").Count
New-Item "$g\DistrictPlanner.jointest2" -ItemType File -Force | Out-Null
Start-Sleep 20
$c = Get-Content "$g\DistrictPlanner.log"
"== road kind $RoadKind, wait $Wait s"
$c | Select-String "summary" | ForEach-Object Line
$c[$s0..($c.Count - 1)] | Select-String "FOOTPATH JOINED|COMMIT mgr\+0xf968|footpath .*refused" | ForEach-Object Line
