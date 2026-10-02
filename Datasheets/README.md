# 📚 Datasheets — índice

Documentación de los componentes del Rabosa-MVR. **26 PDF, ≈35 MB** (todo versionado en git).

> Cómo volverlos a texto: `pdftotext -layout Datasheets/<fichero>.pdf -` (paquete `poppler-utils`).

---

## 1. Microcontrolador y herramienta de programación

| Componente | Fichero | Qué es |
|:-----------|:--------|:-------|
| STM32F103C8T6 (Blue Pill) | [`stm32f103c8.pdf`](./stm32f103c8.pdf) | Hoja de datos del MCU (pines, tipos `FT`/`TTa`, máximas absolutas, consumos) |
| — | [`stm32f103-rm0008.pdf`](./stm32f103-rm0008.pdf) | **Manual de referencia** (RM0008): registros de TIM, ADC, SPI, I2C… ⚠️ 13 MB |
| — | [`um1850-description-of-stm32f1-hal-and-lowlayer-drivers-stmicroelectronics.pdf`](./um1850-description-of-stm32f1-hal-and-lowlayer-drivers-stmicroelectronics.pdf) | Descripción del **HAL/LL de STM32F1** (UM1850) |
| — | [`stm32f103-an2586-hardware.pdf`](./stm32f103-an2586-hardware.pdf) | AN2586: *getting started* con el hardware STM32F10xxx (alimentación, decoupling, relojes) |
| — | [`stm32f103-an4899-gpio.pdf`](./stm32f103-an4899-gpio.pdf) | AN4899: **configuración de GPIO** y bajo consumo (tipos de pin, tolerancia a 5 V) |
| ST-Link V2 | [`stlinkv2-um1075.pdf`](./stlinkv2-um1075.pdf) | UM1075: manual del programador/depurador ST-LINK/V2 (SWD) |

## 2. Motores y encoder

| Componente | Fichero | Qué es |
|:-----------|:--------|:-------|
| Familia N20 (ASLONG) | [`aslong-n20-gearmotors.pdf`](./aslong-n20-gearmotors.pdf) | Tablas de la familia N20 (JGA13-N20 y 1024-N20), ratios 1:100…1:2100 y **planos con cotas** |
| **GA12-N20 + encoder** (nuestro) | — ver [`../images/motor-encoder/`](../images/motor-encoder/) | El fabricante no publica un PDF: sus tablas y figuras están como imágenes (pinout, circuito de salida, formas de onda, especificaciones) |

## 3. Sensores

| Componente | Fichero | Qué es |
|:-----------|:--------|:-------|
| IMU MPU-9250 | [`mpu-9250-datasheet.pdf`](./mpu-9250-datasheet.pdf) | Product specification (PS-MPU-9250A-01) |
| — | [`mpu-9250-register-map.pdf`](./mpu-9250-register-map.pdf) | Register map (RM-MPU-9250A-00): registros, `WHO_AM_I`, DMP |
| IMU MPU-9255 | [`mpu-9255-datasheet.pdf`](./mpu-9255-datasheet.pdf) · [`mpu-9255-register-map.pdf`](./mpu-9255-register-map.pdf) | Variante de la familia que también etiqueta el módulo clónico |
| Magnetómetro AK8963 | [`ak8963.pdf`](./ak8963.pdf) | El magnetómetro **dentro** del MPU-9250 (`WHO_AM_I` = 0x48, modo *bypass*) |
| Emisor IR | [`sfh4550.pdf`](./sfh4550.pdf) | SFH 4550 (ams-OSRAM): $V_F$, $I_F$, semiángulo, potencia radiada |
| Fototransistor IR | [`st-1kl3a.pdf`](./st-1kl3a.pdf) | ST-1KL3A (Kodenshi): $I_L$, $V_{CE(sat)}$, tiempos de conmutación |

## 4. Alimentación y pantalla

| Componente | Fichero | Qué es |
|:-----------|:--------|:-------|
| Boost 9 V | [`mt3608.pdf`](./mt3608.pdf) | MT3608: elevador conmutado hasta 28 V (alimenta el `VM` del DRV8833) |
| LDO 3,3 V | [`ams1117.pdf`](./ams1117.pdf) | AMS1117 (el del módulo externo y el de la Blue Pill) |
| Buck 5 V | [`mp1584.pdf`](./mp1584.pdf) | **MP1584** (MPS): buck 4,5–28 V → 5 V, 3 A, 1,5 MHz, Iq 100 µA. ⚠️ Está **NRND**: el PDF (Rev 1.0 de 2011) lleva el sello **«NOT RECOMMENDED FOR NEW DESIGNS · REFER TO MP2338»** ⇒ **a sustituir en la PCB V2** por el sucesor que apunta el propio fabricante |
| Pantalla OLED | [`ssd1306.pdf`](./ssd1306.pdf) | Controlador **SSD1306** (Solomon Systech): comandos, páginas, direcciones I2C |

## 5. Driver y conectores

| Componente | Fichero | Qué es |
|:-----------|:--------|:-------|
| Driver de motor | [`drv8833.pdf`](./drv8833.pdf) | DRV8833 (Texas Instruments): puente H doble, límites, tabla de verdad |
| Conector de batería | [`jst-xh.pdf`](./jst-xh.pdf) | Serie **XH** de JST: cotas del conector y de los terminales |
| BLE HJ-580XP *(aparcado)* | [`da14580.pdf`](./da14580.pdf) | DA14580 (Dialog/Renesas): el SoC del módulo BLE |

---

## 6. Transistores

Cinco ficheros cubren **los diez transistores de la caja de componentes**. Los compró el usuario sin
una función asignada; el que se usa hoy es el **BC337** para conmutar los emisores IR (ver
[hardware.md §4.4](../Documentacion/hardware.md#44-sensores-de-pared-ir)).

| Fichero | Partes que cubre | Tipo | Datos clave |
|:--------|:-----------------|:-----|:------------|
| [`bc337.pdf`](./bc337.pdf) | **BC337**, BC337-25, BC337-40 | **NPN** TO-92 | $V_{CEO}$ 45 V · $I_C$ **800 mA** · $P_D$ 625 mW · $h_{FE}$ 100–630 a 100 mA · $V_{CE(sat)}$ 0.7 V máx a 500 mA · **patillaje C-B-E** |
| [`bc546-bc550.pdf`](./bc546-bc550.pdf) | **BC546, BC547, BC548, BC549, BC550** (grupos A/B/C) | NPN TO-92 (señal, los 549/550 de bajo ruido) | $I_C$ **100 mA** · $h_{FE}$ 110–800 a $I_C$ = 2 mA |
| [`bc517.pdf`](./bc517.pdf) | **BC517** | NPN **Darlington** TO-92 | $I_C$ **1.2 A** · $h_{FE}$ **30 000** a 20 mA · ⚠️ $V_{CE(sat)}$ **1.0 V** a 100 mA |
| [`bc327.pdf`](./bc327.pdf) | **BC327**, BC327-16/-25/-40 | **PNP** TO-92 (complementario del BC337) | $V_{CEO}$ −45 V · $I_C$ **−800 mA** · $V_{CE(sat)}$ −0.7 V máx |
| [`bc556-bc558.pdf`](./bc556-bc558.pdf) | **BC556, BC557, BC558** (grupos B/C) | PNP TO-92 (señal) | $I_C$ **−100 mA** (−200 mA de pico) |

> [!TIP] Cómo se consiguieron (onsemi bloquea las descargas directas)
> Las URLs nuevas (`onsemi.com/pdf/datasheet/<parte>-d.pdf`) devuelven 200 con una página HTML, no
> el PDF. Se localizaron por la ruta antigua (`onsemi.com/pub/Collateral/BC337-D.PDF`) leída de
> **web.archive.org**:
> ```
> https://web.archive.org/web/2020id_/https://www.onsemi.com/pub/Collateral/BC337-D.PDF
> ```
> (Nexperia también bloquea: `assets.nexperia.com` contesta 403 y, con *User-Agent* de navegador,
> una página HTML de consentimiento.)

---

## 7. Sin datasheet disponible

| Elemento | Por qué |
|:---------|:--------|
| **Batería LiPo 2S 450 mAh 80C** (Eachine HALOB4502S80C) | Producto de consumo: no hay documento técnico. Datos útiles en [`../Documentacion/hardware.md`](../Documentacion/hardware.md) §4.6. ⚠️ Es **provisional**: se sustituirá por otra más nueva y entonces se añadirán sus especificaciones |
| **Pulsadores táctiles** | Kit genérico comprado en Amazon: cualquier hoja de un pulsador de 6×6 mm sirve (≈50 mA, 12 V) |
| **Conversor TTL↔USB** | Habría que identificar el chip (¿CH340? ¿CP2102?) para bajar su hoja de datos |
