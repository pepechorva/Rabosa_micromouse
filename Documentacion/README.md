# 📚 Documentación — Rabosa-MVR

Índice de la documentación técnica del robot micromouse **Rabosa-MVR**.
La documentación se escribe **junto** al desarrollo, no después: cada hardware, prueba o módulo
nuevo se da de alta aquí.

---

## 🧭 Por dónde empezar

| Si buscas… | Documento |
|---|---|
| Entender el robot completo (qué lleva y cómo se conecta) | [hardware.md](./hardware.md) |
| Saber cómo está montado el software y por qué | [Layered Architecture.md](./Layered%20Architecture.md) |
| **Ver todo el firmware de un vistazo**: capas, APP y máquinas de estados | [esquema-firmware.md](./esquema-firmware.md) |
| Ejecutar, flashear y ver telemetría | [entorno-desarrollo.md](./entorno-desarrollo.md) |
| Mandar órdenes al robot por el puerto serie | [comandos-serie.md](./comandos-serie.md) |
| Probar un periférico paso a paso | [pruebas-bringup.md](./pruebas-bringup.md) |
| **Llevar el robot a un laberinto SIN PC** | [laberinto-sin-pc.md](./laberinto-sin-pc.md) |
| **Ejecutar la campaña de puesta a punto** (los 4 bloques, con sus puertas) | [puesta-a-punto.md](./puesta-a-punto.md) |
| **El plan de mejoras**: qué falló, qué hacen los demás y qué NO copiamos | [plan-mejoras.md](./plan-mejoras.md) |
| **Estado del arte**: qué tenemos, qué funciona, qué falta y en qué orden | [estado-del-arte.md](./estado-del-arte.md) |
| Ver el chasis nuevo (placa base 70 × 90 mm) | [chasis-v1.md](./chasis-v1.md) |
| **Guion de la sesión de pruebas del 2-oct** (histórico) | [sesion-pruebas.md](./sesion-pruebas.md) |
| Ver qué se ha probado ya, día a día | [historial-pruebas.md](./historial-pruebas.md) |
| Saber qué toca hacer ahora | [pruebas-pendientes.md](./pruebas-pendientes.md) |
| Calibrar los encoders y entender las cuentas | [calibracion-encoders.md](./calibracion-encoders.md) |
| Saber qué componentes faltan por comprar | [calculos-diseno.md](./calculos-diseno.md) |
| Ver qué está hecho y qué toca ahora | [estado-del-proyecto.md](./estado-del-proyecto.md) |
| El plan hasta el hito MVP | [planificacion.md](./planificacion.md) |
| Entender los **tecnicismos en inglés** que usa el proyecto | [glosario.md](./glosario.md) |

---

## 📁 Contenido

### Fundamentos y diseño
| Documento | Contenido |
|---|---|
| [Layered Architecture.md](./Layered%20Architecture.md) | Arquitectura por capas APP→CONTROL→FILTER→SENSOR→HAL→HW y regla de aislamiento |
| [glosario.md](./glosario.md) | **Glosario**: tecnicismos en inglés (*duty*, *setpoint*, *bring-up*, *anti-windup*…) con su significado y su uso en este robot |
| [modelado.md](./modelado.md) | Sistemas dinámicos, modelado del motor DC, reductora, inercia, cinemática diferencial |
| [cinemáticas.md](./cinemáticas.md) | Posición, velocidad, aceleración y jerk; perfiles trapezoidal y en S |
| [odometria.md](./odometria.md) | Integración de posición (x, y, θ) y su fusión con el IEKF |
| [esquema-firmware.md](./esquema-firmware.md) | **El firmware de un vistazo**: capas, APP, camino del movimiento, arranque, las dos máquinas de estados y el árbol de menús |
| [firmwares-referencia.md](./firmwares-referencia.md) | Análisis de UKMARSBOT, ZoroBot3 y KERISE: cómo están hechos y qué adoptamos |
| [auditoria-capas.md](./auditoria-capas.md) | **Auditoría de la frontera aplicación/drivers**: la aplicación compila sin el STM32, qué se escapa y por dónde seguir |

### Hardware y puesta en marcha
| Documento | Contenido |
|---|---|
| [hardware.md](./hardware.md) | Componentes, pinout oficial, cadena de alimentación, cableado crítico |
| [laberinto-sin-pc.md](./laberinto-sin-pc.md) | **Chuleta para el laberinto sin PC**: rutas de botones, ajuste de los umbrales de pared y qué hacer si algo va mal |
| [pruebas-bringup.md](./pruebas-bringup.md) | Protocolo de bring-up, catálogo de pruebas de banco y registro de resultados |
| [historial-pruebas.md](./historial-pruebas.md) | **Cronología**: qué se probó cada día, con qué rutina y con qué resultado |
| [pruebas-pendientes.md](./pruebas-pendientes.md) | **Backlog**: hardware por montar, calibraciones, PID, odometría, movimiento y navegación |
| [calibracion-encoders.md](./calibracion-encoders.md) | Teoría y procedimiento de la calibración del encoder (PPR, reducción, µm/cuenta) |
| [calculos-diseno.md](./calculos-diseno.md) | Cálculos de los sensores IR y de los divisores de batería, aviso de batería baja y **lista de la compra** |
| [entorno-desarrollo.md](./entorno-desarrollo.md) | Toolchain, compilación, flasheo, telemetría Teleplot y convenciones de código |
| [comandos-serie.md](./comandos-serie.md) | **Órdenes por USART3** (entrada): `BAT LIST`, `BAT <UMBRAL> <valor>`, `HELP` — ajustar umbrales sin recompilar |
| [estado-del-proyecto.md](./estado-del-proyecto.md) | Snapshot del estado actual y siguientes pasos |
| [estado-del-arte.md](./estado-del-arte.md) | **Estado del arte frente a los firmwares de referencia**: matriz de capacidades, qué funciona medido, qué está escrito y no vale, qué falta (con coste y bloqueos) y las fases F1–F9 con su puerta |
| [maquina-estados-menu.md](./maquina-estados-menu.md) | Máquina de estados y menú con botones+OLED (**implementados el 28-sep**): estados, árbol, API y fases |
| [control-pid.md](./control-pid.md) | **CONTROL**: lazo de velocidad por rueda (PID + rampas), unidades, decisiones de diseño, sintonía Z-N y verificación en el PC |
| [lazo-control.md](./lazo-control.md) | **El lazo de control en interrupción (TIM1)**: el retardo medido (picos de 68,8 ms), qué arregla un ISR y qué no, dónde vive cada cosa y las secciones críticas |
| [puesta-a-punto.md](./puesta-a-punto.md) | **Campaña de puesta a punto**: los 4 bloques (encoders/PWM, sensores, movimiento, aceleraciones), con su procedimiento, sus puertas y sus tablas de resultados |
| [chasis-v1.md](./chasis-v1.md) | **Placa base 70 × 90 mm** con el frente redondeado y muescas para las ruedas: cotas, STEP y cómo regenerarlo |

### Planificación
| Documento | Contenido |
|---|---|
| [planificacion.md](./planificacion.md) | Plan semanal por bloques hasta el 19 de septiembre de 2026 |
| [plan-mejoras.md](./plan-mejoras.md) | **Plan de mejoras tras la competición**: qué falló medido, qué aportan los otros tres firmwares, qué NO copiamos y las fases con su puerta de salida |

### Recursos
| Recurso | Descripción |
|---|---|
| [Pinout.pdf](./Pinout.pdf) / `Pinout.ods` | Pinout de la Blue Pill |
| [encoders.md](../firmware/sensor/encoders.md) | Uso del driver de encoders (TIM2/TIM3) |
| [../Datasheets](../Datasheets/) | Hojas de datos de componentes |
| [../mechanical](../mechanical/) | Diseños propios (FreeCAD) y modelos descargados para el 3D |

> `Obsidian/` se eliminó el 2026-09-21: era un cuaderno personal que quedó desactualizado
> (mencionaba TB6612FNG, otro pinout…). Lo útil se ha volcado a esta carpeta.

---

## ✍️ Convenciones de esta documentación

1. **Un tema, un fichero.** Si crece demasiado, se desdobla.
2. **Tablas antes que prosa**: pinout, pruebas, parámetros y estados van en tabla.
3. **Estado siempre explícito**: ✅ hecho · ⏳ en curso · ⚠️ bloqueado · ❌ pendiente.
4. **Trazabilidad**: cada afirmación de hardware debe indicar de dónde sale
   (datasheet, medición propia o estimación).
5. **Al dar de alta algo nuevo**: primero el documento, después el código.

---

## 🗂️ Estructura del repositorio

```
Rabosa-MVR/
├── README.md              Presentación del proyecto y diagrama de bloques
├── Documentacion/         ← esta carpeta (documentación técnica)
├── firmware/              Firmware STM32 (PlatformIO, arquitectura por capas)
├── Datasheets/            Hojas de datos
├── mechanical/            Piezas 3D: diseños propios (FreeCAD) y modelos de GrabCAD
└── images/                Imágenes y diagramas
```

> **Qué se versiona** (`.gitignore` con lista blanca `*` + excepciones `!`): código del firmware,
> **toda la documentación `.md`**, los PDF/ODS de pinout, los **`Datasheets/*.pdf`**, las
> **`images/`** y las personalizaciones de `.github/`. De `mechanical/` solo entran los `.md` y los
> `freecad/*.FCStd`. Quedan **fuera**: los ZIP/STEP de GrabCAD, `Obsidian/` (cuaderno personal) y los
> artefactos de PlatformIO (`firmware/.pio/`).

> **Última actualización de este índice:** 30 de octubre de 2026.
