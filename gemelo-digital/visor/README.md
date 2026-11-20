# Visor 3D del gemelo digital

Página web con el robot dibujado en 3D (isométrico). El cuerpo se inclina sobre el eje de las
ruedas con el ángulo real del robot y las ruedas giran con la velocidad de los motores. Detrás hay
un cuadrante con una aguja que marca el mismo ángulo. Las rayas de ±56° indican el paso a las
ganancias de levantado y las de ±74°, el corte de motores.

## Verla

Con la plataforma IIoT levantada (`iiot/`, `docker compose up -d`): **http://localhost:1880/gemelo**.
También hay un enlace **Gemelo 3D** arriba a la derecha del dashboard de Grafana.

- Si no llegan datos durante 1,5 s, la página pasa sola a modo **demo**: un balanceo con una caída
  cada 24 s. Pasa lo mismo si se abre el archivo directo, sin Node-RED.
- Las flechas ← → inclinan el robot a mano durante 4 s.
- `?invertir=1` invierte el signo del ángulo, por si el robot real se inclina al revés que el dibujo.
- `?ws=ws://192.168.4.1/ws&token=…` hace que la página se conecte directo al robot, sin Node-RED.
  En ese caso el token queda en la URL y en el historial del navegador: usalo solo para pruebas.

## De dónde salen los datos

Node-RED ya recibe la telemetría del robot. El nodo **Solo A: para el gemelo** reenvía los
mensajes `A:<angulo>:<velocidad>:<motores>` (10 Hz) a `ws://<laptop>:1880/ws/gemelo`, sin token y
en un solo sentido: lo que manden los navegadores no llega al robot. La página interpola entre
muestras para dibujar a 60 fps.

| Dato | En el dibujo |
|---|---|
| `angulo` | Inclinación del cuerpo y de la aguja (se limita a ±95°, el ángulo en que el cuerpo toca el banco) |
| `velocidad` | Giro de las ruedas: 5,625 °/s por unidad (`Motors.cpp`: pasos/s = velocidad · 25; 1600 pasos por vuelta a 1/8 de micropaso) |
| `motores` | Lectura `motores ON/OFF` |

## Regenerar la página

```bash
cd gemelo-digital/visor
node build.mjs            # escribe iiot/node-red/gemelo.html (tema claro)
node build.mjs --dark     # escribe iiot/node-red/gemelo-oscuro.html
```

Requiere Node 20 o más nuevo, sin dependencias. La página generada es un solo HTML autónomo y
queda versionada, para que Node-RED la sirva sin compilar nada.

- `build.mjs`: ensamble del robot y página.
- `piezas/`: las piezas impresas salen de los STL de `docs/hardware/3d_models/files`. Son placas
  extruidas, así que `piezas/stl.mjs` corta cada STL a media altura y obtiene su perfil exacto
  (contorno y agujeros). `piezas/convexas.mjs` parte cada perfil en polígonos convexos, porque el kit
  solo dibuja piezas convexas, y las uniones internas se marcan como cortes para que no se vean.
  `piezas/robot.mjs` dice qué corte se toma de cada STL; `node piezas/preview.mjs > perfiles.svg`
  muestra los perfiles, y `node piezas/convexas.mjs` corre la autoprueba de la partición.

  | Pieza | STL | Cómo se arma |
  |---|---|---|
  | Paneles | `Side_Panel_[V11.stl` | perfil exacto, 6 agujeros calados, 14 de tornillo grabados |
  | Ruedas | `Wheel_[V10.stl` | aro, 3 rayos y centro en "Y", calados |
  | Buje | `Hub_[V10_ENLARGED.stl` | cuello, brida en "Y" alineada con los rayos y escalón |
  | Estante de electrónica | `Electronics_Shelf_[V10.stl` | placa con las dos ventanas sobre los motores + soportes |
  | Estante superior | `Top_Shelf_[V10.stl` | perfil en reloj de arena con las ranuras en T |
  | Bandeja de motores | `Motor_Shelf_[V10.stl` | piso con dos ventanas + soportes de pared |

  Los paneles, los estantes y la bandeja vienen ensamblados en el mismo sistema de los STL (el eje de
  los motores está en z = 21,15). Los paneles se acercaron 3,17 mm hacia adentro para que los
  estantes apoyen en su cara interior. Los motores (NEMA17 de 40 mm), la batería y las placas no
  están en los STL y van modelados aparte.
- `mock-relay.mjs`: servidor local con telemetría simulada (`node mock-relay.mjs`, después
  http://localhost:18800/gemelo).
- `live.js`: conexión WebSocket, modo demo, interpolación y teclado.
- `kit/`: kit de dibujo isométrico [Anatomy](kit/LICENSE) (MIT). El cuerpo es una bisagra sobre
  el eje y las ruedas, una bisagra hija. Tiene un cambio propio en `kit/turn.mjs` (marcado
  `gemelo-digital:`): las caras laterales que caen sobre un corte no se dibujan, porque son
  uniones internas entre trozos de una misma placa.
- `node build.mjs --audit` prueba la geometría: holguras, apoyos y orden de dibujo. Tarda varios
  minutos; `--quick` da una pasada rápida.

## Límites

- Las piezas impresas tienen el perfil de los STL simplificado a 0,5 mm (ruedas y paneles) o 0,7 mm
  (el resto), y se toma una sola altura por capa: los chaflanes y los detalles de menos de 2 mm no
  están. No están el paragolpes (una tira flexible que se curva), los cables ni los agregados.
- Son unas 410 piezas: redibujar cuesta unos 27 ms por cuadro en una notebook lenta (el visor
  dibuja un cuadro sí y otro no cuando pasa de 14 ms).
- Con el robot inclinado entre −55° y −51° hacia atrás, el kit tiene que forzar el orden de una
  pieza, y un detalle de unos 2 × 5 px puede parpadear. En el resto del recorrido la auditoría de
  orden da 0 ciclos y 0 errores de profundidad.
- Solo muestra la inclinación y el giro de las ruedas. No muestra el avance ni el giro sobre sí
  mismo, porque la telemetría no trae posición ni las dos velocidades por separado.
- El signo del ángulo y de la velocidad todavía no se comprobó contra el robot real.
