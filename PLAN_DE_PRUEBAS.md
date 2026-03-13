# Plan de Pruebas — Lanchester-CIO

## 1. Requisitos del entorno

### Lo que necesitas instalar

| Herramienta | Para que sirve | Comando de instalacion (Ubuntu/Debian) |
|---|---|---|
| **g++** | Compilar el codigo C++ | `sudo apt install g++` |
| **make** | Ejecutar el Makefile (evita escribir el comando de compilacion a mano) | `sudo apt install make` |
| **python3** | El script de tests lo usa para extraer campos JSON | Ya viene instalado en la mayoria de distros |
| **bash** | Ejecutar el script de validacion | Ya viene instalado |

> **Nota para macOS**: sustituir `apt install` por `brew install gcc make python3` (requiere Homebrew).
> **Nota para Windows**: usar WSL2 (Windows Subsystem for Linux) con Ubuntu. Todo lo demas es igual.

### Verificar que todo esta instalado

```bash
g++ --version       # debe mostrar version >= 7 (necesita C++17)
make --version      # debe mostrar GNU Make
python3 --version   # debe mostrar Python 3.x
```

Si alguno falla, instalarlo con el comando de la tabla anterior.

---

## 2. Compilar el proyecto (sin tocar cmake)

El proyecto usa un **Makefile** sencillo, no CMake. Solo hay que ejecutar:

```bash
cd /ruta/donde/clonaste/lanchesterAPP
make
```

Esto ejecuta internamente:
```
g++ -std=c++17 -Wall -Wextra -Wpedantic -O2 -Iinclude -o lanchester main.cpp
```

Si la compilacion es exitosa, aparecera el ejecutable `lanchester` en la misma carpeta. Puedes verificarlo con:

```bash
ls -la lanchester
# Debe mostrar un archivo ejecutable (permisos -rwx...)
```

Si necesitas recompilar desde cero:
```bash
make clean   # borra el ejecutable anterior
make         # compila de nuevo
```

---

## 3. Ejecucion rapida: probar que funciona

Antes de correr los tests formales, verifica que el programa arranca:

```bash
./lanchester tests/test_01_symmetric.json
```

Debe imprimir un JSON con el resultado del combate. Si ves algo como:

```json
{
  "scenario_id": "TEST-01-SYMMETRIC",
  "combats": [{
    "combat_id": 1,
    "outcome": "DRAW",
    ...
  }]
}
```

...el programa funciona correctamente.

---

## 4. Ejecutar la suite de pruebas completa

El proyecto ya incluye 8 tests y un script que los ejecuta todos de golpe:

```bash
cd /ruta/donde/clonaste/lanchesterAPP
bash tests/run_validation.sh
```

El script:
1. Ejecuta cada uno de los 8 escenarios JSON contra el binario `./lanchester`
2. Extrae campos del JSON de salida (usando python3)
3. Verifica invariantes (condiciones que siempre deben cumplirse)
4. Compara modos pre-tasa vs post-tasa
5. Muestra un resumen con tests pasados/fallidos en colores (verde = OK, rojo = FAIL)

### Salida esperada (todo correcto)

```
=============================================
 Validacion del Modelo Lanchester-CIO
=============================================

TEST-01: Combate simetrico (T-80U vs T-80U, 10 vs 10)
  [OK] Outcome es DRAW
  [OK] Static advantage = 1.0 (simetrico)
  [OK] Fuerzas iniciales iguales (10 vs 10)
  [OK] Fuerzas iniciales = 10

TEST-02: Fuerza abrumadora (20 LEO2E vs 2 T-80U)
  [OK] Outcome es BLUE_WINS
  [OK] Supervivientes azul > 18 (bajas minimas)
  [OK] Fuerzas iniciales azul = 20

...

 Resultado: XX pasados, 0 fallidos de XX
=============================================
```

Si algun test muestra `[FAIL]`, significa que el comportamiento del modelo ha cambiado respecto a lo esperado.

---

## 5. Que verifica cada test

| Test | Archivo | Que prueba | Resultado esperado |
|---|---|---|---|
| TEST-01 | `test_01_symmetric.json` | 10 T-80U vs 10 T-80U, mismo estado tactico | DRAW, ventaja estatica = 1.0 |
| TEST-02 | `test_02_overwhelming.json` | 20 LEOPARDO_2E vs 2 T-80U | BLUE_WINS, supervivientes azul > 18 |
| TEST-03 | `test_03_no_cc.json` | VEC_25 vs BTR-82A (ningun vehiculo tiene C/C) | Municion C/C consumida = 0 en ambos bandos |
| TEST-04 | `test_04_out_of_range.json` | Distancia 9000m (fuera del alcance de todos) | INDETERMINATE, 0 bajas en ambos bandos |
| TEST-05 | `test_05_aft_casualties.json` | 50% bajas pre-contacto (AFT) en azul | Azul inicia con 5.0 (10 * 0.5), rojo con 10.0 |
| TEST-06 | `test_06_defense_mult.json` | Multiplicador defensivo (posicion preparada) | BLUE_WINS pese a inferioridad numerica |
| TEST-07 | `test_07_mixed_forces.json` | Fuerzas mixtas LEO2E+PIZARRO vs T-80U+BMP-3 | 12 azul, 16 rojo iniciales; C/C consumida > 0 |
| TEST-08 | `test_08_engagement_fraction.json` | engagement_fraction=0.5, count_factor=2.0 | Azul y rojo inician con 10.0 (10*0.5*2.0) |

### Invariantes verificados en TODOS los tests

Para cada combate en cada test, se verifica:
- `bajas = inicial - supervivientes` (conservacion de fuerzas)
- `supervivientes >= 0` (no hay efectivos negativos)

---

## 6. Como crear un nuevo test

### Paso 1: Crear el archivo JSON

Copia un test existente como plantilla:

```bash
cp tests/test_01_symmetric.json tests/test_09_mi_prueba.json
```

### Paso 2: Editar el escenario

Abre `tests/test_09_mi_prueba.json` con cualquier editor y modifica los campos:

```json
{
  "scenario_id": "TEST-09-MI-PRUEBA",
  "description": "Descripcion de lo que quieres probar",
  "terrain": "MEDIO",
  "engagement_distance_m": 2000,
  "solver": {"h": 0.001666, "t_max_minutes": 30.0},
  "combat_sequence": [{
    "combat_id": 1,
    "blue": {
      "tactical_state": "Ataque a posicion defensiva",
      "mobility": "ALTA",
      "aft_received": 0,
      "aft_casualties_pct": 0.0,
      "engagement_fraction": 1.0,
      "rate_factor": 1.0,
      "count_factor": 1.0,
      "composition": [
        {"vehicle": "LEOPARDO_2E", "count": 5},
        {"vehicle": "PIZARRO", "count": 10}
      ]
    },
    "red": {
      "tactical_state": "Ataque a posicion defensiva",
      "mobility": "ALTA",
      "aft_received": 0,
      "aft_casualties_pct": 0.0,
      "engagement_fraction": 1.0,
      "rate_factor": 1.0,
      "count_factor": 1.0,
      "composition": [
        {"vehicle": "T-80U", "count": 8}
      ]
    },
    "reinforcements_blue": [],
    "reinforcements_red": []
  }]
}
```

### Campos que puedes modificar

| Campo | Valores validos | Efecto |
|---|---|---|
| `terrain` | `"LLANO"`, `"MEDIO"`, `"BOSCOSO"` | Modifica factor de terreno |
| `engagement_distance_m` | Entero positivo (metros) | Distancia de combate |
| `tactical_state` | `"Ataque a posicion defensiva"`, `"En posicion de tiro"`, `"En movimiento"` | Multiplicador tactico |
| `mobility` | `"ALTA"`, `"MEDIA"`, `"BAJA"` | Factor de movilidad |
| `aft_received` | `0` o `1` | Si recibio fuego AFT pre-contacto |
| `aft_casualties_pct` | `0.0` a `1.0` | Porcentaje de bajas pre-contacto |
| `engagement_fraction` | `0.0` a `1.0` | Fraccion de la fuerza que participa |
| `rate_factor` | Positivo | Multiplicador de tasa de fuego |
| `count_factor` | Positivo | Multiplicador de efectivos |
| `solver.h` | Positivo pequeno | Paso temporal del integrador Euler (0.001666 = ~0.1s) |
| `solver.t_max_minutes` | Positivo | Duracion maxima del combate |

### Vehiculos disponibles

**Base de datos `vehicle_db.json`** (bando azul tipicamente):
- `TOA_SPIKE_I` — TOA con misiles Spike (tiene C/C)
- `VEC_25` — VEC 8x8 con canon 25mm (sin C/C)
- `LEOPARDO_2E` — Carro de combate Leopard 2E (sin C/C)
- `PIZARRO` — VCBR Pizarro (tiene C/C)

**Base de datos `vehicle_db_en.json`** (bando rojo tipicamente):
- `T-80U` — Carro de combate T-80U (sin C/C)
- `BMP-3` — BMP-3 (tiene C/C)
- `T-72B3` — Carro de combate T-72B3 (sin C/C)
- `BTR-82A` — BTR-82A (sin C/C)

### Paso 3: Ejecutar tu test manualmente

```bash
./lanchester tests/test_09_mi_prueba.json
```

Revisa la salida JSON y verifica que los resultados son coherentes.

### Paso 4: Ejecutar con diferente modo de agregacion

```bash
./lanchester tests/test_09_mi_prueba.json --aggregation pre
./lanchester tests/test_09_mi_prueba.json --aggregation post
```

Compara los resultados entre ambos modos para fuerzas mixtas.

---

## 7. Tests adicionales recomendados (para ampliar cobertura)

A continuacion se proponen escenarios que **no estan cubiertos** por los 8 tests actuales:

### 7.1 Combate encadenado (chaining)

Verifica que los supervivientes del combate 1 pasan como efectivos al combate 2.

```json
{
  "scenario_id": "TEST-09-CHAIN",
  "description": "Cadena de 2 combates: supervivientes del 1 luchan en el 2",
  "terrain": "MEDIO",
  "engagement_distance_m": 2000,
  "solver": {"h": 0.001666, "t_max_minutes": 30.0},
  "combat_sequence": [
    {
      "combat_id": 1,
      "blue": {
        "tactical_state": "Ataque a posicion defensiva",
        "mobility": "ALTA",
        "aft_received": 0, "aft_casualties_pct": 0.0,
        "engagement_fraction": 1.0, "rate_factor": 1.0, "count_factor": 1.0,
        "composition": [{"vehicle": "LEOPARDO_2E", "count": 10}]
      },
      "red": {
        "tactical_state": "Ataque a posicion defensiva",
        "mobility": "ALTA",
        "aft_received": 0, "aft_casualties_pct": 0.0,
        "engagement_fraction": 1.0, "rate_factor": 1.0, "count_factor": 1.0,
        "composition": [{"vehicle": "T-80U", "count": 5}]
      },
      "reinforcements_blue": [], "reinforcements_red": []
    },
    {
      "combat_id": 2,
      "blue": {
        "tactical_state": "En posicion de tiro",
        "mobility": "ALTA",
        "aft_received": 0, "aft_casualties_pct": 0.0,
        "engagement_fraction": 1.0, "rate_factor": 1.0, "count_factor": 1.0,
        "composition": [{"vehicle": "LEOPARDO_2E", "count": 10}]
      },
      "red": {
        "tactical_state": "Ataque a posicion defensiva",
        "mobility": "ALTA",
        "aft_received": 0, "aft_casualties_pct": 0.0,
        "engagement_fraction": 1.0, "rate_factor": 1.0, "count_factor": 1.0,
        "composition": [{"vehicle": "T-72B3", "count": 8}]
      },
      "reinforcements_blue": [], "reinforcements_red": []
    }
  ]
}
```

**Que verificar:**
- `combats[0].outcome` = `BLUE_WINS`
- `combats[1].blue_initial` < 10 (azul llega mermado del combate 1)
- Invariantes de conservacion se cumplen en ambos combates

### 7.2 Terrenos extremos

```bash
# Crear variantes del test 01 cambiando solo el terreno:
cp tests/test_01_symmetric.json tests/test_10_llano.json
# Editar terrain a "LLANO"

cp tests/test_01_symmetric.json tests/test_11_boscoso.json
# Editar terrain a "BOSCOSO"
```

**Que verificar:** El combate simetrico sigue siendo DRAW independientemente del terreno (ambos bandos se ven afectados por igual).

### 7.3 Distancia limite (justo en el alcance maximo)

Probar con `engagement_distance_m` = 4000 (alcance maximo del T-80U y LEOPARDO_2E). Verificar que el combate ocurre y no da INDETERMINATE.

### 7.4 Modo batch

```bash
mkdir -p /tmp/batch_test
cp tests/test_01_symmetric.json tests/test_02_overwhelming.json /tmp/batch_test/
./lanchester --batch /tmp/batch_test --output /tmp/batch_result.csv
cat /tmp/batch_result.csv
```

**Que verificar:** Se genera un CSV con una fila por cada escenario.

### 7.5 Modo sweep

```bash
./lanchester --sweep blue_count --sweep-min 5 --sweep-max 20 --sweep-step 1 \
    --scenario tests/test_01_symmetric.json --output /tmp/sweep_result.csv
cat /tmp/sweep_result.csv
```

**Que verificar:** CSV con filas para blue_count = 5, 6, 7, ..., 20.

---

## 8. Resumen rapido: comandos paso a paso

```bash
# 1. Instalar dependencias (solo la primera vez)
sudo apt install g++ make python3

# 2. Clonar o ir al directorio del proyecto
cd /ruta/a/lanchesterAPP

# 3. Compilar
make

# 4. Verificacion rapida
./lanchester tests/test_01_symmetric.json

# 5. Ejecutar todos los tests
bash tests/run_validation.sh

# 6. (Opcional) Probar modos adicionales
./lanchester tests/test_07_mixed_forces.json --aggregation pre
./lanchester tests/test_07_mixed_forces.json --aggregation post
./lanchester --batch tests --output /tmp/batch.csv
```

Si todos los tests pasan con 0 fallos, la aplicacion funciona correctamente.

---

## 9. Problemas frecuentes

| Problema | Causa probable | Solucion |
|---|---|---|
| `g++: command not found` | No tienes compilador C++ | `sudo apt install g++` |
| `make: command not found` | No tienes make | `sudo apt install make` |
| `error: filesystem not found` | g++ version < 8 | Actualizar: `sudo apt install g++-11` |
| `Vehicle 'X' not found` | Typo en nombre de vehiculo en el JSON | Revisar nombres exactos en seccion 6.3 |
| `Permission denied: ./lanchester` | Falta permiso de ejecucion | `chmod +x lanchester` |
| `python3: command not found` | No tienes Python 3 | `sudo apt install python3` |
| Tests pasan localmente pero fallan en otro PC | Diferencias de precision flotante | Tolerancia del script es 0.01-0.02, deberia ser suficiente |
