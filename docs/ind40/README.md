# Robot Balanceador IoT — Industria 4.0

Extensión del robot balanceador CODAPLI (firmware en [`src/`](../../src)) para la materia
Industria 4.0 (UTN FRLP, 2026). Pilares trabajados: **simulación** (gemelo digital),
**IIoT** (plataforma local de telemetría) y **ciberseguridad** (autenticación del WebSocket).

## Estructura del repositorio

```
src/                  Firmware ESP32 (Arduino) — base del proyecto anterior
gemelo-digital/       A. Gemelo digital: modelo del péndulo invertido + sincronización con telemetría
iiot/                 B. Plataforma IIoT local: Node-RED + InfluxDB + Grafana (docker compose)
docs/ind40/           Documentación de este trabajo (protocolo, arquitectura, decisiones)
docs/, figs/          Documentación y figuras del proyecto original
```

## Hoja de ruta

| # | Objetivo específico | Dónde |
|---|---|---|
| 1 | Modelo dinámico del robot y gemelo digital | `gemelo-digital/gemelo/modelo.py` |
| 2 | Sincronizar el gemelo con la telemetría real | `gemelo-digital/gemelo/telemetria.py` |
| 3 | Banco de pruebas de ganancias PID en el gemelo | `gemelo-digital/` |
| 4 | Recolección e histórico de telemetría | `iiot/` (Node-RED → InfluxDB) |
| 5 | Dashboard de KPIs (uptime, caídas, MTBF, batería) | `iiot/grafana/` |
| 6 | Análisis de datos históricos | `gemelo-digital/` o notebooks (a definir) |
| 7 | Token en el WebSocket + nueva contraseña del AP | `src/` + `src/secrets.example.h` |

## Decisiones

- Backend IIoT **local** (laptop en la misma red que el robot; ESP32 en modo AP+STA).
- Seguridad: **token compartido** en el WebSocket y contraseña del AP fuera del código
  (`src/secrets.h`, ignorado por git). Sin TLS para no comprometer el loop de control de 100 Hz.

Ver también: [protocolo WebSocket](protocolo-websocket.md).
