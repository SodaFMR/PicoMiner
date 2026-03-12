# Arquitectura -- Vision General del Sistema

Documento tecnico que describe la arquitectura completa del sistema PicoMiner:
la particion hardware/software, el flujo de datos, y la integracion de todos
los componentes.

---

## Indice

1. [Vision general](#1-vision-general)
2. [Particion ARM/FPGA](#2-particion-armfpga)
3. [Diagrama del sistema](#3-diagrama-del-sistema)
4. [Optimizacion de midstate](#4-optimizacion-de-midstate)
5. [Cabecera de bloque Bitcoin](#5-cabecera-de-bloque-bitcoin)
6. [Orden de bytes](#6-orden-de-bytes)
7. [Flujo de datos detallado](#7-flujo-de-datos-detallado)
8. [Mapa de ficheros del proyecto](#8-mapa-de-ficheros-del-proyecto)

---

## 1. Vision general

PicoMiner es un acelerador de mineria Bitcoin que implementa el doble SHA-256
(Proof of Work) en la logica programable (PL) de un Zynq-7020, controlado por
el procesador ARM Cortex-A9 del Processing System (PS).

**Objetivo**: Minar el Bloque 939260 de Bitcoin (4 de marzo de 2026) mediante
busqueda por fuerza bruta del nonce desde 0, demostrando la aceleracion
hardware del algoritmo de Proof of Work.

**Plataforma**: ZedBoard con Xilinx Zynq-7020 (`xc7z020clg484-1`)

**Herramientas**:
- Vivado HLS 2019.1 (sintesis C-a-RTL)
- Vivado 2019.1 (diseno de bloques, bitstream)
- Xilinx SDK 2019.1 (driver ARM bare-metal)

---

## 2. Particion ARM/FPGA

La clave del diseno es la **optimizacion de midstate**, que define como se
reparte el trabajo entre ARM y FPGA:

| Componente | Tarea | Frecuencia |
|------------|-------|------------|
| **ARM (PS)** | Calcular midstate (SHA-256 de chunk 1) | **1 vez** por bloque |
| **ARM (PS)** | Escribir parametros, iniciar FPGA, leer resultados | 1 vez por chunk |
| **ARM (PS)** | Progreso UART + LEDs + medicion de tiempo | 1 vez por chunk |
| **ARM (PS)** | Verificacion software post-mineria | **1 vez** al final |
| **FPGA (PL)** | SHA-256 chunk 2 (64 rondas) | 1 vez por nonce |
| **FPGA (PL)** | SHA-256 segundo hash (64 rondas) | 1 vez por nonce |
| **FPGA (PL)** | Comprobacion de dificultad | 1 vez por nonce |

El FPGA realiza **128 rondas de compresion SHA-256 por nonce** (2 compresiones
x 64 rondas). A 100 MHz con II=1, esto da ~781 KH/s.

---

## 3. Diagrama del sistema

```
+------------------------------------------------------------------+
|                         ZedBoard (Zynq-7020)                     |
|                                                                  |
|  +---------------------------+    +---------------------------+  |
|  |     Processing System     |    |   Programmable Logic      |  |
|  |        (ARM PS)           |    |        (FPGA PL)          |  |
|  |                           |    |                           |  |
|  |  +---------------------+ |    |  +---------------------+  |  |
|  |  | pico_miner_arm.c    | |    |  | pico_miner IP       |  |  |
|  |  |                     | |    |  | (Vivado HLS)        |  |  |
|  |  | - Midstate calc     |========| - SHA-256 compress   |  |  |
|  |  | - Chunk management  | AXI  | | - Mining loop        |  |  |
|  |  | - Progress display  | Lite | | - Difficulty check   |  |  |
|  |  | - SW verification   | |    |  +---------------------+  |  |
|  |  +---------------------+ |    |                           |  |
|  |                           |    |  +---------------------+  |  |
|  |  +---------------------+ |    |  | AXI GPIO            |  |  |
|  |  | UART (Terminal)     | |    |  | (8-bit output)      |  |  |
|  |  | - Banner            | |    |  +----------+----------+  |  |
|  |  | - Progress lines    | |    |             |             |  |
|  |  | - Verification      | |    +---------------------------+  |
|  |  +---------------------+ |                  |                |
|  +---------------------------+                  |                |
|                                        +--------+--------+      |
|                                        |  8 LEDs ZedBoard |      |
|                                        |  (barra progreso)|      |
|                                        +-----------------+      |
+------------------------------------------------------------------+
```

### Conexiones

- **AXI-Lite**: Bus de 32 bits entre ARM y la IP HLS. El ARM escribe registros
  de entrada (midstate, tail, nonce_start/end, target) y lee registros de
  salida (found_nonce, status). Tambien controla ap_start/ap_done.

- **AXI GPIO**: Bus AXI-Lite separado conectado a los 8 LEDs del ZedBoard.
  El ARM escribe patrones de 8 bits para la barra de progreso.

- **UART**: Puerto serie del PS para salida de texto al terminal (115200 baud).

---

## 4. Optimizacion de midstate

### 4.1 Sin optimizacion

La cabecera de Bitcoin tiene 80 bytes. SHA-256 procesa bloques de 64 bytes.
Sin optimizacion, se necesitan **3 compresiones** por nonce:

```
Bloque 1 (bytes 0-63):   Compresion 1  ----+
                                            |
Bloque 2 (bytes 64-79 + padding):           v
                          Compresion 2  --> Primer hash (32 bytes)
                                            |
Bloque 3 (hash + padding):                 v
                          Compresion 3  --> Hash final (32 bytes)
```

### 4.2 Con optimizacion (PicoMiner)

Los bytes 0-63 (chunk 1) no contienen el nonce. Su resultado (midstate)
es **constante** para todos los nonces. El ARM lo calcula una sola vez:

```
ARM (una vez):
    Chunk 1 (bytes 0-63) --> SHA-256 --> midstate (8 palabras)

FPGA (por cada nonce):
    midstate + [tail, nonce, padding] --> SHA-256 --> primer hash
    H_init + [primer hash, padding]   --> SHA-256 --> hash final
    if hash_final[7] == 0: ENCONTRADO
```

**Resultado**: 2 compresiones por nonce en vez de 3 = **33% de reduccion**.

### 4.3 Datos transferidos ARM -> FPGA

| Dato | Palabras | Descripcion |
|------|----------|-------------|
| `midstate[0..7]` | 8 | Estado SHA-256 tras chunk 1 |
| `chunk2_tail[0..2]` | 3 | merkle_tail, timestamp, bits |
| `nonce_start` | 1 | Inicio del rango |
| `nonce_end` | 1 | Fin del rango |
| `target_hi` | 1 | Umbral de dificultad |
| **Total** | **14** | 14 escrituras de 32 bits por invocacion |

---

## 5. Cabecera de bloque Bitcoin

La cabecera de 80 bytes se estructura asi:

| Campo | Bytes | Palabras | Contenido (Bloque 939260) |
|-------|-------|----------|---------------------------|
| Version | 0-3 | 1 | `0x3c000000` |
| Hash bloque anterior | 4-35 | 8 | 256 bits |
| Raiz de Merkle | 36-67 | 8 | 256 bits |
| Timestamp | 68-71 | 1 | `0x69a828bf` |
| Bits (dificultad) | 72-75 | 1 | `0x1701f303` |
| Nonce | 76-79 | 1 | `0x1e740a08` (LE) |

### Division para midstate

```
Chunk 1 (palabras 0-15):  Version + Hash anterior + Merkle[0..27]
                           = 64 bytes -> midstate

Chunk 2 (palabras 16-19): Merkle[28..31] + Timestamp + Bits + Nonce
                           = 16 bytes (+ 48 bytes de padding SHA-256)
```

---

## 6. Orden de bytes

### 6.1 Bitcoin vs SHA-256

- **Bitcoin** serializa los datos en **little-endian** (LE) en la red
- **SHA-256** procesa palabras en **big-endian** (BE)

El driver ARM byte-swapea las 20 palabras del header al inicio:

```c
// src/pico_miner_arm.c, lineas 265-266
for (i = 0; i < 20; i++)
    header_be[i] = bswap32(BLOCK_HEADER_LE[i]);
```

### 6.2 Hash de visualizacion

Bitcoin muestra los hashes en **orden de bytes invertido**. Los ceros iniciales
del hash mostrado corresponden a `final_hash[7]` (la ultima palabra de la
salida SHA-256), no a `final_hash[0]`.

```
SHA-256 output: [0] [1] [2] [3] [4] [5] [6] [7]
Display:        [7] [6] [5] [4] [3] [2] [1] [0]  (cada palabra byte-swapped)
```

Para el Bloque 939260:
```
Display: 000000000000000000017588478b3612...
          ^^^^^^^^
          Estos ceros vienen de final_hash[7] = 0x00000000
```

### 6.3 Orden del nonce

El nonce en el header LE del Bloque 939260 es `0x1E740A08`. Byte-swapped a BE:
`0x080A741E` = 134,869,022 en decimal. El FPGA itera directamente en BE.

---

## 7. Flujo de datos detallado

```
Cabecera Bitcoin (80 bytes, LE)
        |
        | bswap32 (ARM)
        v
Cabecera BE (20 palabras x 32 bits)
        |
        +--- Chunk 1 [0..15] ----> SHA-256 compress (ARM) ----> midstate[8]
        |
        +--- Chunk 2 [16..18] ---> chunk2_tail[3]
        |
        +--- Chunk 2 [19] -------> known_nonce_be
        
        
Para cada chunk de 1M nonces:
        
        midstate[8]    ----+
        chunk2_tail[3] ----+---> AXI-Lite write (14 registros)
        nonce_start    ----+           |
        nonce_end      ----+           v
        target_hi      ----+     +------------+
                                 | FPGA IP    |
                                 |            |
                                 | for each   |
                                 | nonce:     |
                                 |   chunk2   |
                                 |   SHA-256  |
                                 |   SHA-256  |
                                 |   check    |
                                 +-----+------+
                                       |
                                       v
                              AXI-Lite read (2 registros)
                                       |
                                +------+------+
                                | status      |
                                | found_nonce |
                                +-------------+
```

---

## 8. Mapa de ficheros del proyecto

```
PicoMiner/
|
+-- src/
|   +-- pico_miner.h          Cabecera compartida (constantes SHA-256, prototipo)
|   +-- pico_miner.cpp        Nucleo HLS (SHA-256 compress + mining loop)
|   +-- pico_miner_tb.cpp     Testbench HLS (5 tests)
|   +-- pico_miner_arm.c      Driver ARM bare-metal (Bloque 939260 demo)
|
+-- doc/
|   +-- memoria.tex           Memoria del proyecto (LaTeX, espanol)
|   +-- memoria.pdf           Memoria compilada
|   +-- HLS.md                Detalles de sintesis de alto nivel (este proyecto)
|   +-- ARM.md                Driver ARM e integracion PS/PL
|   +-- VERIFICACION.md       Estrategia de pruebas y validacion
|   +-- ARQUITECTURA.md       Vision general del sistema (este documento)
|
+-- run_hls.tcl               Script de automatizacion Vivado HLS
+-- README.md                 Documentacion principal del repositorio
+-- .gitignore                Exclusiones de git
```

### Descripcion de cada fichero fuente

| Fichero | Lineas | Descripcion |
|---------|--------|-------------|
| `src/pico_miner.h` | 83 | Constantes SHA-256 (H0-H7, K[64]), macros dimensionales, codigos de estado, prototipo de `pico_miner()` |
| `src/pico_miner.cpp` | 225 | Funcion de compresion SHA-256 con 25 pragmas HLS + bucle de mineria con doble hash y comprobacion de dificultad |
| `src/pico_miner_tb.cpp` | 508 | Golden model SW, 5 tests (NIST, Block 1 full-range, Block 1 HW, Block 939260 HW, no-solution) |
| `src/pico_miner_arm.c` | 466 | Inicializacion PS, calculo de midstate, mineria por chunks con progreso UART + LEDs, verificacion SW |
| `run_hls.tcl` | 138 | Configuracion del proyecto HLS, 2 soluciones (II=1 y II=2), csim+csynth+cosim+export |

### Datos del Bloque 939260

| Campo | Valor |
|-------|-------|
| Altura | 939,260 |
| Fecha | 4 de marzo de 2026 |
| Dificultad | 144,398,401,518,101 |
| Nonce (LE) | `0x1E740A08` |
| Nonce (BE) | `0x080A741E` (~134.9M) |
| Hash | `000000000000000000017588478b3612182486d33006ede1164fb146fa41cd06` |
| Tiempo estimado | ~2 min 53 s a 781 KH/s |
