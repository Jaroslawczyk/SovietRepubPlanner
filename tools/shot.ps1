param([string]$Out = "$env:TEMP\wrsr_shot.png")
Add-Type -AssemblyName System.Drawing
$b = New-Object System.Drawing.Bitmap 1920,1080
$g = [System.Drawing.Graphics]::FromImage($b)
$g.CopyFromScreen(320,180,0,0,$b.Size)
$b.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $b.Dispose()
Write-Output $Out
