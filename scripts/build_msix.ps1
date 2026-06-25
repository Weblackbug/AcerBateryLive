# Genera un paquete MSIX firmado para instalacion local (sideload).
# Uso: powershell -ExecutionPolicy Bypass -File scripts\build_msix.ps1

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $projectRoot "build"
$releaseExe = Join-Path $buildDir "Release\VidaUtilBateria.exe"
$packagingDir = Join-Path $projectRoot "packaging"
$stagingDir = Join-Path $projectRoot "dist\msix-staging"
$outputDir = Join-Path $projectRoot "dist"
$msixPath = Join-Path $outputDir "VidaUtilBateria.msix"
$certPath = Join-Path $packagingDir "VidaUtilBateria.cer"
$pfxPath = Join-Path $packagingDir "VidaUtilBateria.pfx"
$pfxPassword = "VidaUtilBateria2026"
$manifestPath = Join-Path $packagingDir "AppxManifest.xml"
$iconPath = Join-Path $projectRoot "resources\app.ico"

function Find-SdkTool {
    param([string]$ToolName)
    $sdkRoot = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\bin"
    if (-not (Test-Path $sdkRoot)) {
        throw "No se encontro Windows SDK. Instala 'Windows SDK' desde Visual Studio Installer."
    }
    $versionDir = Get-ChildItem $sdkRoot -Directory |
        Where-Object { $_.Name -match '^\d+\.\d+' } |
        Sort-Object Name -Descending |
        Select-Object -First 1
    if (-not $versionDir) {
        throw "No se encontro una version del Windows SDK en $sdkRoot"
    }
    $toolPath = Join-Path $versionDir.FullName "x64\$ToolName"
    if (-not (Test-Path $toolPath)) {
        throw "No se encontro $ToolName en $($versionDir.FullName)"
    }
    return $toolPath
}

function New-AssetsFromIcon {
    param([string]$SourceIcon, [string]$AssetsDir)
    Add-Type -AssemblyName System.Drawing
    New-Item -ItemType Directory -Force -Path $AssetsDir | Out-Null

    $icon = New-Object System.Drawing.Icon $SourceIcon
    $sizes = @{
        "StoreLogo.png"           = 50
        "Square44x44Logo.png"     = 44
        "Square150x150Logo.png"   = 150
        "Wide310x150Logo.png"     = 310
    }

    foreach ($entry in $sizes.GetEnumerator()) {
        $size = [int]$entry.Value
        $bitmap = New-Object System.Drawing.Bitmap $size, $size
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.Clear([System.Drawing.Color]::Transparent)
        $rect = New-Object System.Drawing.Rectangle 0, 0, $size, $size
        $graphics.DrawIcon($icon, $rect)
        $graphics.Dispose()
        $target = Join-Path $AssetsDir $entry.Key
        $bitmap.Save($target, [System.Drawing.Imaging.ImageFormat]::Png)
        $bitmap.Dispose()
    }

    $icon.Dispose()
}

function Ensure-SigningCertificate {
    param([string]$CerPath, [string]$PfxPath, [string]$Password)
    $existing = Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Subject -eq "CN=WeBlackBug" } | Select-Object -First 1
    if ($existing) {
        Export-Certificate -Cert $existing -FilePath $CerPath -Force | Out-Null
        if (-not (Test-Path $PfxPath)) {
            $secure = ConvertTo-SecureString -String $Password -Force -AsPlainText
            Export-PfxCertificate -Cert $existing -FilePath $PfxPath -Password $secure -Force | Out-Null
        }
        return $existing
    }

    $cert = New-SelfSignedCertificate `
        -Type Custom `
        -Subject "CN=WeBlackBug" `
        -KeyUsage DigitalSignature `
        -FriendlyName "VidaUtil Bateria MSIX" `
        -CertStoreLocation "Cert:\CurrentUser\My" `
        -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}")

    Export-Certificate -Cert $cert -FilePath $CerPath -Force | Out-Null
    $secure = ConvertTo-SecureString -String $Password -Force -AsPlainText
    Export-PfxCertificate -Cert $cert -FilePath $PfxPath -Password $secure -Force | Out-Null
    return $cert
}

Write-Host "Compilando Release..." -ForegroundColor Cyan
Push-Location $projectRoot
try { taskkill /IM VidaUtilBateria.exe /F 2>$null | Out-Null } catch {}
cmake --build build --config Release
Pop-Location

if (-not (Test-Path $releaseExe)) {
    throw "No se encontro $releaseExe"
}

Write-Host "Preparando staging MSIX..." -ForegroundColor Cyan
if (Test-Path $stagingDir) {
    Remove-Item $stagingDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $stagingDir | Out-Null
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

Copy-Item $releaseExe (Join-Path $stagingDir "VidaUtilBateria.exe") -Force
Copy-Item $manifestPath (Join-Path $stagingDir "AppxManifest.xml") -Force
New-AssetsFromIcon -SourceIcon $iconPath -AssetsDir (Join-Path $stagingDir "Assets")

$makeappx = Find-SdkTool "makeappx.exe"
$signtool = Find-SdkTool "signtool.exe"

if (Test-Path $msixPath) {
    Remove-Item $msixPath -Force
}

Write-Host "Empaquetando MSIX..." -ForegroundColor Cyan
& $makeappx pack /d $stagingDir /p $msixPath /o | Out-Host

Write-Host "Firmando MSIX..." -ForegroundColor Cyan
Ensure-SigningCertificate -CerPath $certPath -PfxPath $pfxPath -Password $pfxPassword | Out-Null
$securePassword = ConvertTo-SecureString -String $pfxPassword -Force -AsPlainText
& $signtool sign /fd SHA256 /f $pfxPath /p $pfxPassword /tr http://timestamp.digicert.com /td SHA256 $msixPath | Out-Host

Write-Host ""
Write-Host "MSIX generado:" -ForegroundColor Green
Write-Host "  $msixPath"
Write-Host ""
Write-Host "Instalacion (solo en este PC, certificado de desarrollo):" -ForegroundColor Yellow
Write-Host "  1. Confiar certificado:"
Write-Host "     Import-Certificate -FilePath `"$certPath`" -CertStoreLocation Cert:\CurrentUser\TrustedPeople"
Write-Host "  2. Instalar paquete:"
Write-Host "     Add-AppxPackage -Path `"$msixPath`""
Write-Host ""
Write-Host "Inicio con Windows:" -ForegroundColor Yellow
Write-Host "  - Marca 'Iniciar con Windows' en Configuracion de la app, o"
Write-Host "  - Configuracion de Windows > Aplicaciones > Inicio > VidaUtil de la Bateria"
