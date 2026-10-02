# 🛠️ Parte mecánica — Rabosa-MVR

Aquí vive todo lo mecánico: los modelos descargados de GrabCAD para montar el escenario 3D y los
diseños propios hechos en FreeCAD.

```
mechanical/
├── freecad/   ← diseños propios (chasis, llantas, goma…)  ✅ en git
└── step/      ← modelos descargados de GrabCAD (ZIP con STEP dentro)
```

- Créditos y enlaces de los modelos descargados: [CREDITOS-GrabCAD.md](./CREDITOS-GrabCAD.md)
- Cotas y cinemática que salen de estos modelos: [Documentacion/cinemáticas.md](../Documentacion/cinemáticas.md)
  y [Documentacion/hardware.md](../Documentacion/hardware.md) §4.1

---

## 1. Qué hay ahora

| Carpeta | Contenido | Estado en git |
|:--------|:----------|:--------------|
| `freecad/` | `Rabosa.FCStd` (montaje completo), `Rabosa-logica.FCStd`, `llanta.FCStd`, `llanta-N20.FCStd`, `Goma.FCStd` + exportaciones `.step` de piezas concretas | ✅ **Versionado** (`mechanical/freecad/*.FCStd`) |
| `step/` | ~25 modelos descargados de GrabCAD (ZIP con STEP dentro) | ⚠️ **Ignorado** (784 MB) — se citan en [CREDITOS-GrabCAD.md](./CREDITOS-GrabCAD.md) |
| `freecad/export/` | Exportaciones `.step` propias (pendientes de crear) | ⚠️ **Ignorado** por ahora: los `.step` entran cuando estén los créditos |

> **Regla de `.gitignore` (2026-09-21)**: el repo funciona con lista blanca (`*` + excepciones `!`).
> De `mechanical/` solo entran los **`.md`** de la raíz y los **`freecad/*.FCStd`**; los
> `mechanical/**/*.zip` y `mechanical/**/*.step` quedan fuera, igual que `mechanical/step/`.
> Cuando quieras versionar tus propias exportaciones, basta añadir
> `!mechanical/freecad/export/*.step` al final de `.gitignore`.

---

## 2. Qué falta exportar (y para qué)

Para poder **documentar las cotas reales** del robot hacen falta las exportaciones a STEP:

| Exportar | Nombre propuesto | Para qué |
|:---------|:-----------------|:---------|
| Montaje completo del robot | `mechanical/freecad/export/Rabosa-completo.step` | Medir caja envolvente, trocha, distancia entre ejes, altura de sensores |
| Estructura/chasis que has diseñado | `…/Rabosa-chasis.step` | Verificar el diseño y documentar las piezas a imprimir |
| Las piezas propias sueltas (llantas, soportes) | `…/Rabosa-<pieza>.step` | Ficha de cada pieza: cotas y parámetros de impresión |

**Cómo exportar desde FreeCAD**: seleccionar el objeto o el ensamblaje → *Archivo → Exportar…* →
tipo **STEP with colors** (`.step`) → guardar en `mechanical/freecad/export/`.

> Con los `.step` puedo leer los puntos y darte las **cotas medidas del modelo** (no las
> "reportadas"). La **trocha efectiva** ya no hace falta deducirla del 3D: se **midió en el robot**
> el 2026-10-01 con la prueba 19 → **77,6 mm** (`ODOM_TROCHA_MM_DEF`). No coincide con ninguna de las
> tres cotas geométricas (82,8 / 83,6 / ≈74,8 mm) porque la efectiva incluye el deslizamiento de las
> ruedas al pivotar: **es el número que la hace cuadrar con el suelo**, no el que mide el calibre.

---

## 3. Ficha de fabricación (rellenar)

| Parámetro | Valor |
|:----------|:------|
| Impresora | |
| Material | |
| Diámetro de boquilla | |
| Altura de capa | |
| Relleno (infill) | |
| Soportes | |
| Temperaturas (boquilla / cama) | |
| Tiempo de impresión por pieza | |
| Post-proceso | |

---

## 3b. Tolerancias de montaje: la rueda en el eje del motor

Criterios para que la rueda entre **sin forzar** y quede **perpendicular** al eje. El eje del N20 es
**Ø3 mm × 10 mm con *D-cut***: el rebaje lo deja en ≈2,5 mm de espesor a lo largo de ≈9,5 mm
([hardware.md §4.1](../Documentacion/hardware.md#41-motores-transmisión-y-chasis)).

### El cubo mide 2 mm, y ahí está medio problema

Con solo 2 mm de agarre en un eje de 10 mm, la rueda tiene un brazo de palanca enorme y **bascule con
solo mirarla**. Alargar el cubo a **6–8 mm** fija la perpendicularidad mucho más que la tolerancia.

### Holguras para impresión FDM

Los agujeros impresos salen **entre 0,1 y 0,3 mm más pequeños** de lo dibujado (contracción del material
+ trayectoria de la boquilla). Por eso un agujero dibujado a **Ø3,0 mm no entra**: sale a ≈2,8 mm, y al
forzarlo **la rueda se tuerce**. Es exactamente lo que pasó con las ruedas actuales.

| Elemento | Medida del eje | Dibujar en el modelo | Holgura |
|:---------|:---------------|:---------------------|:--------|
| Ø del eje | 3,0 mm | **3,15 – 3,25 mm** | +0,15 / +0,25 |
| Espesor en el rebaje (*D-cut*) | ≈2,5 mm | **2,65 – 2,75 mm** | +0,15 / +0,25 |

> **Mide el eje con el calibre antes de dibujar**: los datos del fabricante bailan entre lotes. Y
> comprueba la holgura con una **probeta de calibración** (un cubo con agujeros de 3,1 / 3,2 / 3,3 mm)
> impresa con la misma boquilla, material y altura de capa. Es el único método que vale.

### Trucos que ahorran el problema

| Truco | Por qué |
|:------|:--------|
| **Imprimir la rueda con el eje del agujero VERTICAL** | El agujero sale **redondo**; en horizontal sale ovalado y con rebabas |
| **Chaflán de 0,5 × 45° en la boca del agujero** | Guía el eje al entrar y quita el *elephant foot* |
| **Escariar** con una broca de 3 mm girada **a mano** (sin taladro) | Deja el agujero a medida sin arrancar material de más |
| **Prisionero M2 radial** contra la cara plana (piloto de **1,6–1,7 mm**) | **La mejor solución**: libera al agujero de sujetar, así que la rueda no gira respecto al eje aunque el agujero sea holgado, y se puede desmontar |
| Montar la rueda **con el eje fuera del motor** | Forzarla con el motor sujeto castiga la reductora |

#### Qué es imprescindible y qué es opcional

Para que la rueda quede alineada bastan **dos cosas**:

1. El **agujero en D** — hace de chavetero: impide que la rueda gire respecto al eje.
2. El **cubo de 6–8 mm** — impide que bascule.

El **prisionero es una mejora de la v2**, para cuando la rueda esté montada y se vea si aún se mueve. No
hace falta para la primera impresión, y hacerlo sin CAD también vale (granete + broca de 1,6 mm a mano,
perpendicular al eje y apuntando a la cara plana).

Si al final lo quieres en el modelo, en FreeCAD son cinco pasos con el banco *Part Design*:

1. Seleccionar la **cara exterior del cubo** → *New sketch*.
2. Dibujar un **círculo de Ø1,7 mm** centrado en la dirección de la **cara plana** del agujero (enfrente del D, no de la parte redonda).
3. *Pocket* → **Through all**.
4. (*Opcional*) *Chamfer* de 0,5 mm en la boca.
5. Tornillo **M2 autotaladrante** para plástico.

> ⚠️ Deja la **pared del cubo en 2–2,5 mm** y no aprietes como si fuera metal: el PLA se ovala y la rueda
> empieza a bailar igual que antes.

---

## 4. Cotas objetivo (a verificar con los STEP)

| Cota | Valor actual en la doc | ¿Verificado en 3D? |
|:-----|:-----------------------|:------------------:|
| Trocha efectiva $L$ | **77,6 mm** ✅ (medida en el robot, 1-oct) | ✅ prueba 19 |
| Distancia entre ejes de motor | 83,6 mm | ❌ |
| Diámetro de rueda (con goma) | **Ø24 mm** ⇒ $r$ = 12 mm | ✅ medido 30-sep |
| Chasis (largo × ancho) | 106,7 × 84 mm | ❌ |
| Altura de los sensores IR al suelo | — | ❌ |
| Posición de los 4 canales IR | ⚠️ sin decidir | ❌ |

---

## 5. Llanta rediseñada: cotas medidas del STEP

Medidas sobre `freecad/llanta-no-engranaje.step` (30-sep, con `grep`/`awk` sobre los cilindros,
planos y puntos del propio STEP, sin abrir FreeCAD). El **eje de la rueda es Y** en el modelo.

| Cota | Valor | Veredicto |
|:-----|:------|:---------:|
| Diámetro exterior (llanta) | **Ø20 mm** (radio 10) | ✅ igual que la anterior ⇒ **el µm/cuenta no cambia** |
| Ancho de la rueda | **8,0 mm** (Y de −8 a 0) | ✅ |
| **Agujero del eje** | **Ø3,15 mm** (radio 1,575) | ✅ holgura de +0,15 |
| **Cara plana del *D*** | **1,100 mm del centro** | ✅ rebaje de +0,175 |
| Longitud del agujero | **5,0 mm** (Y de 0 a −5) | ✅ |
| Radios/nervios | 20 (líneas rectas a 18°) | — |

> [!TIP] ✅ **Revisión del 30-sep (21:57): a imprimir**
> Comprobado sobre el STEP: el agujero es de radio **1,575** (Ø3,15) y **no queda ninguna entidad de
> radio 1,5**; la cara plana del *D* está a **1,100 mm** —los puntos del contorno caen en
> $x = \pm\sqrt{1{,}575^2 - 1{,}100^2} = \pm1{,}1272$, que es exactamente lo que debe salir— y el ancho
> son **8,0 mm**.
>
> | Holgura | Cuenta | Valor |
> |:--------|:-------|:-----:|
> | Del diámetro | 3,15 − 3,00 | **+0,15 mm** |
> | Del rebaje | (1,575 + 1,100) − 2,50 | **+0,175 mm** |
>
> Las dos dentro de lo que pide §3b. **Se puede imprimir.**
>
> Al montarla: **imprimir con el eje del agujero vertical** (sale redondo), chaflán en la boca, y si
> entra justa, **escariar con una broca de 3 mm girada a mano**. Montar con el eje **fuera del motor**.
