# Plan de Pruebas — Lanchester-CIO

Guia paso a paso para compilar, ejecutar y probar el simulador de combate Lanchester.
Entorno recomendado: **VS Code + Remote SSH** conectado a un servidor Linux (Ubuntu).

---

## PARTE 1: Preparar el entorno de trabajo

### Paso 1.1 — Instalar VS Code en tu PC

1. Ve a https://code.visualstudio.com/ y descarga el instalador para tu sistema (Windows/Mac)
2. Instala normalmente (siguiente, siguiente, finalizar)
3. Abre VS Code

### Paso 1.2 — Instalar la extension Remote SSH

1. En VS Code, pulsa `Ctrl+Shift+X` (abre el panel de extensiones)
2. Escribe `Remote - SSH` en el buscador
3. Busca la que dice **"Remote - SSH"** de Microsoft
4. Pulsa **Install**

### Paso 1.3 — Conectarte al servidor Linux

1. En VS Code, pulsa `Ctrl+Shift+P` (abre la paleta de comandos)
2. Escribe `Remote-SSH: Connect to Host...` y seleccionalo
3. Escribe la direccion de tu servidor: `tu_usuario@ip_del_servidor`
4. Te pedira la contrasena (o usara tu clave SSH si la tienes configurada)
5. Espera a que se conecte (la primera vez tarda un poco porque instala el servidor de VS Code en remoto)

### Paso 1.4 — Abrir la carpeta del proyecto

1. Ya conectado al servidor, pulsa `Ctrl+Shift+E` (explorador de archivos)
2. Pulsa **"Open Folder"**
3. Navega hasta la carpeta del proyecto, por ejemplo `/home/tu_usuario/lanchesterAPP`
4. Pulsa **OK**

### Paso 1.5 — Instalar la extension C/C++ en el servidor

1. VS Code te sugerira instalar extensiones recomendadas. Si no:
2. Pulsa `Ctrl+Shift+X`, busca **"C/C++"** de Microsoft
3. Pulsa **Install in SSH: servidor** (se instala en el servidor remoto, no en tu PC)

Esto te da autocompletado, resaltado de sintaxis y navegacion por el codigo.

### Paso 1.6 — Abrir un terminal

1. Pulsa `` Ctrl+` `` (la tecla debajo del Escape) o ve a **Terminal > New Terminal**
2. Se abre un terminal Linux dentro de VS Code, conectado al servidor
3. **Todos los comandos de esta guia se ejecutan en este terminal**

---

## PARTE 2: Instalar dependencias en el servidor

Ejecuta estos comandos en el terminal de VS Code (uno por uno):

```bash
sudo apt update
```
```bash
sudo apt install -y g++ make python3
```

Verifica que todo se instalo correctamente:

```bash
g++ --version
```
Debe mostrar algo como `g++ (Ubuntu 11.4.0...) 11.4.0`. La version debe ser **7 o superior**.

```bash
make --version
```
Debe mostrar `GNU Make 4.x`.

```bash
python3 --version
```
Debe mostrar `Python 3.x`.

Si alguno de estos comandos dice "command not found", repite el `sudo apt install` correspondiente.

---

## PARTE 3: Compilar el proyecto

### Paso 3.1 — Situarte en la carpeta del proyecto

```bash
cd /home/tu_usuario/lanchesterAPP
```
(Sustituye `tu_usuario` por tu nombre de usuario real.)

### Paso 3.2 — Compilar

```bash
make
```

Este comando lee el archivo `Makefile` del proyecto y ejecuta automaticamente:
```
g++ -std=c++17 -Wall -Wextra -Wpedantic -O2 -Iinclude -o lanchester main.cpp
```

No necesitas escribir eso a mano; `make` lo hace por ti.

- Si **no hay errores**: se crea el archivo ejecutable `lanchester` en la carpeta
- Si **hay errores**: aparecen mensajes en rojo/blanco indicando la linea del problema

### Paso 3.3 — Verificar que se compilo

```bash
ls -la lanchester
```

Debe mostrar algo como:
```
-rwxr-xr-x 1 usuario grupo 283360 mar 13 12:00 lanchester
```

La `x` en los permisos significa que es ejecutable. Si no aparece, ejecuta:
```bash
chmod +x lanchester
```

### Paso 3.4 — Si necesitas recompilar

Cuando modifiques `main.cpp` y quieras volver a compilar:
```bash
make clean
make
```
`make clean` borra el ejecutable anterior. `make` compila de nuevo.

---

## PARTE 4: Verificacion rapida

Antes de ejecutar todos los tests, comprueba que el programa funciona con un caso sencillo.

### Paso 4.1 — Ejecutar un escenario de prueba

```bash
./lanchester tests/test_01_symmetric.json
```

### Paso 4.2 — Que deberias ver

Un bloque de JSON impreso en el terminal, parecido a esto:

```json
{
  "scenario_id": "TEST-01-SYMMETRIC",
  "combats": [
    {
      "combat_id": 1,
      "outcome": "DRAW",
      "blue_initial": 10.0,
      "red_initial": 10.0,
      "blue_survivors": 0.0,
      "red_survivors": 0.0,
      "blue_casualties": 10.0,
      "red_casualties": 10.0,
      "static_advantage": 1.0
    }
  ]
}
```

Si ves esto (o algo similar), el programa funciona. Puedes pasar a la parte 5.

### Si algo falla

| Lo que ves | Que hacer |
|---|---|
| `bash: ./lanchester: No such file or directory` | No compilaste. Vuelve al paso 3.2 |
| `bash: ./lanchester: Permission denied` | Ejecuta `chmod +x lanchester` |
| `Vehicle 'XXX' not found in any database` | El JSON de prueba tiene un nombre de vehiculo incorrecto |

---

## PARTE 5: Ejecutar la suite de pruebas completa

### Paso 5.1 — Lanzar todos los tests

```bash
bash tests/run_validation.sh
```

Este script ejecuta automaticamente los **9 escenarios de prueba** incluidos en el proyecto y verifica que los resultados son correctos.

### Paso 5.2 — Interpretar la salida

Veras algo como esto (resumido):

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

TEST-03: Sin capacidad C/C (VEC-25 vs BTR-82A)
  [OK] Municion C/C azul = 0
  [OK] Municion C/C roja = 0

...mas tests...

INVARIANTES: Verificacion en todos los tests
  [OK] test_01_symmetric[c0]: bajas_azul = inicial - supervivientes
  [OK] test_01_symmetric[c0]: bajas_rojo = inicial - supervivientes
  [OK] test_01_symmetric[c0]: supervivientes >= 0
  ...

=============================================
 Resultado: XX pasados, 0 fallidos de XX
=============================================
```

- **[OK] en verde**: el test paso correctamente
- **[FAIL] en rojo**: algo no cumple lo esperado — hay un bug o el modelo cambio

**Si todos dicen [OK] y hay 0 fallidos, todo funciona correctamente.**

### Paso 5.3 — Comparacion pre-tasa vs post-tasa

El script tambien muestra una tabla comparando los dos modos de agregacion. Esto es informativo — no hay "bien" ni "mal", solo muestra las diferencias entre ambos metodos para cada test.

---

## PARTE 6: Que verifica cada test (referencia)

| # | Archivo | Escenario | Que se comprueba |
|---|---|---|---|
| 01 | `test_01_symmetric.json` | 10 T-80U vs 10 T-80U, mismo bando | Resultado DRAW, ventaja estatica = 1.0 |
| 02 | `test_02_overwhelming.json` | 20 LEOPARDO_2E vs 2 T-80U | BLUE_WINS, supervivientes azul > 18 |
| 03 | `test_03_no_cc.json` | VEC_25 vs BTR-82A | Municion C/C = 0 (ningun vehiculo tiene C/C) |
| 04 | `test_04_out_of_range.json` | Combate a 9000m | INDETERMINATE, 0 bajas (nadie alcanza al otro) |
| 05 | `test_05_aft_casualties.json` | 50% bajas pre-contacto | Azul inicia con 5 (10 * 0.5) |
| 06 | `test_06_defense_mult.json` | Defensor en posicion preparada | Defensor gana pese a inferioridad numerica |
| 07 | `test_07_mixed_forces.json` | LEO2E+PIZARRO vs T-80U+BMP-3 | 12 vs 16 iniciales; C/C consumida > 0 |
| 08 | `test_08_engagement_fraction.json` | engagement_fraction=0.5, count_factor=2.0 | Efectivos = 10*0.5*2.0 = 10 |
| 09 | `test_09_analytical.json` | 15 LEOPARDO_2E vs 10 T-80U | Verificacion contra solucion analitica de Lanchester |

**Invariantes que se verifican en TODOS los tests:**
- `bajas = inicial - supervivientes` (las cuentas cuadran)
- `supervivientes >= 0` (no hay efectivos negativos)

---

## PARTE 7: Como crear un test nuevo

### Paso 7.1 — Copiar un test existente como plantilla

```bash
cp tests/test_01_symmetric.json tests/test_09_mi_prueba.json
```

### Paso 7.2 — Abrir y editar el archivo

En VS Code, en el explorador de archivos de la izquierda, navega a `tests/` y haz clic en `test_09_mi_prueba.json`. Se abre en el editor.

Modifica los campos que quieras. Aqui tienes la estructura completa con comentarios:

```json
{
  "scenario_id": "TEST-09-MI-PRUEBA",
  "description": "Describe aqui lo que quieres probar",
  "terrain": "MEDIO",
  "engagement_distance_m": 2000,
  "solver": {
    "h": 0.001666,
    "t_max_minutes": 30.0
  },
  "combat_sequence": [
    {
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
    }
  ]
}
```

### Paso 7.3 — Referencia rapida de campos

| Campo | Que es | Valores posibles |
|---|---|---|
| `terrain` | Tipo de terreno | `"FACIL"`, `"MEDIO"`, `"DIFICIL"` |
| `engagement_distance_m` | Distancia del combate en metros | Cualquier numero positivo |
| `tactical_state` | Estado tactico de la unidad | `"Ataque a posicion defensiva"`, `"Busqueda del contacto"`, `"En posicion de tiro"`, `"Defensiva condiciones minimas"`, `"Defensiva organizacion ligera"`, `"Defensiva organizacion media"`, `"Retardo"`, `"Retrocede"` |
| `mobility` | Movilidad de la unidad | `"MUY_ALTA"`, `"ALTA"`, `"MEDIA"`, `"BAJA"` |
| `aft_received` | Si sufrio fuego antes del combate | `0` (no) o `1` (si) |
| `aft_casualties_pct` | % de bajas pre-combate | `0.0` a `1.0` (ej: 0.3 = 30%) |
| `engagement_fraction` | Fraccion de fuerza que participa | `0.0` a `1.0` |
| `rate_factor` | Multiplicador de cadencia de tiro | Numero positivo (1.0 = normal) |
| `count_factor` | Multiplicador de efectivos | Numero positivo (1.0 = normal) |
| `solver.h` | Paso temporal del simulador | Numero pequeno (0.001666 funciona bien) |
| `solver.t_max_minutes` | Duracion maxima del combate | Minutos (30 o 60 son valores tipicos) |

### Paso 7.4 — Vehiculos disponibles

**Bando azul** (base de datos `vehicle_db.json`):

| Nombre en JSON | Vehiculo real | Tiene C/C |
|---|---|---|
| `LEOPARDO_2E` | Carro de combate Leopard 2E | No |
| `PIZARRO` | VCBR Pizarro | Si |
| `TOA_SPIKE_I` | TOA con misiles Spike | Si |
| `VEC_25` | VEC 8x8 con canon 25mm | No |

**Bando rojo** (base de datos `vehicle_db_en.json`):

| Nombre en JSON | Vehiculo real | Tiene C/C |
|---|---|---|
| `T-80U` | Carro de combate T-80U | No |
| `T-72B3` | Carro de combate T-72B3 | No |
| `BMP-3` | BMP-3 | Si |
| `BTR-82A` | BTR-82A | No |

**Importante:** Los nombres deben escribirse **exactamente** como aparecen en la tabla (mayusculas, guiones, guiones bajos). Un error de escritura hace que el programa se detenga.

### Paso 7.5 — Ejecutar tu test

```bash
./lanchester tests/test_09_mi_prueba.json
```

Revisa el JSON de salida. Los campos clave son:
- `outcome`: `"BLUE_WINS"`, `"RED_WINS"`, `"DRAW"` o `"INDETERMINATE"`
- `blue_survivors` / `red_survivors`: cuantos quedan de cada bando
- `blue_casualties` / `red_casualties`: cuantas bajas hubo

### Paso 7.6 — Probar con los dos modos de agregacion

```bash
./lanchester tests/test_09_mi_prueba.json --aggregation pre
./lanchester tests/test_09_mi_prueba.json --aggregation post
```

La diferencia entre `pre` y `post` solo es relevante cuando hay **fuerzas mixtas** (mas de un tipo de vehiculo en el mismo bando). Con un solo tipo de vehiculo, ambos modos dan el mismo resultado.

---

## PARTE 8: Tests adicionales recomendados

Estos escenarios **no estan cubiertos** por los 8 tests actuales y seria bueno anadirlos:

### 8.1 Combate encadenado (chaining)

Verifica que los supervivientes del combate 1 pasan al combate 2 con fuerzas reducidas.

Crea `tests/test_09_chain.json`:

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

Ejecuta y verifica:
```bash
./lanchester tests/test_09_chain.json
```
- El combate 1 debe ser `BLUE_WINS`
- En el combate 2, `blue_initial` debe ser **menor que 10** (azul llego mermado)

### 8.2 Terrenos extremos

```bash
cp tests/test_01_symmetric.json tests/test_10_facil.json
```
Edita `test_10_facil.json` y cambia `"terrain": "MEDIO"` por `"terrain": "FACIL"`.

```bash
cp tests/test_01_symmetric.json tests/test_11_dificil.json
```
Edita `test_11_dificil.json` y cambia `"terrain": "MEDIO"` por `"terrain": "DIFICIL"`.

Ejecuta ambos y verifica que siguen siendo DRAW (el terreno afecta a ambos bandos por igual en un combate simetrico).

### 8.3 Modo batch (procesar multiples escenarios de golpe)

```bash
mkdir -p /tmp/batch_test
cp tests/test_01_symmetric.json tests/test_02_overwhelming.json /tmp/batch_test/
./lanchester --batch /tmp/batch_test --output /tmp/batch_result.csv
cat /tmp/batch_result.csv
```

Debe generar un CSV con una fila por escenario.

### 8.4 Modo sweep (barrido parametrico)

Los scripts de ejemplo ya incluidos en `ejemplos/` hacen esto:

```bash
bash ejemplos/sweep_count.sh
bash ejemplos/sweep_distancia.sh
bash ejemplos/sweep_engagement.sh
```

Cada uno genera un CSV variando un parametro. Revisa la salida para comprobar que tiene sentido (mas tropas = mas supervivientes, mayor distancia = menos dano, etc.)

### 8.5 Modo Monte Carlo (simulacion estocastica)

Verifica que la simulacion estocastica produce resultados estadisticamente consistentes con el determinista.

```bash
# Ejecutar Monte Carlo con 500 replicas
./lanchester tests/test_02_overwhelming.json --montecarlo 500 --seed 42
```

Que comprobar:
- La salida incluye `"mode": "montecarlo"` y `"n_replicas": 500`
- El resultado `deterministic` es identico al modo normal
- `outcome_distribution.BLUE_WINS` debe ser cercano a 1.0 (fuerza abrumadora)
- `blue_survivors.mean` debe estar cerca del valor determinista
- Los percentiles p05 < p25 < median < p75 < p95

```bash
# Validacion automatica de Monte Carlo
python3 tests/test_montecarlo_validation.py
```

### 8.6 Modo sensibilidad (analisis de parametros)

Verifica que el analisis de sensibilidad produce salida correcta.

```bash
./lanchester tests/test_01_symmetric.json --sensitivity --output /tmp/sens_test.csv
cat /tmp/sens_test.csv
```

Que comprobar:
- El CSV tiene columnas: parametro, valor_base, factor, valor_test, blue_surv_ref, blue_surv_test, etc.
- Los factores son 0.8, 0.9, 1.1, 1.2
- Las elasticidades indican que parametros son mas influyentes
- En un combate simetrico, los cambios afectan igual a ambos bandos

### 8.7 Verificacion analitica

```bash
python3 tests/test_analytical_verify.py
```

Compara la salida del simulador contra la solucion cerrada de las ecuaciones de Lanchester para un caso sin C/C. Verifica que el error numerico del integrador RK4 esta dentro de tolerancia.

---

## PARTE 9: Problemas frecuentes y soluciones

| Que ves en el terminal | Causa | Solucion |
|---|---|---|
| `g++: command not found` | No tienes compilador | `sudo apt install g++` |
| `make: command not found` | No tienes make | `sudo apt install make` |
| `error: filesystem header not found` | g++ muy antiguo (< version 8) | `sudo apt install g++-11` y editar Makefile para usar `g++-11` |
| `Vehicle 'XXX' not found in any database` | Nombre mal escrito en el JSON | Revisa la tabla de vehiculos en la Parte 7.4 |
| `Permission denied: ./lanchester` | Falta permiso de ejecucion | `chmod +x lanchester` |
| `python3: command not found` | No tienes Python 3 | `sudo apt install python3` |
| `No such file or directory` al ejecutar | No compilaste o estas en otra carpeta | `make` y verifica con `ls lanchester` |
| Tests pasan en un PC pero fallan en otro | Diferencias de precision flotante | La tolerancia del script (0.01) deberia cubrirlo |
| VS Code no se conecta al servidor | SSH no configurado | Verifica que puedes hacer `ssh usuario@ip` desde un terminal normal |
