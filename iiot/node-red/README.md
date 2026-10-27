# Flujos de Node-RED

Exportar acá los flujos (`flows.json`) para versionarlos. Dentro del contenedor esta carpeta
se ve como `/data/proyecto`.

Flujo previsto:

1. **websocket in** conectado a `$ROBOT_WS_URL` (primero envía `T:<token>` cuando el firmware lo pida).
2. **function**: parsea `A:<angulo>:<velocidad>:<motores>` y detecta caídas (|ángulo| alto o motores que pasan a 0).
3. **influxdb out** (nodo `node-red-contrib-influxdb`): measurement `telemetria` y `eventos` en el bucket `$INFLUX_BUCKET`.
