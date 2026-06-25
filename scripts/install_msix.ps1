# Instala el MSIX generado (requiere build_msix.ps1 previo).
# Uso: powershell -ExecutionPolicy Bypass -File scripts\install_msix.ps1

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$msixPath = Join-Path $projectRoot "dist\VidaUtilBateria.msix"
$certPath = Join-Path $projectRoot "packaging\VidaUtilBateria.cer"

if (-not (Test-Path $msixPath)) {
    Write-Host "No se encontro el MSIX. Genera primero:" -ForegroundColor Red
    Write-Host "  powershell -ExecutionPolicy Bypass -File scripts\build_msix.ps1"
    exit 1
}

if (Test-Path $certPath) {
    Import-Certificate -FilePath $certPath -CertStoreLocation Cert:\CurrentUser\TrustedPeople | Out-Null
    Write-Host "Certificado de desarrollo importado." -ForegroundColor Green
}

Add-AppxPackage -Path $msixPath -ForceApplicationShutdown
Write-Host ""
Write-Host "VidaUtil de la Bateria instalado desde MSIX." -ForegroundColor Green
Write-Host "Busca el icono en la bandeja del sistema."
Write-Host "Para inicio automatico: Configuracion de la app > Iniciar con Windows."
