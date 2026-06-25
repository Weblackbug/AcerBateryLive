# Diagnostico WMI Acer - VidaUtil de la Bateria
# Ejecutar como administrador puede dar mas detalle, pero no siempre es necesario.

$guid = '79772EC5-04B1-4bfd-843C-61E7F77B6CC9'
Write-Host "=== VidaUtil - Probe Acer WMI ===" -ForegroundColor Cyan
Write-Host "GUID: $guid"
Write-Host ""

Write-Host "Clases en ROOT\WMI que contienen '79772' o 'Acer' o 'Battery':" -ForegroundColor Yellow
try {
    Get-CimClass -Namespace root/wmi -ErrorAction Stop |
        Where-Object { $_.CimClassName -match '79772|Acer|Battery' } |
        Select-Object CimClassName |
        Format-Table -AutoSize
} catch {
    Write-Host "Get-CimClass fallo: $_" -ForegroundColor Red
    Write-Host "Intentando Get-WmiObject..."
    Get-WmiObject -Namespace root/wmi -List |
        Where-Object { $_.Name -match '79772|Acer|Battery' } |
        Select-Object Name |
        Format-Table -AutoSize
}

Write-Host "Metodos de BatteryControl:" -ForegroundColor Yellow
try {
    (Get-CimClass -Namespace root/wmi -ClassName BatteryControl).CimClassMethods.Name
} catch {
    Write-Host "BatteryControl no disponible: $_"
}
Get-CimInstance Win32_Battery | Select-Object Name, EstimatedChargeRemaining, BatteryStatus, PowerOnline | Format-List

Write-Host ""
Write-Host "GetSystemPowerStatus equivalente:" -ForegroundColor Yellow
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Power {
    [StructLayout(LayoutKind.Sequential)]
    public struct SYSTEM_POWER_STATUS {
        public byte ACLineStatus;
        public byte BatteryFlag;
        public byte BatteryLifePercent;
        public byte SystemStatusFlag;
        public uint BatteryLifeTime;
        public uint BatteryFullLifeTime;
    }
    [DllImport("kernel32.dll")]
    public static extern bool GetSystemPowerStatus(out SYSTEM_POWER_STATUS s);
}
"@
$p = New-Object Power+SYSTEM_POWER_STATUS
[Power]::GetSystemPowerStatus([ref]$p) | Out-Null
Write-Host "Carga: $($p.BatteryLifePercent)%  AC: $($p.ACLineStatus)  Flag: $($p.BatteryFlag)"

Write-Host ""
Write-Host "Fin del diagnostico." -ForegroundColor Green
