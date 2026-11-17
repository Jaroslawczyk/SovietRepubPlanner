# Click at game-window pixel (x,y) (1920x1080 window at screen offset 320,180).
# Windows display scale is 125%: the process MUST be DPI-aware, otherwise SetCursorPos lands 1.25x off.
# Clicks only if the game window is in the foreground. -NoClick only moves.
param([int]$X, [int]$Y, [switch]$NoClick)
Add-Type @"
using System; using System.Runtime.InteropServices; using System.Text;
public class W {
 [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
 [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
 [DllImport("user32.dll")] public static extern void mouse_event(int f, int x, int y, int d, int e);
}
"@
[W]::SetProcessDPIAware() | Out-Null
$sb = New-Object System.Text.StringBuilder 256
[W]::GetWindowText([W]::GetForegroundWindow(), $sb, 256) | Out-Null
if (-not $sb.ToString().StartsWith("Workers")) { "game not in foreground: '$sb'"; exit 1 }
[W]::SetCursorPos($X + 320, $Y + 180) | Out-Null
Start-Sleep -Milliseconds 100
[W]::mouse_event(1,1,0,0,0); Start-Sleep -Milliseconds 50; [W]::mouse_event(1,-1,0,0,0)
Start-Sleep -Milliseconds 200
if (-not $NoClick) { [W]::mouse_event(2,0,0,0,0); Start-Sleep -Milliseconds 80; [W]::mouse_event(4,0,0,0,0); "clicked $X,$Y" } else { "moved $X,$Y" }
