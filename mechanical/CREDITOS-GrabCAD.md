# 🙏 Créditos — modelos 3D descargados de GrabCAD

> **A rellenar a mano**: autor (diseñador de GrabCAD) y enlace del modelo.
> Los ficheros están en `mechanical/step/` tal y como se descargaron (los ZIP contienen el STEP
> dentro; algunos están ya descomprimidos junto al ZIP).
>
> La columna **Uso** está pre-rellenada con lo que dice la documentación del robot:
> - **robot** = pieza o módulo que realmente lleva el Rabosa-MVR
> - **referencia** = solo para medir/encajar en el modelo 3D, no va en el robot
> - **¿?** = conviene decidir si se queda

| # | Modelo (GrabCAD) | Fichero en `step/` | Uso | Autor | Enlace |
|:-:|:-----------------|:-------------------|:----|:------|:-------|
| 1 | MCU STM32F103C8T6 Blue Pill | `mc-stm32f103c8t6-blue-pill-1.snapshot.5.zip` | robot | | |
| 2 | MPU-9250 9DOF IMU | `mpu-9250-9dof-imu-1.snapshot.4(.zip)` | robot | | |
| 3 | DRV8833 H-Bridge PCB | `drv8833-h-bridge-pcb-1.snapshot.1.zip` | robot | | |
| 4 | MT3608 Boost converter 2 A | `mt-3608-boost-converter-2amp-1.snapshot.1(.zip)` | robot | | |
| 5 | D-Sun DC-DC step-down (MP1584) | `d-sun-dc-dc-step-down-voltage-regulator-simple-version-1.snapshot.4.zip` | robot | | |
| 6 | AMS1117 step-down ultra thin | `ams1117-step-down-buck-converter-ultra-thin-1.snapshot.4.zip` | robot (¿soporte del display?) | | |
| 7 | Motor GA12-N20 | `motor-ga12-n20-1.snapshot.5.zip` | robot | | |
| 8 | N20 motor mount | `n20-motor-mount-1.snapshot.1.zip` | robot | | |
| 9 | N20 motor mount (v2) | `n20-motor-mount-2.snapshot.1.zip` | robot | | |
| 10 | Clamp for N20 motor | `clamp-for-n20-motor-1.snapshot.1(.zip)` | robot | | |
| 11 | Motor mount body (suelto) | `motor mount-Body.step` | robot | | |
| 12 | LiPo 2S 3000 mAh (Gaoneng) | `lipo-battery-2s-3000mah-by-gaoneng-1.snapshot.1(.zip)` | **referencia** (comprobar tamaños; la del robot es de 450 mAh) | | |
| 13 | OLED display 0.69" 96×16 | `oled-display-0-69-96x16-1.snapshot.10.zip` | **referencia** (el del robot es 128×32) | | |
| 14 | Push button | `push-button-64.snapshot.5.zip` | robot | | |
| 15 | XH 2.54 female connector | `xh-2-54-female-connector-1.snapshot.25.zip` | robot | | |
| 16 | Male pin ×1 a ×20 | `male-pin-x1-to-x20-1.snapshot.48.zip` | robot (tiras de pines) | | |
| 17 | Double-sided FR-4 perfboard con esquinas M2 | `double-sided-fr-4-perfboards-w-m2-corners-1.snapshot.6.zip` | robot (placa de prototipos) | | |
| 18 | Line Sensor Assy | `LineSensorAssy.step` | **referencia** | | |
| 19 | TCRT5000 line sensor PCB | `tcrt5000-line-sensor-pcb-1.snapshot.1.zip` | **referencia** | | |
| 20 | ESP32-C3 mini | `esp32-c3-mini-1.snapshot.6.zip` | **¿?** (no aparece en la documentación; 71 MB) | | |

> Carpetas sueltas en `step/` que parecen copias ya descomprimidas de algunos de los modelos
> anteriores: `connector`, `display`, `motor`, `microcontroller`, `pcb`, `power`, `push button`,
> `mpu-9250-9dof-imu-1.snapshot.4`, `mt-3608-boost-converter-2amp-1.snapshot.1`.

---

## Cómo se usan estos créditos

1. Rellenar **Autor** y **Enlace** de cada fila.
2. Las filas marcadas como **referencia** se quedarán igualmente citadas (se usaron para el modelo
   3D), pero no forman parte del robot.
3. Cuando estén todos los enlaces, se puede decidir si los `.step` se versionan en git
   (ahora solo entra `mechanical/freecad/*.FCStd`).
