# Protocolo WebSocket del robot

Estado **actual** del firmware (`src/esp32_balancing_robot.ino`, `processWebSocketMessage` y
`sendTelemetry`). Endpoint: `ws://192.168.4.1/ws` (AP `BalanceBot`).

## Cliente → robot (comandos)

| Mensaje | Significado |
|---|---|
| `J:<throttle>:<steering>` | Joystick, valores en [-1.0, 1.0] |
| `M:<0\|1>` | Modo normal / pro |
| `P:<kp>:<kd>:<kpt>:<kit>` | Ganancias PID (estabilidad PD + velocidad PI) |
| `C` | Calibrar |
| `S:<0\|1>` | Servo |
| `L:<0\|1>` | LED |
| `B` | Buzzer (doble beep) |
| `W` | Morse "UTN" por buzzer |

**Problema de seguridad:** ninguno de estos comandos requiere autenticación; cualquier
cliente conectado al AP puede mover el robot o cambiarle las ganancias.

## Robot → clientes (telemetría, 10 Hz)

```
A:<angulo>:<velocidad>:<motores_activos>
```

- `angulo`: ángulo de inclinación ajustado, en grados, 1 decimal.
- `velocidad`: `actual_robot_speed` (entero, unidades internas del firmware).
- `motores_activos`: `1` si los motores están habilitados, `0` si no.

## Cambios previstos

- **Autenticación:** el cliente envía `T:<token>` al conectarse; el robot ignora cualquier
  otro comando de ese cliente hasta validar el token (y lo desconecta si es inválido).
- **Telemetría extendida para IIoT y gemelo digital** (a definir): batería, uptime,
  contador de caídas, setpoint de ángulo, comando de motores. Mantener el prefijo `A:`
  compatible con la interfaz web actual y agregar un mensaje nuevo para los campos extra.
