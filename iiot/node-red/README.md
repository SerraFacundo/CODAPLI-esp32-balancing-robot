# Flujos de Node-RED

Exportar acá los flujos (`flows.json`) para versionarlos. Dentro del contenedor esta carpeta
se ve como `/data/proyecto`.

Flujo previsto:

1. **websocket in / out** conectados a `$ROBOT_WS_URL`. Apenas se abre la conexión hay que
   enviar `T:<token>` (el mismo `SECRET_WS_TOKEN` del firmware) y esperar `T:OK`; sin eso el
   robot no manda telemetría y cierra la conexión a los 5 s. Ver
   [protocolo WebSocket](../../docs/ind40/protocolo-websocket.md).
2. **function**: parsea `A:<angulo>:<velocidad>:<motores>` y detecta caídas (|ángulo| alto o motores que pasan a 0).
3. **influxdb out** (nodo `node-red-contrib-influxdb`): measurement `telemetria` y `eventos` en el bucket `$INFLUX_BUCKET`.

El token va en `iiot/.env` (variable `ROBOT_WS_TOKEN`), no en el flujo exportado.
