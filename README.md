# ESP32 Robot Balanceador — CODAPLI

> Réplica del proyecto [ESP32 Balancing Robot Shield de PCBWay](https://www.pcbway.com/project/shareproject/ESP32_Balancing_Robot_Shield.html), adaptado y documentado para contexto académico en el marco del proyecto final para la materia de Tecnologias para la Automatización y de la mano con el laboratorio CODAPLI.

## Descripción

Robot balanceador de dos ruedas basado en ESP32. Utiliza un sensor IMU MPU-6050 para estimar el ángulo de inclinación y un sistema de control en cascada (PID) para mantener el equilibrio mediante dos motores paso a paso NEMA17.

El robot se controla en tiempo real a través de una interfaz web embebida, accesible por WiFi desde cualquier navegador, sin necesidad de instalar aplicaciones. Soporta control táctil (joystick virtual), control mediante gamepad USB y ajuste de parámetros PID en tiempo real.

## Hardware

### Lista de componentes (BOM)

| Componente           | Especificación                       | Cantidad |
| -------------------- | ------------------------------------ | -------- |
| Microcontrolador     | ESP32 (ESP-WROOM-32)                 | 1        |
| Motor paso a paso    | NEMA17                               | 2        |
| Driver de motor      | A4988                                | 2        |
| IMU                  | MPU-6050 (giroscopio + acelerómetro) | 1        |
| Servo                | MG-90S                               | 1        |
| Batería              | Li-ion 18650 3.7V                    | 2        |
| Buzzer               | Activo                               | 1        |
| LED                  | Alta luminosidad                     | 2        |
| Regulador de voltaje | LM7805                               | 2        |
| Capacitor            | 220µF / 16V                          | 2        |
| Capacitor            | 100µF / 16V                          | 1        |

### Pinout ESP32

| Pin ESP32 | Función                           |
| --------- | --------------------------------- |
| GPIO 12   | ENABLE motores (compartido A4988) |
| GPIO 14   | STEP motor 1                      |
| GPIO 27   | DIR motor 1                       |
| GPIO 26   | STEP motor 2                      |
| GPIO 25   | DIR motor 2                       |
| GPIO 13   | Señal servo                       |
| GPIO 21   | I2C SDA (MPU-6050)                |
| GPIO 22   | I2C SCL (MPU-6050)                |
| GPIO 32   | LED                               |
| GPIO 2    | LED WiFi (onboard)                |
| GPIO 33   | Buzzer                            |

![Schematic](docs/hardware/hardaware-diag.png)

## Arquitectura del Firmware

El firmware implementa un **sistema de control en cascada** corriendo a 100Hz:

<img src="docs/firmware/pid-diag.png" alt="Schematic" width="50%">

### Módulos principales

| Archivo                     | Descripción                                                       |
| --------------------------- | ----------------------------------------------------------------- |
| `esp32_balancing_robot.ino` | Setup, loop principal, WebSocket, WiFi AP                         |
| `Control.cpp / .h`          | Controladores PD de estabilidad, PI de velocidad y PD de posición |
| `MPU6050.cpp / .h`          | Driver IMU: lectura I2C, filtro complementario, calibración       |
| `Motors.cpp / .h`           | Control de velocidad de motores con limitación de aceleración     |
| `Timers.cpp / .h`           | ISRs de hardware para generación de pulsos de paso                |
| `WebInterface.h`            | Interfaz web embebida (HTML/CSS/JS en PROGMEM)                    |
| `defines.h`                 | Pines, constantes, parámetros PID, macros de compatibilidad       |
| `globals.h / .cpp`          | Variables globales compartidas entre módulos                      |

### Filtro complementario (IMU)

El MPU-6050 fusiona giroscopio y acelerómetro mediante un filtro complementario:

```cpp
angle = 0.99 * (angle + gyro_rate * dt) + 0.01 * accel_angle
```

Esto combina la precisión a corto plazo del giroscopio con la estabilidad a largo plazo del acelerómetro, con una constante de tiempo de ~1 segundo a 100Hz.

### Compatibilidad ESP32 Arduino Core

El código soporta automáticamente **Core 2.x y Core 3.x** mediante macros de compatibilidad definidas en `defines.h`. Las APIs de timers y LEDC cambiaron significativamente entre versiones.

## Dependencias

Instalar desde el **Gestor de Librerías** de Arduino IDE:

| Librería            | Versión recomendada | Uso                      |
| ------------------- | ------------------- | ------------------------ |
| `AsyncTCP`          | ≥ 1.1.1             | TCP asíncrono para ESP32 |
| `ESPAsyncWebServer` | ≥ 1.2.3             | Servidor web y WebSocket |

La librería `Wire` (I2C) es parte del core de Arduino y no requiere instalación separada.

En el Gestor de Placas de Arduino IDE, instalar: [ESP32 by Espressif Systems](https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json) (versión 2.x o 3.x, ambas soportadas)

## Instalación y Carga

1. Clonar el repositorio:

   ```bash
   git clone https://github.com/La-patrulla-AG/CODAPLI-esp32-balancing-robot.git
   ```

2. Abrir `firmware/src/esp32_balancing_robot.ino` en Arduino IDE.

3. Seleccionar la placa: **Tools → Board → ESP32 Dev Module**

4. Configurar:
   - Upload Speed: `115200`
   - CPU Frequency: `240MHz`
   - Partition Scheme: `Default`

5. Conectar el ESP32 por USB y cargar el firmware con el botón **Upload**.

6. Abrir el Monitor Serie a `115200 baud` para ver el proceso de calibración y la IP del AP.

## Uso

1. Encender el robot. El ESP32 creará una red WiFi:
   - **SSID:** `BalanceBot`
   - **Contraseña:** `12345678`

2. Conectarse a esa red desde el celular o PC.

3. Abrir el navegador en: **http://192.168.4.1**

4. Esperar que el robot complete la calibración recostado en el suelo (no moverlo durante ese proceso).

5. Controlar con el joystick táctil (móvil) o gamepad USB (escritorio).

## Configuración y Calibración

Los parámetros principales se configuran en `defines.h`:

```cpp
// Microstepping: debe coincidir con el hardware del A4988
#define MICROSTEPPING 8          // 1/8 paso (MS1=HIGH, MS2=HIGH, MS3=LOW)

// Ganancias PID por defecto
#define KP          0.32         // Proporcional estabilidad
#define KD          0.050        // Derivativo estabilidad
#define KP_THROTTLE 0.080        // Proporcional velocidad
#define KI_THROTTLE 0.1          // Integral velocidad

// Compensación de centro de gravedad
#define ANGLE_OFFSET 0.0         // Ajustar tras calibración
```

### Calibración en campo

Desde la interfaz web, el botón **CALIB** captura el ángulo actual como referencia de equilibrio. El robot debe estar vertical y quieto en ese momento.

Los parámetros PID también se pueden ajustar en tiempo real con los sliders de la sección **PID TUNING** de la interfaz web.

## Interfaz Web

La interfaz está completamente embebida en el firmware (no requiere servidor externo).

**Funcionalidades:**

- Joystick táctil para móvil
- Soporte de gamepad USB (escritorio)
- Telemetría en tiempo real: ángulo, velocidad, estado de motores
- Ajuste de ganancias PID con sliders
- Control de servo, LEDs y buzzer
- Modos Normal y Pro (diferentes límites de velocidad)
- Calibración remota

**Protocolo WebSocket:**

| Mensaje                   | Descripción                      |
| ------------------------- | -------------------------------- |
| `J:<throttle>:<steering>` | Control de joystick (-1.0 a 1.0) |
| `M:<0\|1>`                | Cambio de modo (Normal/Pro)      |
| `P:<kp>:<kd>:<kpt>:<kit>` | Actualización de PID             |
| `C`                       | Solicitud de calibración         |
| `S:<0\|1>`                | Control de servo                 |
| `L:<0\|1>`                | Control de LED                   |
| `B`                       | Buzzer                           |
| `W`                       | Código morse "UTN"               |

## Referencias

- [Proyecto original — PCBWay](https://www.pcbway.com/project/shareproject/ESP32_Balancing_Robot_Shield.html)
- [Video de ensamblaje](https://www.youtube.com/watch?v=tZynIj1StpM)
- [MPU-6000/6050 Register Map](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Register-Map1.pdf) _(referencia del driver IMU)_
- [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32)

## Licencia

Este proyecto es un trabajo derivado del proyecto original de PCBWay, basado a su vez en el B-Robot EVO2 de JJROBOTS.

Distribuido bajo los términos de la **GNU General Public License v2.0**.

Ver el archivo [LICENSE](LICENSE) para el texto completo.
