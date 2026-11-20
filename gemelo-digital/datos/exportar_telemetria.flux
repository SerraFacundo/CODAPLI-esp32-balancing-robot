// Exporta la telemetría rápida (A:) que Node-RED guarda en InfluxDB, una fila por muestra.
// Ajustar range() al momento del ensayo, p. ej. range(start: 2026-11-12T18:00:00Z, stop: 2026-11-12T18:20:00Z)
from(bucket: "robot")
  |> range(start: -1h)
  |> filter(fn: (r) => r._measurement == "telemetria" and r.robot == "balancebot")
  |> filter(fn: (r) => r._field == "angulo" or r._field == "velocidad" or r._field == "motores")
  |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
  |> keep(columns: ["_time", "angulo", "velocidad", "motores"])
  |> sort(columns: ["_time"])
