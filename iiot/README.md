# Plataforma IIoT local

Node-RED recibe la telemetría del robot por WebSocket y la guarda en InfluxDB; Grafana muestra
los KPIs (uptime, caídas, MTBF, batería). Todo corre en la laptop, sin depender de Internet.

## Requisitos

- Docker Desktop (Windows) o Docker Engine + compose.

## Levantar

```bash
cd iiot
cp .env.example .env      # en Windows: copy .env.example .env  — y editar las claves
docker compose up -d
```

| Servicio | URL |
|---|---|
| Node-RED | http://localhost:1880 |
| InfluxDB | http://localhost:8086 |
| Grafana  | http://localhost:3000 |

La laptop tiene que estar en la misma red que el robot (conectada al AP `BalanceBot`, o el
robot en modo AP+STA conectado a la red de la laptop).

## Dashboard de Grafana

Se carga solo al arrancar Grafana: **Dashboards → Robot Balanceador → Robot Balanceador — Monitoreo**
(`grafana/dashboards/robot.json`). Muestra tiempo encendido, caídas, tiempo de pie, MTBF,
batería, señal WiFi, ángulo, velocidad, caídas por minuto, últimas caídas y memoria libre.

- Las consultas usan el bucket `robot`; si cambiás `INFLUX_BUCKET` en `.env`, hay que cambiarlo
  también en el JSON.
- Al ser un dashboard provisionado, los cambios hechos en la interfaz no se pueden guardar encima:
  usá *Save as* o editá el JSON.
