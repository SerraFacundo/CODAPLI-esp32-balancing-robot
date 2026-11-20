# Robot Balanceador IoT — Industria 4.0

Extensión del robot balanceador CODAPLI (firmware en [`esp32_balancing_robot/`](../../esp32_balancing_robot)) para la materia
Industria 4.0 (UTN FRLP, 2026). Pilares trabajados: **simulación** (gemelo digital),
**IIoT** (plataforma local de telemetría) y **ciberseguridad** (autenticación del WebSocket).

## Estructura del repositorio

```
esp32_balancing_robot/ Firmware ESP32 (Arduino) — base del proyecto anterior
gemelo-digital/        A. Gemelo digital: modelo del péndulo invertido + sincronización con telemetría
iiot/                  B. Plataforma IIoT local: Node-RED + InfluxDB + Grafana (docker compose)
docs/ind40/            Documentación de este trabajo (protocolo, arquitectura, decisiones)
docs/, figs/           Documentación y figuras del proyecto original
```

## Hoja de ruta

| # | Objetivo específico | Dónde |
|---|---|---|
| 1 | Modelo dinámico del robot y gemelo digital — modelo de Lagrange + réplica del control del firmware a 100 Hz, con validación contra InfluxDB. Falta medir los parámetros físicos ([guía](../../gemelo-digital/MEDICION.md)) | `gemelo-digital/gemelo/modelo.py`, `firmware.py`, `simulador.py`, `validacion.py` |
| 2 | Sincronizar el gemelo con la telemetría real — visor 3D en vivo (`/gemelo` en Node-RED). Falta confirmar el signo del ángulo | `gemelo-digital/gemelo/telemetria.py`, `gemelo-digital/visor/` |
| 3 | Banco de pruebas de ganancias PID en el gemelo — listo: escenarios de empujón, joystick y empuje, métricas, ranking y comando `P:` (`python -m gemelo.banco`) | `gemelo-digital/gemelo/banco.py` |
| 4 | Recolección e histórico de telemetría — funcionando con el robot real (12/11) | `iiot/node-red/flows.json` |
| 5 | Dashboard de KPIs (uptime, caídas, MTBF, batería) — listo. Falta la batería: requiere divisor resistivo a GPIO 34 | `iiot/grafana/` |
| 6 | Análisis de datos históricos | `gemelo-digital/` o notebooks (a definir) |
| 7 | Token en el WebSocket + nueva contraseña del AP — probado en el robot (27/10). Falta probar modo AP+STA | `esp32_balancing_robot/` + `secrets.h` — ver [protocolo](protocolo-websocket.md) |

## Decisiones

- Backend IIoT **local** (laptop en la misma red que el robot; ESP32 en modo AP+STA).
- Seguridad: **token compartido** en el WebSocket y contraseña del AP fuera del código
  (`esp32_balancing_robot/secrets.h`, ignorado por git). Sin TLS para no comprometer el loop de control de 100 Hz.

Ver también: [protocolo WebSocket](protocolo-websocket.md).
