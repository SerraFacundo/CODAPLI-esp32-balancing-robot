# Cómo medir los parámetros físicos del robot

Los valores de `ParametrosFisicos` (en `gemelo/modelo.py`) son provisorios. Con una balanza
de cocina, una regla, un cronómetro (el del celular alcanza) y una varilla se miden en
unos 20 minutos. Conviene medir todo **con la batería puesta**, como anda el robot.

| Parámetro | Campo en `ParametrosFisicos` | Cómo |
|---|---|---|
| Masa de cada rueda | `masa_rueda_kg` | Balanza (1) |
| Masa del cuerpo | `masa_cuerpo_kg` | Balanza (1) |
| Radio de rueda | `radio_rueda_m` | Regla o rodando (2) |
| Altura del centro de masa | `altura_cm_m` | Equilibrio sobre un borde (3) |
| Inercia del cuerpo | `inercia_cuerpo_kg_m2` | Péndulo físico (4) |

## 1. Masas

1. Pesá el robot completo: `m_total`.
2. Sacá una rueda y pesala: `m_rueda`. Si no se puede sacar, usá el dato del fabricante o
   estimala por el volumen de plástico impreso.
3. `masa_cuerpo_kg = m_total − 2·m_rueda`. Incluye motores, placa y batería, o sea todo lo
   que se inclina.

## 2. Radio de la rueda

Medí el diámetro con calibre o regla y dividilo por 2. Es más preciso marcar el piso,
hacer rodar el robot 5 vueltas completas y medir la distancia `D`:
`r = D / (2π·5)`. Incluye el aplastamiento de la goma.

## 3. Altura del centro de masa (`altura_cm_m`)

Es la distancia desde el **eje de las ruedas** hasta el centro de masa del cuerpo.

1. Sacá las ruedas o dejalas puestas. Su centro de masa está sobre el eje, así que no
   corren el punto de equilibrio hacia arriba ni hacia abajo, pero sí cambian la masa total.
   Lo más limpio es sacarlas.
2. Acostá el robot y apoyalo sobre un borde fino perpendicular a su altura (el canto de una
   regla, un lápiz o un cuchillo sin filo).
3. Movelo hasta que quede en equilibrio y marcá esa línea con cinta.
4. Medí la distancia desde el centro del eje de los motores hasta la marca: esa es
   `altura_cm_m` (`d`).
5. Repetí 3 veces y promediá. Si las medidas difieren en más de 3 mm, el borde es demasiado
   grueso.

> Si las ruedas quedaron puestas, el punto de equilibrio corresponde a cuerpo + ruedas.
> El centro de masa del cuerpo solo está en `d·m_total/m_cuerpo`.

## 4. Inercia del cuerpo: péndulo físico + Steiner

Colgado de un eje horizontal que **no** pase por el centro de masa, el cuerpo oscila como
un péndulo físico con período

    T = 2π·sqrt(I_pivote / (m·g·d))   ⇒   I_pivote = m·g·d·T² / (4π²)

y, por el teorema de Steiner, la inercia respecto del centro de masa es

    I_c = I_pivote − m·d²

Procedimiento, **sin ruedas**:

1. Dalo vuelta y apoyá los dos ejes de los motores sobre dos bordes lisos y horizontales
   (dos reglas sobre libros, por ejemplo), para que el cuerpo cuelgue hacia abajo. El
   pivote queda sobre el eje de las ruedas, así que `d = altura_cm_m`, que ya mediste.
   Alternativa: un clavo o varilla que pase por un agujero del chasis. En ese caso, `d`
   es la distancia de ese agujero al centro de masa.
2. Apartalo **menos de 10°** y soltalo. Con ángulos grandes la fórmula deja de valer.
3. Cronometrá **20 oscilaciones completas** (ida y vuelta) y dividí: `T = tiempo / 20`.
   Repetí 3 veces. Contar muchas oscilaciones reduce el error de reacción del cronómetro.
4. Calculá:

```bash
python -m gemelo.medicion --masa-total 0.70 --masa-rueda 0.05 --d 0.08 --tiempo 13.4 --oscilaciones 20
```

```
masa_cuerpo_kg       = 0.6000
período              = 0.6700 s
I_pivote             = 0.005354 kg·m²
inercia_cuerpo_kg_m2 = 0.001514 kg·m²  (Steiner: I_pivote − m·d²)
```

Si `I_c` da negativa o casi cero, `d` está mal medida (o se usó la masa con ruedas).
`I_c` es una resta de dos números parecidos, así que un error chico en `d` pesa mucho.

## 5. Cargar los valores y validar

Editá los valores por defecto de `ParametrosFisicos` en `gemelo/modelo.py` (dejando un
comentario con la fecha y cómo se midió) y corré la validación contra la telemetría real
(ver el README). Conviene grabar un tramo en el que se suelta el robot a mano desde unos
5–10° y se lo deja quieto. Si el transitorio inicial y la frecuencia de oscilación del
gemelo se parecen a los reales, el modelo está listo para probar ganancias.
