# Protocolo WebSocket del robot

Firmware: `esp32_balancing_robot/esp32_balancing_robot.ino` (`onWebSocketEvent`, `handleAuthMessage`,
`processWebSocketMessage`, `sendTelemetry`).

Endpoints:

- Por el AP del robot (`BalanceBot`): `ws://192.168.4.1/ws`
- Por la red de la laptop (modo AP+STA, si `SECRET_STA_SSID` está configurado):
  `ws://<IP del robot en esa red>/ws` — la IP se imprime en el Monitor Serie al conectarse.

## Autenticación

Cada conexión arranca **sin autenticar**. Mientras no lo esté, el robot no le envía
telemetría e ignora sus comandos (responde `T:REQ`).

| Paso | Mensaje | Notas |
|---|---|---|
| Cliente → robot | `T:<token>` | `SECRET_WS_TOKEN` de `esp32_balancing_robot/secrets.h` |
| Robot → cliente | `T:OK` | Desde ahí se aceptan comandos y llega telemetría |
| Robot → cliente | `T:ERR` + cierre con código **4001** | Token inválido |
| Robot → cliente | cierre con código **4002** | No se autenticó en `WS_AUTH_TIMEOUT_MS` (5 s) |

Otras reglas (`esp32_balancing_robot/defines.h`):

- Hasta `WS_MAX_CLIENTS` (4) conexiones simultáneas; las que sobran se cierran al conectar.
- Después de un token inválido, durante `WS_AUTH_FAIL_COOLDOWN_MS` (2 s) se rechaza
  cualquier intento, para frenar la prueba de tokens por fuerza bruta.
- La comparación del token es en tiempo constante.
- Si se desconecta un cliente autenticado, el robot pone el joystick en cero (como antes).

La interfaz web pide el token antes de abrir la conexión y lo guarda en el navegador
(`localStorage`); si el robot lo rechaza, lo borra y lo vuelve a pedir.

**Límite conocido:** sin TLS el token viaja en texto plano dentro de la red WiFi. Lo protege
el cifrado WPA2 de la red (por eso la contraseña del AP también salió del código); alguien
que ya conoce la contraseña de la red podría capturarlo. Decisión documentada en el informe.

## Cliente → robot (comandos, requieren autenticación)

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

## Robot → clientes autenticados (telemetría, 10 Hz)

```
A:<angulo>:<velocidad>:<motores_activos>
```

- `angulo`: ángulo de inclinación ajustado, en grados, 1 decimal.
- `velocidad`: `actual_robot_speed` (entero, unidades internas del firmware).
- `motores_activos`: `1` si los motores están habilitados, `0` si no.

## Pendiente

- **Telemetría extendida para IIoT y gemelo digital**: batería, uptime, contador de caídas,
  setpoint de ángulo, comando de motores. Mantener `A:` como está (lo usa la interfaz web)
  y agregar un mensaje nuevo para los campos extra.
