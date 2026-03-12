# Verificación -- Estrategia de Pruebas y Validación

Documento técnico que detalla la estrategia de verificación del acelerador
PicoMiner a múltiples niveles: simulación C, co-simulación RTL, y verificación
en placa.

---

## Índice

1. [Niveles de verificación](#1-niveles-de-verificación)
2. [Golden model software](#2-golden-model-software)
3. [Test 1: Vector conocido NIST SHA-256](#3-test-1-vector-conocido-nist-sha-256)
4. [Test 2: Minería completa del Bloque 1 en software](#4-test-2-minería-completa-del-bloque-1-en-software)
5. [Test 3: Bloque 1 en hardware (ventana estrecha)](#5-test-3-bloque-1-en-hardware-ventana-estrecha)
6. [Test 4: Bloque 939260 en hardware (ventana estrecha)](#6-test-4-bloque-939260-en-hardware-ventana-estrecha)
7. [Test 5: Sin solución en rango](#7-test-5-sin-solución-en-rango)
8. [Co-simulación C/RTL](#8-co-simulación-crtl)
9. [Verificación en placa (ARM)](#9-verificación-en-placa-arm)

---

## 1. Niveles de verificación

El proyecto implementa tres niveles de verificación independientes:

| Nivel | Herramienta | Qué valida |
|-------|-------------|------------|
| **C Simulation (csim)** | Vivado HLS `csim_design` | Corrección funcional del algoritmo C |
| **C/RTL Co-simulation (cosim)** | Vivado HLS `cosim_design` | El RTL generado produce resultados idénticos al C |
| **Verificación en placa** | Xilinx SDK + ZedBoard | El sistema completo PS+PL funciona correctamente |

El testbench (`src/pico_miner_tb.cpp`, 508 líneas) se usa para los dos primeros
niveles. El driver ARM (`src/pico_miner_arm.c`) incluye su propia verificación
para el tercer nivel.

---

## 2. Golden model software

**Archivo:** `src/pico_miner_tb.cpp`, líneas 28-89

El testbench incluye una implementación **completamente independiente** de
SHA-256 en software, sin compartir código con la implementación HLS:

```c
static void sha256_compress_sw(const unsigned int state_in[8],
                                const unsigned int msg[16],
                                unsigned int state_out[8])
```

Esta función usa el mismo algoritmo SHA-256 (FIPS 180-4) pero sin pragmas HLS.
Sirve como **modelo de referencia** (golden model) contra el cual se comparan
los resultados del acelerador hardware.

### Funciones auxiliares del golden model

| Función | Líneas | Propósito |
|---------|--------|-----------|
| `sha256_compress_sw()` | 33-66 | Compresión SHA-256 software |
| `sha256_blocks_sw()` | 69-89 | SHA-256 multi-bloque |
| `bswap32()` | 95-98 | Byte-swap LE <-> BE |
| `prepare_mining_params()` | 105-131 | Preparar midstate y tail desde header LE |
| `verify_block_hash_sw()` | 134-175 | Doble SHA-256 completo + verificación de ceros |

---

## 3. Test 1: Vector conocido NIST SHA-256

**Archivo:** `src/pico_miner_tb.cpp`, líneas 184-223

### Propósito

Validar que la implementación SHA-256 del golden model es correcta usando el
vector de prueba oficial del NIST (FIPS 180-4).

### Datos de prueba

```
Entrada:    "abc" (3 bytes)
Hash SHA-256 esperado: ba7816bf 8f01cfea 414140de 5dae2223
                       b00361a3 96177a9c b410ff61 f20015ad
```

### Método

1. Construir bloque de un solo mensaje: `0x61626380` ("abc" + padding `0x80`)
   con longitud `0x00000018` (24 bits)
2. Ejecutar `sha256_blocks_sw(msg, 1, hash)`
3. Comparar las 8 palabras del resultado con el valor esperado

### Importancia

Si este test falla, ninguna otra prueba puede ser confiable. Es el cimiento de
toda la verificación.

---

## 4. Test 2: Minería completa del Bloque 1 en software

**Archivo:** `src/pico_miner_tb.cpp`, líneas 225-319

### Propósito

Demostrar que el algoritmo de minería con optimización de midstate funciona
correctamente a través de una búsqueda completa desde nonce=0.

### Datos de prueba

- **Bloque**: Block 1 (primer bloque tras el genesis, 9 de enero de 2009)
- **Nonce conocido (BE)**: `0x01e36299` (~31.8M en decimal)
- **Header LE**: 20 palabras (líneas 243-250)

### Método

1. Preparar parámetros de minería (midstate, chunk2_tail) via `prepare_mining_params()`
2. Verificar el hash del bloque completo con `verify_block_hash_sw()`
3. Iterar desde nonce=0 hasta `known_nonce_be + 100`:
   - Construir chunk 2 (tail + nonce + padding)
   - Primera SHA-256: comprimir chunk 2 con midstate
   - Construir bloque para segunda SHA-256
   - Segunda SHA-256: hash del hash
   - Comprobar `final_hash[7] == 0x00000000`
4. Verificar que el nonce encontrado coincide con el conocido

### Tiempo de ejecución

~10 segundos en simulación C (csim). Las ~31.8M iteraciones se ejecutan
en la CPU del PC de laboratorio.

### Importancia

Valida la corrección del enfoque de midstate sobre un rango completo de
nonces reales. Es la prueba más exhaustiva del algoritmo.

---

## 5. Test 3: Bloque 1 en hardware (ventana estrecha)

**Archivo:** `src/pico_miner_tb.cpp`, líneas 384-402

### Propósito

Verificar que la función HLS `pico_miner()` produce el mismo resultado que
el golden model software para el Bloque 1.

### Método

Usa la función genérica `test_mine_block_hw()` (líneas 325-382):

1. Prepara parámetros de minería
2. Verifica el hash en software
3. Llama a `pico_miner()` con ventana estrecha: `[known_nonce_be - 16, known_nonce_be + 16)` = 32 nonces
4. Verifica status == MINING_FOUND y nonce correcto

### Ventana estrecha

La co-simulación RTL es precisa a nivel de ciclo y muy lenta. Con 32 nonces x
128 ciclos = ~4096 ciclos, la simulación es rápida. Una búsqueda completa de
31.8M nonces seria impracticable en cosim.

### Hash esperado

```
00000000839a8e6886ab5951d76f411475428afc90947ee320161bbf18eb6048
```

---

## 6. Test 4: Bloque 939260 en hardware (ventana estrecha)

**Archivo:** `src/pico_miner_tb.cpp`, líneas 404-429

### Propósito

Verificar el acelerador contra un bloque reciente de alta dificultad,
garantizando que no solo funciona con bloques triviales tempranos.

### Datos de prueba

- **Bloque**: Block 939260 (4 de marzo de 2026)
- **Dificultad**: 144,398,401,518,101
- **Nonce LE**: `0x1E740A08`, **BE**: `0x080A741E`
- **Header LE**: 20 palabras (líneas 417-424)

### Método

Idéntico al Test 3: ventana de +/-16 nonces alrededor del nonce conocido.

### Hash esperado

```
000000000000000000017588478b3612182486d33006ede1164fb146fa41cd06
```

### Importancia

Demuestra que el acelerador funciona correctamente con bloques modernos de
dificultad real, no solo con bloques históricos de dificultad mínima.

---

## 7. Test 5: Sin solución en rango

**Archivo:** `src/pico_miner_tb.cpp`, líneas 434-464

### Propósito

Verificar que el acelerador informa correctamente "no encontrado" cuando
no existe un nonce válido en el rango de búsqueda.

### Método

1. Usar un midstate arbitrario (valores iniciales SHA-256)
2. Usar chunk2_tail con valores de prueba: `{0x11111111, 0x22222222, 0x33333333}`
3. Buscar en rango [0, 100) con `target_hi = 0x00000000`
4. Verificar que `status == MINING_NOT_FOUND`

### Probabilidad de falso positivo

Con 100 nonces y target de 32 bits cero: `100 / 2^32 = 2.33 x 10^-8`.
Probabilidad esencialmente nula de encontrar un nonce por casualidad.

---

## 8. Co-simulación C/RTL

### 8.1 Qué es cosim

La co-simulación (`cosim_design` en el script TCL) ejecuta el RTL sintetizado
con las mismas entradas que la simulación C y compara las salidas. Esto verifica
que la traducción C-a-RTL no ha introducido errores.

### 8.2 Por qué las ventanas estrechas

La cosim es ~128 ciclos por nonce y la simulación RTL es órdenes de magnitud
más lenta que la simulación C. Las ventanas de 32 nonces mantienen la cosim
rápida mientras siguen verificando la corrección:

| Test | Nonces en cosim | Ciclos RTL |
|------|-----------------|------------|
| Test 3 (Block 1) | 32 | ~4,096 |
| Test 4 (Block 939260) | 32 | ~4,096 |
| Test 5 (sin solución) | 100 | ~12,800 |

### 8.3 Tests en cosim

Los tests 1 y 2 solo usan funciones software y no llaman a `pico_miner()`,
por lo que no generan actividad RTL. Los tests 3, 4 y 5 son los que realmente
ejercitan el RTL durante la cosim.

---

## 9. Verificación en placa (ARM)

**Archivo:** `src/pico_miner_arm.c`, líneas 401-458

### 9.1 Verificación cruzada

El driver ARM incluye su propia implementación SHA-256 software (`verify_nonce_sw`,
líneas 162-193). Cuando el FPGA encuentra un nonce:

1. El ARM recalcula el doble SHA-256 completo en software
2. Comprueba que `final_hash[7] == 0x00000000`
3. Imprime el hash calculado por SW junto al hash esperado
4. Compara el nonce encontrado con el nonce conocido del blockchain

### 9.2 Salida de verificación esperada

```
STEP 3: Verification
============================================================
  [HW] Found nonce (BE): 0x080A741E
  [SW] Verification:     VALID
  [SW] Double-SHA-256 hash (display order):
    000000000000000000017588478b3612182486d33006ede1164fb146fa41cd06
  [EXP] Expected hash:
    000000000000000000017588478b3612182486d33006ede1164fb146fa41cd06

  Nonce match:  HW=0x080A741E  Known=0x080A741E  [OK]
```

### 9.3 Indicadores fisicos

- **LEDs**: 8 LEDs ON = bloque minado correctamente
- **UART**: Mensajes de verificación detallados
- **Tiempo**: El tiempo total (~3 minutos) confirma el throughput esperado

---

## Resumen de la estrategia de verificación

```
                    +-----------------------+
                    |  NIST FIPS 180-4      |
                    |  (Test 1: SHA-256     |
                    |   known vector)       |
                    +-----------+-----------+
                                |
                    +-----------v-----------+
                    |  Golden Model SW      |
                    |  (sha256_compress_sw) |
                    +-----------+-----------+
                                |
              +-----------------+------------------+
              |                                    |
   +----------v-----------+          +-------------v-----------+
   | Test 2: Block 1      |          | Tests 3,4,5: HW calls  |
   | full-range SW mining |          | pico_miner() function   |
   | (~31.8M nonces)      |          | (narrow windows)        |
   +----------+-----------+          +-------------+-----------+
              |                                    |
              |                      +-------------v-----------+
              |                      | C/RTL Co-simulation     |
              |                      | (RTL vs C comparison)   |
              |                      +-------------+-----------+
              |                                    |
              +-------------------+----------------+
                                  |
                    +-------------v-----------+
                    | Verificación en placa   |
                    | ARM SW golden model     |
                    | compara con resultado   |
                    | del FPGA               |
                    +-------------------------+
```

---

## Archivos relevantes

| Archivo | Descripción |
|---------|-------------|
| `src/pico_miner_tb.cpp` | Testbench HLS con 5 tests (508 líneas) |
| `src/pico_miner_arm.c` | Driver ARM con verificación post-minería (466 líneas) |
| `run_hls.tcl` | Script que ejecuta csim y cosim automáticamente |
