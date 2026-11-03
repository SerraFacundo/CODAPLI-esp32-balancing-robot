# Flujo de Node-RED: robot → InfluxDB

`flows.json` es el flujo que carga Node-RED al arrancar (`FLOWS` en `docker-compose.yml`).
Lo que se cambie en el editor se guarda acá mismo, así que queda versionado en git.

## Qué hace

1. **Robot** (websocket in) se conecta a `ROBOT_WS_URL` y se reconecta solo si se corta.
2. **Enviar token al conectar**: apenas se abre la conexión manda `T:<ROBOT_WS_TOKEN>`
   (sin eso el robot no manda telemetría y cierra a los 5 s).
3. **Parsear → line protocol**: convierte `A:`, `E:` y `F:` en líneas de InfluxDB.
4. **Lote para InfluxDB**: junta las líneas y las escribe cada 1 s con un POST a
   `/api/v2/write` (no hace falta instalar nodos extra de InfluxDB).
5. **Resultado**: muestra en el estado del nodo cuántas líneas se escribieron, o el error.

Probado contra un robot y un InfluxDB simulados: autenticación, las tres mediciones, y
reconexión con nuevo envío del token cuando el robot se reinicia.

## Datos en InfluxDB (bucket `INFLUX_BUCKET`, tag `robot=balancebot`)

| Measurement | Campos | Origen |
|---|---|---|
| `telemetria` | `angulo` (float), `velocidad` (int), `motores` (0/1) | `A:` a 10 Hz |
| `estado` | `uptime_s`, `caidas`, `de_pie_s`, `bateria_mv`*, `rssi_dbm`*, `heap_libre`, `clientes` | `E:` a 1 Hz |
| `caida` | `numero`, `angulo` (float), `tramo_ms` | `F:` en cada caída |

\* Se omiten cuando el robot no los mide (batería sin divisor, STA desconectada).

La marca de tiempo es la de recepción en Node-RED (ms).

Variables (de `iiot/.env`): `ROBOT_WS_URL`, `ROBOT_WS_TOKEN`, `INFLUX_URL`, `INFLUX_ORG`,
`INFLUX_BUCKET`, `INFLUX_TOKEN`.
