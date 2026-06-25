# VidaUtil de la Bateria 1.0

**By Segi Serrano** — (c) 2026 -WeBlackBug-

Aplicacion residente para Windows escrita en **C++ (Win32)** que monitoriza la carga de la bateria, avisa con un MP3 configurable cuando se supera un umbral, y ofrece acciones segun la marca del portatil.

## Caracteristicas

- Icono en la bandeja del sistema (junto al WiFi, volumen, etc.)
- Aviso sonoro con archivo `.mp3` (loop opcional)
- Umbral de aviso configurable (1–100 %)
- Inicio automatico con Windows
- Selector de marca de portatil con texto de compatibilidad
- **Acer:** integracion WMI con clase `BatteryControl` (`SetBatteryHealthControl`)
- Boton **Probar WMI / modo salud Acer** en configuracion
- Opcion **Aplicar limite de carga al alcanzar el umbral**
- **Detener aviso sonoro** desde el menu de la bandeja
- Configuracion en el primer arranque
- Accion **Parar carga / Modo salud** segun soporte del fabricante
- Ventana **Acerca de**

## Diagnostico Acer (tu portatil)

En tu Acer se detecto la clase WMI `BatteryControl` en `ROOT\WMI` con los metodos:

- `GetBatteryHealthControlStatus`
- `SetBatteryHealthControl`

Eso permite activar el **modo salud (~80 %)** desde la app sin abrir Acer Care Center (si el firmware lo permite).

Script de diagnostico:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\probe_acer_wmi.ps1
```

## Compatibilidad de parada automatica de carga

| Marca | Nivel | Notas |
|-------|-------|-------|
| **Acer** | Parcial | Modo salud ~80 % via WMI/firmware o Acer Care Center. No permite % libre como un movil. |
| Lenovo | Alta | Lenovo Vantage / `batteryChargeThreshold.exe` |
| Dell | Alta | Dell Power Manager |
| HP / ASUS / MSI / Samsung | Parcial | Limite fijo ~80 % en app del fabricante |
| Framework | Alta | `framework_tool --charge-limit N` |
| Generico | Solo aviso | Desenchufar manualmente |

En tu **Acer**, la app intenta activar el **modo salud** por la interfaz WMI del firmware (`79772EC5-04B1-4bfd-843C-61E7F77B6CC9`). Si no esta disponible, abre **Acer Care Center / AcerSense** para activar *Battery Charge Limit*.

## Requisitos

- Windows 10/11 (64 bits recomendado)
- Visual Studio 2022 con workload **Desktop development with C++**, o Build Tools equivalentes
- CMake 3.20+

## Compilacion

```powershell
cd C:\Users\WeBlackBug\CodeProjects\Bateria
cmake -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
```

El ejecutable queda en:

`build\Release\VidaUtilBateria.exe`

## Uso

1. Ejecuta `VidaUtilBateria.exe`.
2. Clic derecho en el icono de la bandeja → **Configuracion**.
3. Elige **Acer** como marca, el umbral (ej. 90 %), tu MP3 y si quieres loop.
4. Marca **Iniciar con Windows** si lo deseas.
5. Al superar el umbral con el cargador conectado, sonara el aviso.
6. Usa **Parar carga / Modo salud** para intentar limitar la carga (en Acer, ~80 % fijo).

Doble clic en el icono de la bandeja abre la configuracion.

## Configuracion guardada

`%APPDATA%\VidaUtilBateria\config.ini`

## Limitaciones

- Windows no ofrece una API universal para detener la carga al % que elijas; depende del hardware y del fabricante.
- El aviso al 90 % y el limite de carga del fabricante (ej. 80 % en Acer) son independientes: la app avisa en tu umbral; la parada automatica usa el limite que permita tu portatil.
