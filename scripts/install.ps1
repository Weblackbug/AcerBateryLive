# Instala VidaUtil de la Bateria y lo registra al inicio de Windows.
# Ejecutar: powershell -ExecutionPolicy Bypass -File scripts\install.ps1

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not (Test-Path "$projectRoot\CMakeLists.txt")) {
    $projectRoot = Split-Path -Parent $PSScriptRoot
}

$sourceExe = Join-Path $projectRoot "build\Release\VidaUtilBateria.exe"
if (-not (Test-Path $sourceExe)) {
    Write-Host "No se encontro el ejecutable. Compila primero:" -ForegroundColor Red
    Write-Host "  cmake --build build --config Release"
    exit 1
}

$installDir = Join-Path $env:LOCALAPPDATA "Programs\VidaUtilBateria"
$targetExe = Join-Path $installDir "VidaUtilBateria.exe"

New-Item -ItemType Directory -Force -Path $installDir | Out-Null
Copy-Item -Path $sourceExe -Destination $targetExe -Force

$runKey = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run"
Set-ItemProperty -Path $runKey -Name "VidaUtilBateria" -Value "`"$targetExe`""

Write-Host ""
Write-Host "Instalacion completada." -ForegroundColor Green
Write-Host "  Carpeta: $installDir"
Write-Host "  Ejecutable: $targetExe"
Write-Host "  Inicio con Windows: ACTIVADO (clave Run)"
Write-Host ""
Write-Host "Iniciando la aplicacion..."
Start-Process -FilePath $targetExe

Write-Host ""
Write-Host "Busca el icono en la bandeja del sistema (junto al WiFi)."
Write-Host "Clic derecho -> Configuracion para elegir MP3, umbral y marca Acer."
