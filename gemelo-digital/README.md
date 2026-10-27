# Gemelo digital

Modelo del robot (péndulo invertido de dos ruedas) que corre en paralelo al robot real,
alimentado con su telemetría, para comparar real vs. simulado y probar ganancias PID
antes de subirlas al hardware.

## Puesta en marcha

```bash
cd gemelo-digital
python -m venv .venv
# Windows:  .venv\Scripts\activate      Linux/macOS:  source .venv/bin/activate
pip install -r requirements.txt
pytest
```

## Módulos

- `gemelo/telemetria.py` — parseo de los mensajes del robot (`A:<ang>:<vel>:<mot>`).
- `gemelo/modelo.py` — parámetros físicos y dinámica del robot (en desarrollo).

## Próximos pasos

1. Medir/estimar parámetros físicos (masas, altura del centro de masa, radio de rueda).
2. Escribir las ecuaciones del péndulo invertido y el controlador en cascada igual al firmware.
3. Cliente WebSocket que reciba la telemetría y la compare con la simulación en vivo.
