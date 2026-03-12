# ARM -- Driver Bare-Metal e Integracion PS/PL

Documento tecnico que detalla el driver ARM Cortex-A9 ejecutado en el
Processing System (PS) del Zynq-7020 y su comunicacion con la IP de mineria
en la Programmable Logic (PL).

---

## Indice

1. [Arquitectura PS/PL en Zynq](#1-arquitectura-pspl-en-zynq)
2. [Inicializacion del sistema](#2-inicializacion-del-sistema)
3. [Comunicacion ARM-FPGA via AXI-Lite](#3-comunicacion-arm-fpga-via-axi-lite)
4. [Estrategia de mineria por chunks](#4-estrategia-de-mineria-por-chunks)
5. [Control de LEDs via AXI GPIO](#5-control-de-leds-via-axi-gpio)
6. [Medicion de tiempo con XTime](#6-medicion-de-tiempo-con-xtime)
7. [Verificacion software post-mineria](#7-verificacion-software-post-mineria)
8. [Flujo completo del programa](#8-flujo-completo-del-programa)

---

## 1. Arquitectura PS/PL en Zynq

El Zynq-7020 integra dos dominios:

- **PS (Processing System)**: Dual-core ARM Cortex-A9 a 667 MHz con caches L1/L2,
  controlador DDR3, UART, y bus AXI maestro.
- **PL (Programmable Logic)**: Fabric FPGA con la IP sintetizada por Vivado HLS
  conectada como esclavo AXI-Lite.

El ARM (PS) actua como **maestro**: escribe parametros de mineria en los
registros de la IP, la arranca, espera a que termine, y lee los resultados.

---

## 2. Inicializacion del sistema

**Archivo:** `src/pico_miner_arm.c`, lineas 199-249

### 2.1 Caches

```c
Xil_ICacheEnable();
Xil_DCacheEnable();
```

Se habilitan las caches L1 de instrucciones y datos del ARM para mejorar el
rendimiento de la ejecucion del driver. Se usa `xil_cache.h` en lugar de
`platform.h` (el proyecto usa la plantilla Empty Application del SDK).

Al finalizar el programa:
```c
Xil_DCacheDisable();
Xil_ICacheDisable();
```

### 2.2 GPIO para LEDs

**Archivo:** `src/pico_miner_arm.c`, lineas 229-235

```c
rc = XGpio_Initialize(&gpio_leds, XPAR_AXI_GPIO_0_DEVICE_ID);
XGpio_SetDataDirection(&gpio_leds, LED_CHANNEL, 0x00);  // todo salida
XGpio_DiscreteWrite(&gpio_leds, LED_CHANNEL, 0x00);     // todo apagado
```

- El bloque AXI GPIO esta conectado a los 8 LEDs del ZedBoard
- Canal unico (channel 1), 8 bits de salida
- `XPAR_AXI_GPIO_0_DEVICE_ID` viene de `xparameters.h` (auto-generado por Vivado)

### 2.3 IP HLS

**Archivo:** `src/pico_miner_arm.c`, lineas 237-249

```c
miner_cfg = XPico_miner_LookupConfig(XPAR_PICO_MINER_0_DEVICE_ID);
rc = XPico_miner_CfgInitialize(&miner, miner_cfg);
```

Las funciones `XPico_miner_*` son **auto-generadas** por Vivado HLS al exportar
la IP. El fichero `xpico_miner.h` contiene la estructura `XPico_miner` y todas
las funciones Set/Get/Start/IsDone.

---

## 3. Comunicacion ARM-FPGA via AXI-Lite

### 3.1 Escritura de parametros

**Archivo:** `src/pico_miner_arm.c`, lineas 331-347

```c
// Midstate: 8 registros individuales
XPico_miner_Set_midstate_0(&miner, midstate[0]);
XPico_miner_Set_midstate_1(&miner, midstate[1]);
// ... hasta midstate_7

// Chunk2 tail: 3 registros
XPico_miner_Set_chunk2_tail_0(&miner, chunk2_tail[0]);
XPico_miner_Set_chunk2_tail_1(&miner, chunk2_tail[1]);
XPico_miner_Set_chunk2_tail_2(&miner, chunk2_tail[2]);

// Rango de busqueda y target
XPico_miner_Set_nonce_start(&miner, nonce_start);
XPico_miner_Set_nonce_end(&miner, nonce_end);
XPico_miner_Set_target_hi(&miner, 0x00000000u);
```

Cada funcion `Set_*` es una escritura de 32 bits a un offset especifico del
espacio de direcciones AXI-Lite de la IP. Vivado HLS 2019.1 mapea arrays como
registros escalares (no como bloques de memoria).

### 3.2 Inicio y espera

**Archivo:** `src/pico_miner_arm.c`, lineas 349-355

```c
XPico_miner_Start(&miner);

while (!XPico_miner_IsDone(&miner)) {
    /* busy wait */
}
```

- `Start`: Escribe 1 en el bit `ap_start` del registro de control
- `IsDone`: Lee el bit `ap_done` del registro de control

Se usa **polling** (espera activa) en lugar de interrupciones. Para un chunk de
1M nonces (~1.3 segundos), la espera activa es aceptable y simplifica el driver.

### 3.3 Lectura de resultados

**Archivo:** `src/pico_miner_arm.c`, lineas 357-368

```c
hw_status = XPico_miner_Get_status(&miner);
// ...
found_nonce_hw = XPico_miner_Get_found_nonce(&miner);
```

Cada funcion `Get_*` lee un registro de 32 bits del espacio AXI-Lite.

---

## 4. Estrategia de mineria por chunks

**Archivo:** `src/pico_miner_arm.c`, lineas 311-390

### 4.1 Motivacion

El espacio total de busqueda para el Bloque 939260 es ~134.9M nonces. Si se
enviara todo el rango en una unica invocacion FPGA, el ARM no podria mostrar
progreso ni actualizar los LEDs durante ~3 minutos.

La solucion es dividir el espacio en **chunks de 1,000,000 nonces**:

```c
#define CHUNK_SIZE  1000000u
```

A ~781 KH/s, cada chunk tarda ~1.3 segundos. Esto proporciona una tasa de
actualizacion adecuada.

### 4.2 Bucle principal

```c
while (nonce_start < total_target && !block_mined) {
    unsigned int nonce_end = nonce_start + CHUNK_SIZE;

    // 1. Actualizar LEDs
    // 2. Escribir parametros a FPGA
    // 3. Start + Wait
    // 4. Leer resultados
    // 5. Calcular tiempo y hash rate
    // 6. Imprimir progreso

    nonce_start = nonce_end;
}
```

Entre chunks, el ARM tiene oportunidad de:
- Calcular y mostrar porcentaje de progreso
- Actualizar la barra de LEDs
- Medir el tiempo transcurrido y calcular hash rate
- Imprimir linea de progreso por UART

### 4.3 Salida de progreso por UART

**Archivo:** `src/pico_miner_arm.c`, lineas 381-386

Cada chunk que no encuentra solucion imprime:

```
[MINING] nonces:   15000000 / 135869022  |   11%  |  19.2s  |  781 KH/s  |  range [0x00E4E1C0..0x00F9CBA0)
```

Cuando se encuentra el nonce:

```
>> NONCE FOUND! <<
Found nonce:    0x080A741E (BE)  0x1E740A08 (LE)
Total searched: 134000000 nonces
Elapsed time:   171.5 seconds
Hash rate:      781234 H/s (781.2 KH/s)
```

---

## 5. Control de LEDs via AXI GPIO

**Archivo:** `src/pico_miner_arm.c`, lineas 302-309 y 324-329

### 5.1 Animacion de inicio

```c
XGpio_DiscreteWrite(&gpio_leds, LED_CHANNEL, 0xFF);  // todos ON
// delay
XGpio_DiscreteWrite(&gpio_leds, LED_CHANNEL, 0x00);  // todos OFF
// delay (x2 para doble parpadeo)
```

Doble parpadeo de los 8 LEDs para indicar que la mineria va a comenzar.

### 5.2 Barra de progreso

```c
unsigned int progress_pct = (unsigned long long)nonce_start * 100ULL / total_target;
unsigned int leds_on = (progress_pct * 8) / 100;
unsigned int led_pattern = (1u << leds_on) - 1;
XGpio_DiscreteWrite(&gpio_leds, LED_CHANNEL, led_pattern);
```

| Progreso | LEDs encendidos | Patron binario |
|----------|-----------------|----------------|
| 0% | 0 | `00000000` |
| 12% | 1 | `00000001` |
| 25% | 2 | `00000011` |
| 37% | 3 | `00000111` |
| 50% | 4 | `00001111` |
| 62% | 5 | `00011111` |
| 75% | 6 | `00111111` |
| 87% | 7 | `01111111` |
| 100% | 8 | `11111111` |

### 5.3 Resultado final

```c
if (block_mined)
    XGpio_DiscreteWrite(&gpio_leds, LED_CHANNEL, 0xFF);  // todos ON
else
    XGpio_DiscreteWrite(&gpio_leds, LED_CHANNEL, 0x00);  // todos OFF
```

---

## 6. Medicion de tiempo con XTime

**Archivo:** `src/pico_miner_arm.c`, lineas 206, 318, 362-365

```c
#include "xtime_l.h"

XTime t_start, t_now;
XTime_GetTime(&t_start);
// ... mineria ...
XTime_GetTime(&t_now);
double elapsed_s = (double)(t_now - t_start) / (double)COUNTS_PER_SECOND;
double hash_rate = (double)total_searched / elapsed_s;
```

- `XTime_GetTime`: Lee el contador global de 64 bits del ARM (registro de
  ciclos)
- `COUNTS_PER_SECOND`: Constante definida en `xtime_l.h`, equivale a la mitad
  de la frecuencia del CPU (el contador incrementa cada 2 ciclos de CPU)
- Precision suficiente para medir intervalos de segundos

---

## 7. Verificacion software post-mineria

**Archivo:** `src/pico_miner_arm.c`, lineas 162-193 y 414-427

### 7.1 Golden model en ARM

El driver incluye su propia implementacion software de SHA-256 (funciones
`sha256_compress_sw` y `verify_nonce_sw`), identica algoritmicamente a la
version HLS pero sin pragmas.

### 7.2 Flujo de verificacion

Cuando el FPGA encuentra un nonce:

1. El ARM llama a `verify_nonce_sw(midstate, chunk2_tail, found_nonce_hw, sw_hash)`
2. La funcion calcula el doble SHA-256 completo en software
3. Comprueba que `final_hash[7] == 0x00000000`
4. El ARM imprime el hash calculado por software
5. Compara con el hash conocido del Bloque 939260

Esto proporciona una **verificacion cruzada independiente**: el ARM confirma que
el resultado del FPGA es correcto usando una implementacion completamente
separada.

---

## 8. Flujo completo del programa

**Archivo:** `src/pico_miner_arm.c` (466 lineas)

```
1. Inicializacion
   - Habilitar caches (Xil_ICacheEnable, Xil_DCacheEnable)
   - Imprimir banner con informacion del bloque
   - Inicializar GPIO (8 LEDs como salida)
   - Inicializar IP HLS (LookupConfig + CfgInitialize)

2. Preparacion (STEP 1)
   - Byte-swap del header LE -> BE (20 palabras)
   - Extraer chunk 1 (palabras 0-15)
   - Calcular midstate en ARM: SHA-256(H_init, chunk1)
   - Extraer chunk2_tail (palabras 16-18) y nonce conocido (palabra 19)
   - Imprimir midstate, tail, nonce para debug

3. Mineria (STEP 2)
   - Animacion de inicio (doble parpadeo LEDs)
   - Iniciar cronometro (XTime)
   - Bucle de chunks:
     a. Actualizar barra de LEDs
     b. Escribir 14 registros a la FPGA via AXI-Lite
     c. Start FPGA
     d. Poll IsDone
     e. Leer status (y found_nonce si encontrado)
     f. Calcular tiempo y hash rate
     g. Imprimir progreso o resultado

4. Verificacion (STEP 3)
   - Todos LEDs ON si minado, OFF si no
   - Verificar nonce con SHA-256 software
   - Imprimir hash calculado vs esperado
   - Comparar nonce encontrado vs conocido

5. Resumen final
   - Imprimir banner de MINING COMPLETE
   - Deshabilitar caches
```

---

## Archivos relevantes

| Archivo | Descripcion |
|---------|-------------|
| `src/pico_miner_arm.c` | Driver ARM completo para mineria del Bloque 939260 (466 lineas) |
| `src/pico_miner.h` | Cabecera compartida con el HLS (constantes, prototipo) |
| `xpico_miner.h` | Driver auto-generado por Vivado HLS (en el workspace del SDK) |
| `xgpio.h` | Driver Xilinx para AXI GPIO |
| `xtime_l.h` | Funciones de medicion de tiempo del ARM |
| `xil_cache.h` | Control de caches L1 del ARM |
