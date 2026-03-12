# HLS -- Diseno Hardware a Nivel de Sintesis de Alto Nivel

Documento tecnico que detalla todo el trabajo realizado a nivel de hardware
mediante Vivado HLS 2019.1 para el acelerador de mineria Bitcoin PicoMiner.

---

## Indice

1. [Funcion de compresion SHA-256](#1-funcion-de-compresion-sha-256)
2. [Pragmas HLS y decisiones de diseno](#2-pragmas-hls-y-decisiones-de-diseno)
3. [Funcion top-level: pico_miner](#3-funcion-top-level-pico_miner)
4. [Interfaz AXI-Lite](#4-interfaz-axi-lite)
5. [Bucle de mineria](#5-bucle-de-mineria)
6. [Soluciones de sintesis](#6-soluciones-de-sintesis)
7. [Script TCL de automatizacion](#7-script-tcl-de-automatizacion)
8. [Estimaciones de rendimiento](#8-estimaciones-de-rendimiento)
9. [Inventario completo de pragmas](#9-inventario-completo-de-pragmas)

---

## 1. Funcion de compresion SHA-256

**Archivo:** `src/pico_miner.cpp`, lineas 34-89

La funcion `sha256_compress` es el nucleo computacional del diseno. Implementa
una compresion SHA-256 completa sobre un bloque de 64 bytes (512 bits), siguiendo
el estandar FIPS 180-4.

### 1.1 Macros auxiliares

Definidas en `src/pico_miner.cpp`, lineas 22-32:

```c
#define ROTR(x, n)  (((x) >> (n)) | ((x) << (32 - (n))))
#define SHR(x, n)   ((x) >> (n))

#define CH(e, f, g)   (((e) & (f)) ^ ((~(e)) & (g)))
#define MAJ(a, b, c)  (((a) & (b)) ^ ((a) & (c)) ^ ((b) & (c)))

#define SIGMA0(a)  (ROTR(a, 2)  ^ ROTR(a, 13) ^ ROTR(a, 22))
#define SIGMA1(e)  (ROTR(e, 6)  ^ ROTR(e, 11) ^ ROTR(e, 25))
#define sigma0(x)  (ROTR(x, 7)  ^ ROTR(x, 18) ^ SHR(x, 3))
#define sigma1(x)  (ROTR(x, 17) ^ ROTR(x, 19) ^ SHR(x, 10))
```

**Implicacion hardware:** Cada rotacion (`ROTR`) se implementa como un
recableado de bits (coste cero en logica). Las operaciones XOR, AND y NOT son
puertas basicas. Todo el conjunto es puramente combinacional.

### 1.2 Expansion del message schedule (W)

**Archivo:** `src/pico_miner.cpp`, lineas 46-58

```c
unsigned int W[64];
#pragma HLS ARRAY_PARTITION variable=W complete dim=1

load_W:
for (i = 0; i < 16; i++) {
#pragma HLS UNROLL
    W[i] = W_in[i];
}

expand_W:
for (i = 16; i < 64; i++) {
#pragma HLS UNROLL
    W[i] = sigma1(W[i-2]) + W[i-7] + sigma0(W[i-15]) + W[i-16];
}
```

**Decisiones de diseno:**

- `ARRAY_PARTITION complete`: El array `W[64]` se descompone en **64 registros
  individuales**. Sin esta directiva, HLS colocaria el array en BRAM con puertos
  limitados, creando un cuello de botella ya que la expansion necesita acceso
  simultaneo a `W[i-2]`, `W[i-7]`, `W[i-15]` y `W[i-16]`.

- `UNROLL` en `load_W`: Las 16 palabras de entrada se cargan en un unico ciclo
  de reloj (16 escrituras paralelas a registros).

- `UNROLL` en `expand_W`: Las 48 palabras restantes (W[16]..W[63]) se calculan
  de forma **completamente combinacional** en un unico ciclo. Esto genera una
  red combinacional profunda pero el resultado se almacena en registros.

### 1.3 Rondas de compresion

**Archivo:** `src/pico_miner.cpp`, lineas 64-78

```c
compress:
for (i = 0; i < 64; i++) {
#pragma HLS PIPELINE II=1
    unsigned int temp1 = h + SIGMA1(e) + CH(e, f, g) + SHA256_K[i] + W[i];
    unsigned int temp2 = SIGMA0(a) + MAJ(a, b, c);
    h = g; g = f; f = e; e = d + temp1;
    d = c; c = b; b = a; a = temp1 + temp2;
}
```

**`PIPELINE II=1`** es la directiva mas critica del diseno. Indica a HLS que
inicie una nueva ronda de compresion en **cada ciclo de reloj** (Initiation
Interval = 1).

Cada ronda realiza en un solo ciclo:
- 5 sumas de 32 bits (`h + SIGMA1(e) + CH(e,f,g) + K[i] + W[i]` y `d + temp1`)
- Rotaciones multiples (SIGMA0, SIGMA1 -- sin coste en logica)
- Operaciones logicas (CH, MAJ)
- Desplazamiento de las 8 variables de trabajo

Esta es la **ruta critica de timing** del diseno. A 100 MHz (10 ns), encajar
5 sumas + rotaciones + logica en un solo ciclo es agresivo. Por eso existe la
Solucion 2 como alternativa con II=2.

### 1.4 INLINE off

**Archivo:** `src/pico_miner.cpp`, linea 40

```c
#pragma HLS INLINE off
```

`sha256_compress` se llama **dos veces** por nonce en el bucle de mineria
(primera SHA-256 con midstate, segunda SHA-256 con valores iniciales). La
directiva `INLINE off` impide que HLS inserte el cuerpo de la funcion en cada
punto de llamada, lo que **duplicaria el area de logica**. Al mantenerla como
un modulo RTL separado, el hardware se comparte y las dos llamadas se ejecutan
secuencialmente.

---

## 2. Pragmas HLS y decisiones de diseno

### 2.1 Filosofia general

El diseno sigue una estrategia de **maximizar el throughput por nonce**:

1. **Todo en registros**: Ningun array interno usa BRAM. Todos los arrays
   (`W[64]`, `ms[8]`, `tail[3]`, `chunk2_W[16]`, etc.) estan completamente
   particionados en registros individuales mediante `ARRAY_PARTITION complete`.

2. **Bucles de copia totalmente desenrollados**: Todos los bucles de copia
   (`load_W`, `cache_ms`, `cache_tail`, `build_hash2_msg`) usan `UNROLL`
   para ejecutarse en un unico ciclo.

3. **Pipeline agresivo**: El bucle de compresion usa `PIPELINE II=1` para
   procesar una ronda por ciclo de reloj.

4. **Comparticion de hardware**: `sha256_compress` se mantiene como modulo
   separado (`INLINE off`) para compartir la logica entre las dos llamadas.

### 2.2 Cache de entradas

**Archivo:** `src/pico_miner.cpp`, lineas 120-136

```c
unsigned int ms[8];
#pragma HLS ARRAY_PARTITION variable=ms complete dim=1
unsigned int tail[3];
#pragma HLS ARRAY_PARTITION variable=tail complete dim=1

cache_ms:
for (i = 0; i < 8; i++) {
#pragma HLS UNROLL
    ms[i] = midstate[i];
}
cache_tail:
for (i = 0; i < 3; i++) {
#pragma HLS UNROLL
    tail[i] = chunk2_tail[i];
}
```

Los valores `midstate` y `chunk2_tail` se copian desde los registros AXI-Lite a
registros locales **una unica vez** al inicio de la funcion. Esto evita que el
bucle de mineria acceda al bus AXI en cada iteracion.

---

## 3. Funcion top-level: pico_miner

**Archivo:** `src/pico_miner.cpp`, lineas 92-225

La funcion `pico_miner` es la **funcion top** que Vivado HLS sintetiza a RTL.
Su signatura define la interfaz hardware completa:

```c
void pico_miner(
    unsigned int midstate[8],       // Estado SHA-256 tras chunk 1 (del ARM)
    unsigned int chunk2_tail[3],    // merkle_tail, timestamp, bits
    unsigned int nonce_start,       // Inicio del rango de busqueda
    unsigned int nonce_end,         // Fin del rango (exclusivo)
    unsigned int target_hi,         // Objetivo de dificultad
    unsigned int *found_nonce,      // Salida: nonce encontrado
    unsigned int *status            // Salida: MINING_FOUND o MINING_NOT_FOUND
);
```

Cada parametro se convierte en uno o mas registros AXI-Lite accesibles desde el
procesador ARM.

---

## 4. Interfaz AXI-Lite

**Archivo:** `src/pico_miner.cpp`, lineas 101-109

```c
#pragma HLS INTERFACE s_axilite port=midstate     bundle=myaxi
#pragma HLS INTERFACE s_axilite port=chunk2_tail  bundle=myaxi
#pragma HLS INTERFACE s_axilite port=nonce_start  bundle=myaxi
#pragma HLS INTERFACE s_axilite port=nonce_end    bundle=myaxi
#pragma HLS INTERFACE s_axilite port=target_hi    bundle=myaxi
#pragma HLS INTERFACE s_axilite port=found_nonce  bundle=myaxi
#pragma HLS INTERFACE s_axilite port=status       bundle=myaxi
#pragma HLS INTERFACE s_axilite port=return       bundle=myaxi
```

**Todos los puertos** se mapean a un unico bus AXI-Lite esclavo (bundle `myaxi`).
Esto genera una interfaz con una unica direccion base y registros en offsets fijos.

### 4.1 Mapeado de registros

Vivado HLS 2019.1 mapea arrays como **registros escalares individuales**. El
array `midstate[8]` genera 8 registros (`midstate_0` a `midstate_7`), cada uno
con su propia funcion `Set`/`Get` en el driver auto-generado.

| Parametro | Tipo | Registros | Funcion SDK |
|-----------|------|-----------|-------------|
| `midstate[0..7]` | Entrada | 8 x 32 bits | `XPico_miner_Set_midstate_0..7` |
| `chunk2_tail[0..2]` | Entrada | 3 x 32 bits | `XPico_miner_Set_chunk2_tail_0..2` |
| `nonce_start` | Entrada | 1 x 32 bits | `XPico_miner_Set_nonce_start` |
| `nonce_end` | Entrada | 1 x 32 bits | `XPico_miner_Set_nonce_end` |
| `target_hi` | Entrada | 1 x 32 bits | `XPico_miner_Set_target_hi` |
| `found_nonce` | Salida | 1 x 32 bits | `XPico_miner_Get_found_nonce` |
| `status` | Salida | 1 x 32 bits | `XPico_miner_Get_status` |
| `return` (control) | Control | 1 x 32 bits | `XPico_miner_Start`, `IsDone` |

### 4.2 Protocolo de control

El pragma `port=return` mapea las senales de control del bloque HLS:

- **`ap_start`** (bit 0 del registro de control): El ARM lo pone a 1 para
  iniciar la ejecucion.
- **`ap_done`** (bit 1): La IP lo pone a 1 cuando termina.
- **`ap_idle`** (bit 2): Indica que la IP esta ociosa.
- **`ap_ready`** (bit 3): Indica que esta lista para una nueva invocacion.

El flujo desde el ARM es: Escribir entradas -> Start -> Poll Done -> Leer salidas.

---

## 5. Bucle de mineria

**Archivo:** `src/pico_miner.cpp`, lineas 147-220

### 5.1 Estructura del bucle

```c
mining_loop:
for (nonce = nonce_start; nonce < nonce_end; nonce++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=16777216
    if (found) continue;
    // ... construir chunk2, doble SHA-256, comprobar dificultad ...
}
```

- **`LOOP_TRIPCOUNT`**: Es una directiva informativa (no afecta a la sintesis).
  Indica a HLS que el bucle ejecutara entre 1 y 16M iteraciones para generar
  estimaciones de rendimiento en los informes.

- **`if (found) continue`**: Una vez encontrado un nonce valido, las iteraciones
  restantes se saltan. Esto es mas sencillo que un `break`, que puede complicar
  la sintesis HLS.

### 5.2 Trabajo por nonce

Cada iteracion del bucle realiza:

1. **Construir bloque chunk 2** (lineas 162-174): Se monta el bloque de 16
   palabras para la primera compresion:
   - `W[0..2]` = merkle_tail, timestamp, bits (constantes del `tail`)
   - `W[3]` = **nonce** (la variable que se itera)
   - `W[4]` = `0x80000000` (padding SHA-256)
   - `W[5..14]` = 0
   - `W[15]` = `0x00000280` (longitud del mensaje = 640 bits)

2. **Primera SHA-256** (linea 179): `sha256_compress(ms, chunk2_W, first_hash)`
   Comprime el chunk 2 usando el midstate como estado inicial.

3. **Construir bloque para segunda SHA-256** (lineas 188-199): El hash de 32
   bytes (8 palabras) se rellena hasta 64 bytes:
   - `W[0..7]` = first_hash
   - `W[8]` = `0x80000000`
   - `W[9..14]` = 0
   - `W[15]` = `0x00000100` (longitud = 256 bits)

4. **Segunda SHA-256** (linea 204): `sha256_compress(H_init, hash2_W, final_hash)`
   Hash del hash usando los valores iniciales estandar SHA-256.

5. **Comprobacion de dificultad** (linea 215):
   ```c
   if (final_hash[7] <= target_hi) { ... }
   ```
   Bitcoin muestra los hashes en orden de bytes invertido. Los ceros iniciales
   del hash en formato de visualizacion corresponden a `final_hash[7]` (la
   **ultima** palabra de la salida SHA-256), no a `final_hash[0]`.

### 5.3 Ciclos por nonce

Cada compresion SHA-256 tarda ~64 ciclos (64 rondas con II=1). Con dos
compresiones por nonce, el total es **~128 ciclos por nonce**.

---

## 6. Soluciones de sintesis

El proyecto define dos soluciones en el script TCL (`run_hls.tcl`):

### 6.1 Solucion 1: Baseline (II=1)

**Archivo:** `run_hls.tcl`, lineas 58-77

- Usa las directivas tal como estan en el codigo fuente
- El bucle `compress` ejecuta una ronda por ciclo (II=1)
- ~128 ciclos por nonce
- Throughput estimado: **~781 KH/s** a 100 MHz
- Ruta critica: 5 sumas + rotaciones + logica en un ciclo de 10 ns

```
100,000,000 ciclos/s / 128 ciclos/nonce = 781,250 nonces/s
```

### 6.2 Solucion 2: Relaxed (II=2)

**Archivo:** `run_hls.tcl`, lineas 88-103

- Sobreescribe la directiva del codigo fuente mediante TCL:
  ```tcl
  set_directive_pipeline -II 2 "sha256_compress/compress"
  ```
- El bucle `compress` tarda 2 ciclos por ronda (II=2)
- ~256 ciclos por nonce
- Throughput estimado: **~390 KH/s** a 100 MHz

Esta solucion existe como **respaldo** en caso de que la Solucion 1 no cumpla
las restricciones de timing a 100 MHz. La ruta critica se relaja al distribuir
el trabajo de cada ronda en 2 ciclos.

| Parametro | Solucion 1 (baseline) | Solucion 2 (relaxed) |
|-----------|-----------------------|----------------------|
| II del bucle compress | 1 | 2 |
| Ciclos por nonce | ~128 | ~256 |
| Throughput a 100 MHz | ~781 KH/s | ~390 KH/s |
| Ruta critica | Agresiva | Relajada |

---

## 7. Script TCL de automatizacion

**Archivo:** `run_hls.tcl` (138 lineas)

El script automatiza todo el flujo HLS:

### 7.1 Configuracion del proyecto

```tcl
set PROJECT_NAME  "pico_miner_hls"
set TOP_FUNCTION  "pico_miner"
set FPGA_PART     "xc7z020clg484-1"
set CLOCK_PERIOD  10
```

- **FPGA**: Zynq-7020 en encapsulado CLG484 (ZedBoard)
- **Reloj**: 10 ns = 100 MHz
- **Funcion top**: `pico_miner`

### 7.2 Flujo por solucion

**Solucion 1** (lineas 58-77):
1. `csim_design` -- Simulacion C: compila y ejecuta el testbench en CPU. Los 5
   tests se ejecutan. Valida la correccion funcional.
2. `csynth_design` -- Sintesis C-a-RTL: analiza los pragmas y genera Verilog/VHDL.
   Produce informes de rendimiento y uso de recursos.
3. `cosim_design` -- Co-simulacion C/RTL: el RTL generado se simula con las
   entradas del testbench. Verifica que el RTL produce los mismos resultados que
   la simulacion C.
4. `export_design` -- Exporta la IP en formato IP Catalog para integracion en
   Vivado.

**Solucion 2** (lineas 88-103):
1. `csynth_design` -- Sintesis con II=2 (sin repetir csim, seria identico).
2. `cosim_design` -- Verifica el RTL con II=2.
3. `export_design` -- Exporta como IP alternativa.

### 7.3 Exportacion de IP

Las IPs se exportan con metadatos descriptivos:

```tcl
export_design -format ip_catalog \
    -description "Pico Miner Bitcoin SHA-256 Accelerator (Midstate, II=1 compress)" \
    -vendor "pico_miner" \
    -display_name "Pico Miner v2.0 SHA-256"
```

La IP exportada incluye:
- Modulos RTL (Verilog)
- Ficheros de driver C auto-generados (`xpico_miner.h`, `xpico_miner.c`,
  `xpico_miner_hw.h`)
- Definicion del interfaz AXI-Lite con offsets de registros

---

## 8. Estimaciones de rendimiento

### 8.1 Calculo de throughput

```
Frecuencia de reloj:       100 MHz = 100,000,000 ciclos/s
Ciclos por compresion:     64 (con II=1)
Compresiones por nonce:    2 (chunk 2 + segundo hash)
Ciclos por nonce:          128
Throughput:                100,000,000 / 128 = 781,250 nonces/s ~ 781 KH/s
```

### 8.2 Tiempo de mineria del Bloque 939260

```
Nonce conocido (BE):       0x080A741E = 134,869,022 en decimal
Throughput:                781,250 nonces/s
Tiempo estimado:           134,869,022 / 781,250 = ~172.7 segundos = ~2 min 53 s
```

### 8.3 Comprobacion de dificultad simplificada

El diseno comprueba `final_hash[7] == 0` (32 bits cero iniciales en el hash de
visualizacion). La dificultad real del Bloque 939260 requiere ~76+ bits cero,
pero la comprobacion de 32 bits es suficiente porque:

- Probabilidad de falso positivo por nonce: 1/2^32 = 2.33 x 10^-10
- Probabilidad acumulada en 134.9M nonces: ~3.1%
- Aceptable para una demostracion

---

## 9. Inventario completo de pragmas

Tabla de todos los pragmas HLS usados en `src/pico_miner.cpp`, con su ubicacion
exacta y proposito:

| Linea | Pragma | Proposito |
|-------|--------|-----------|
| 40 | `INLINE off` | Mantener `sha256_compress` como modulo separado (compartido por 2 llamadas) |
| 42 | `ARRAY_PARTITION W complete dim=1` | 64 registros para acceso paralelo sin contenciones |
| 49 | `UNROLL` (load_W) | Cargar 16 palabras en 1 ciclo |
| 56 | `UNROLL` (expand_W) | Calcular 48 palabras de expansion combinacionalmente |
| 67 | `PIPELINE II=1` (compress) | Una ronda de compresion por ciclo de reloj |
| 102 | `INTERFACE s_axilite port=midstate bundle=myaxi` | Mapear a registro AXI-Lite |
| 103 | `INTERFACE s_axilite port=chunk2_tail bundle=myaxi` | Mapear a registro AXI-Lite |
| 104 | `INTERFACE s_axilite port=nonce_start bundle=myaxi` | Mapear a registro AXI-Lite |
| 105 | `INTERFACE s_axilite port=nonce_end bundle=myaxi` | Mapear a registro AXI-Lite |
| 106 | `INTERFACE s_axilite port=target_hi bundle=myaxi` | Mapear a registro AXI-Lite |
| 107 | `INTERFACE s_axilite port=found_nonce bundle=myaxi` | Mapear a registro AXI-Lite |
| 108 | `INTERFACE s_axilite port=status bundle=myaxi` | Mapear a registro AXI-Lite |
| 109 | `INTERFACE s_axilite port=return bundle=myaxi` | Mapear senales de control |
| 112 | `ARRAY_PARTITION midstate complete dim=1` | Acceso paralelo a midstate de entrada |
| 113 | `ARRAY_PARTITION chunk2_tail complete dim=1` | Acceso paralelo a tail de entrada |
| 122 | `ARRAY_PARTITION ms complete dim=1` | Cache local de midstate en registros |
| 124 | `ARRAY_PARTITION tail complete dim=1` | Cache local de tail en registros |
| 129 | `UNROLL` (cache_ms) | Copiar midstate en 1 ciclo |
| 134 | `UNROLL` (cache_tail) | Copiar tail en 1 ciclo |
| 140 | `ARRAY_PARTITION H_init complete dim=1` | Valores iniciales SHA-256 en registros |
| 149 | `LOOP_TRIPCOUNT min=1 max=16777216` | Hint para estimacion de rendimiento |
| 163 | `ARRAY_PARTITION chunk2_W complete dim=1` | Bloque chunk 2 en registros |
| 178 | `ARRAY_PARTITION first_hash complete dim=1` | Primer hash en registros |
| 189 | `ARRAY_PARTITION hash2_W complete dim=1` | Bloque segundo hash en registros |
| 193 | `UNROLL` (build_hash2_msg) | Copiar first_hash en 1 ciclo |
| 203 | `ARRAY_PARTITION final_hash complete dim=1` | Hash final en registros |

**Total: 25 pragmas HLS** distribuidos entre la funcion de compresion (5) y la
funcion top (20).

---

## Archivos relevantes

| Archivo | Descripcion |
|---------|-------------|
| `src/pico_miner.h` | Cabecera compartida: constantes SHA-256, prototipo de funcion, definiciones |
| `src/pico_miner.cpp` | Nucleo HLS: funcion de compresion SHA-256 + bucle de mineria (225 lineas) |
| `run_hls.tcl` | Script TCL que automatiza csim, csynth, cosim y export (138 lineas) |
