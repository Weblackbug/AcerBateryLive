Add-Type -AssemblyName System.Drawing
$bmp = New-Object System.Drawing.Bitmap 32, 32
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.Clear([System.Drawing.Color]::FromArgb(0, 120, 215))
$brush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::White)
$g.FillRectangle($brush, 8, 10, 6, 14)
$g.FillRectangle($brush, 18, 6, 6, 18)
$g.Dispose()
$icon = [System.Drawing.Icon]::FromHandle($bmp.GetHicon())
$outPath = Join-Path $PSScriptRoot '..\resources\app.ico'
$fs = [System.IO.File]::Create($outPath)
$icon.Save($fs)
$fs.Close()
$bmp.Dispose()
Write-Host "Created $outPath"
