# Gemelo digital

Modelo del robot (péndulo invertido de dos ruedas con motores paso a paso) que sirve para:

- **Simular** el robot con el mismo control discreto que corre en el ESP32.
- **Probar ganancias PID** en un banco de pruebas antes de cargarlas al robot.
- **Validar** el modelo contra la telemetría real guardada en InfluxDB.
- **Visualizar** el robot real en 3D con la telemetría en vivo ([visor/](visor/README.md)).

## Puesta en marcha

Requiere Python 3.10 o más nuevo. En Windows (PowerShell):

```powershell
cd gemelo-digital
py -m venv .venv            # o: python -m venv .venv
.venv\Scripts\Activate.ps1  # si PowerShell lo bloquea: Set-ExecutionPolicy -Scope Process Bypass
pip install -r requirements.txt
pytest
```

En Linux/macOS: `python3 -m venv .venv && source .venv/bin/activate`. Todos los comandos
de abajo se corren desde `gemelo-digital/`.

## Módulos

| Archivo | Qué hace |
|---|---|
| `gemelo/modelo.py` | Parámetros físicos, ganancias (`comando_ws()` → `P:…`) y ecuaciones de movimiento |
| `gemelo/firmware.py` | Réplica del lazo de control de `esp32_balancing_robot.ino`, `Control.cpp` y `Motors.cpp` |
| `gemelo/simulador.py` | Lazo cerrado: planta + sensor MPU6050 + firmware, y escenarios |
| `gemelo/banco.py` | Banco de pruebas de ganancias (métricas, ranking y comando `P:`) |
| `gemelo/validacion.py` | Lectura del CSV de InfluxDB y comparación real vs. gemelo |
| `gemelo/medicion.py` | Cálculo de la inercia por péndulo físico ([MEDICION.md](MEDICION.md)) |
| `gemelo/telemetria.py` | Parser de los mensajes `A:`, `E:` y `F:` del robot |

## Modelo

**Planta.** Por defecto se usa el modelo de Lagrange del robot con motores paso a paso. El
motor fija la velocidad de la rueda *relativa al cuerpo*, porque el estator va pegado al
chasis, y el modelo incluye la reacción del motor sobre el cuerpo y la masa e inercia de
las ruedas. La ecuación del enunciado, con la aceleración de la base como entrada
(`(I_c + m·l²)·θ̈ = m·g·l·sinθ − m·l·a·cosθ`), queda disponible como `modelo="base_impuesta"`
para comparar. Las ecuaciones están en el docstring de `gemelo/modelo.py`. Con los
parámetros provisorios, el polo inestable a lazo abierto es 6.5 1/s (Lagrange) contra
9.4 1/s (base impuesta).

**Control.** `ControlFirmware` replica, ciclo a ciclo y a 100 Hz:

1. el filtro complementario;
2. la estimación de velocidad (`−actual_robot_speed + Δθ·25`, filtrada);
3. el PI de velocidad con anti-windup;
4. el PD de estabilidad, cuya salida se acumula en `control_output` (±500);
5. la rampa `MAX_ACCEL` = 20 por ciclo;
6. el período entero del timer de pasos (1 unidad = 25 pasos/s = 5.625 °/s);
7. el corte de motores a ±74°;
8. las ganancias de levantado por encima de ±56°.

Se respetan los truncamientos a `int16_t`.

**Sensor.** DLPF de 10 Hz del MPU6050 y un ángulo del acelerómetro que incluye la
aceleración de la base (`θ − atan(a/g)`). El ruido es opcional (`--ruido`).

### Cosas a tener en cuenta

- **El PI de velocidad corre en todos los ciclos (100 Hz), no cada 4.** El comentario de
  `Control.cpp` dice ~25 Hz, pero `cascade_control_loop_counter` está declarado y no se usa
  en ningún lado. El gemelo copia el firmware real (`periodo_pi_ciclos=1`); con
  `--periodo-pi 4` se puede ver qué pasaría si se implementara la cascada.
- **Signo.** El firmware responde con motor negativo cuando el ángulo es positivo. Para que
  eso equilibre, `signo_motor = −1` (velocidad positiva de motor = hacia atrás). No está
  confirmado contra el robot real. Con `--signo-motor 1` el gemelo se cae, y para comparar
  con datos reales existe `--invertir-angulo` en la validación.
- **Con los parámetros provisorios, las ganancias de `defines.h` no tiran al robot pero lo
  dejan en un ciclo límite de ±11–13° a ~0.6 Hz.** Lo provoca el término integral del PI
  de velocidad (con `KI_THROTTLE` más bajo o `KD` más alto desaparece). Si el robot real no
  oscila así, los parámetros reales son distintos, probablemente un centro de masa más bajo
  o ruedas más grandes. Por eso, **antes de cargar ganancias elegidas en el banco hay que
  medir los parámetros** ([MEDICION.md](MEDICION.md)) y validar.
- Simplificaciones: sin giro (steering = 0), motores ideales (no pierden pasos), sin
  deslizamiento de las ruedas y float de 64 bits en lugar de 32.

## Simulación desde Python

```python
from gemelo.modelo import GananciasPID, ParametrosFisicos
from gemelo.simulador import ConfigSimulacion, empujon_inicial, simular

r = simular(GananciasPID(kp=0.45, kd=0.08, ki_throttle=0.05),
            empujon_inicial(10.0),
            ParametrosFisicos(altura_cm_m=0.07),
            ConfigSimulacion(ruido_gyro_dps=0.3, ruido_accel_deg=1.0, semilla=1))
print(r.cayo, r.theta_deg.max(), r.t[-1])   # arrays numpy por ciclo de 10 ms
for t, msg in r.mensajes_telemetria():       # "A:<ang>:<vel>:<mot>" a 10 Hz, como el robot
    ...
```

## Banco de pruebas de ganancias

```bash
python -m gemelo.banco                                     # candidatos de ejemplo
python -m gemelo.banco -g "mia=0.45,0.08,0.08,0.05" -g P:0.4:0.06:0.08:0.05
python -m gemelo.banco --empujon 5 --joystick 1 --empuje 3 --ruido --grafico banco.png
```

Cada candidato se simula en los mismos escenarios: **empujón inicial** (se suelta inclinado
X°), **escalón de joystick** (de 0 a un valor en t = 1 s) y **empuje** (fuerza horizontal en
el centro de masa durante 100 ms). Métricas por escenario:

| Columna | Significado |
|---|---|
| ¿cae? | Si el firmware cortó los motores (\|ángulo\| ≥ 74°) y cuándo |
| θmáx ° | Máximo ángulo de inclinación |
| t_est s | Tiempo de establecimiento: desde el evento hasta quedar en ±1° del valor final (— = no se establece) |
| osc | Oscilaciones completas después del evento |
| pp fin ° | Pico a pico del ángulo en el último segundo (≠ 0 = ciclo límite) |
| v fin m/s | Velocidad final (en el escalón de joystick, la velocidad alcanzada) |
| motor rms / máx | Esfuerzo de motores en unidades del firmware (1 = 5.625 °/s de rueda) |
| rampa % | Ciclos en que `MAX_ACCEL` recortó la aceleración pedida |

El ranking ordena por caídas, escenarios sin establecerse, suma de tiempos de
establecimiento y suma de ángulos máximos. Al final se muestra el comando listo para mandar
por WebSocket. Ejemplo (parámetros provisorios, con ruido):

```
▶ empuje 3 N × 100 ms  (5 s)
  candidato          ¿cae?   θmáx °  t_est s  osc  pp fin °  v fin m/s  motor rms   máx  rampa %
  Kp+Kd, Ki bajo        no     14.2     2.17    1       0.5      -0.01         52   181      0.0
  más Kd                no     16.9        —    2      11.6      -0.16         76   176      0.0
  Ki bajo               no     23.7        —    3      21.1      -0.14        104   215      0.0
  firmware              no     25.6        —    2      36.6      -0.35        122   225      0.0
  absurda         sí 1.31s     81.4        —    0         —          —          3    13      1.5

▶ Ranking (caídas, sin establecerse, Σ t_est, Σ θmáx) y comando para el robot
  1. Kp+Kd, Ki bajo  caídas=0 sin_est=0 Σt=5.70s Σθ=36°  → P:0.45:0.08:0.08:0.05
  2. más Kd          caídas=0 sin_est=3 Σt=16.00s Σθ=40°  → P:0.32:0.08:0.08:0.1
  3. Ki bajo         caídas=0 sin_est=3 Σt=16.00s Σθ=51°  → P:0.32:0.05:0.08:0.03
  4. firmware        caídas=0 sin_est=3 Σt=16.00s Σθ=60°  → P:0.32:0.05:0.08:0.1
  5. absurda         caídas=3 sin_est=0 Σt=16.00s Σθ=246°  → P:0.02:0:0.08:0.1

Mejor candidato: Kp+Kd, Ki bajo. Para cargarlo, mandá por WebSocket:
  P:0.45:0.08:0.08:0.05
```

Otras opciones: `--modelo base_impuesta`, `--signo-motor 1`, `--periodo-pi 4` y
`--sin-firmware`.

## Validación contra datos reales

**1. Exportar la telemetría de InfluxDB** (bucket `robot`, measurement `telemetria`,
tag `robot=balancebot`). La consulta está en
[`datos/exportar_telemetria.flux`](datos/exportar_telemetria.flux). Ajustá `range()` al
momento del ensayo.

- **Desde la interfaz web** (http://localhost:8086 → Data Explorer → *Script Editor*): pegá
  la consulta, *Submit* y después *CSV* para descargar.
- **Desde la consola** (PowerShell, en `iiot/`, con el token de `iiot/.env`):

  ```powershell
  docker compose cp ..\gemelo-digital\datos\exportar_telemetria.flux influxdb:/tmp/consulta.flux
  docker compose exec -T influxdb influx query --org utn-frlp --token <INFLUX_TOKEN> --raw --file /tmp/consulta.flux > ..\gemelo-digital\telemetria.csv
  ```

  El CSV que genera PowerShell 5 sale en UTF-16; el lector lo detecta solo.

**2. Comparar:**

```bash
python -m gemelo.validacion telemetria.csv --grafico validacion.png
python -m gemelo.validacion telemetria.csv --tramo 2 -g 0.32,0.05,0.08,0.1   # otro tramo / ganancias cargadas
python -m gemelo.validacion telemetria.csv --desde 12.5 --hasta 30 --invertir-angulo
```

El script busca los tramos en que el robot estuvo de pie (motores = 1, al menos 3 s), simula
el gemelo desde el mismo ángulo inicial y superpone ángulo y velocidad. Informa el RMSE
(total y de los primeros 2 s), el RMS y la frecuencia dominante de la oscilación. El robot
real recibe empujones que el gemelo no conoce, así que importan sobre todo el transitorio
inicial y la frecuencia y amplitud de la oscilación.

**Sin datos del robot** se puede probar con un CSV de ejemplo, que tiene el mismo formato que
exporta InfluxDB. Lo genera el simulador con un "robot" un poco distinto del gemelo, ruido y
algunos empujones:

```bash
python -m gemelo.validacion --generar-ejemplo datos/ejemplo_telemetria.csv
python -m gemelo.validacion datos/ejemplo_telemetria.csv --grafico validacion.png
```

## Medición de parámetros

Ver [MEDICION.md](MEDICION.md): masas con balanza, radio de rueda, altura del centro de masa
por equilibrio sobre un borde e inercia por el período de oscilación como péndulo físico
(`python -m gemelo.medicion …`).

## Próximos pasos

1. Medir los parámetros físicos y validar con un ensayo grabado en InfluxDB.
2. Confirmar el signo del ángulo respecto del sentido de avance (`signo_motor`).
3. Cliente WebSocket que alimente el gemelo con la telemetría en vivo y lo compare con la
   simulación.
