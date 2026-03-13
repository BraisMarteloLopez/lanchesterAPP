# Modelo Lanchester-CIO

Herramienta de investigacion interna del CIO/ET. Calcula el resultado de enfrentamientos entre fuerzas terrestres usando ecuaciones de Lanchester: vencedor, bajas, duracion, municion consumida.

**Aplicacion de escritorio para Windows** con interfaz grafica (Dear ImGui + SDL2).

## Uso rapido

1. Copia la carpeta `release/` a un PC con Windows
2. Ejecuta `lanchester_gui.exe`
3. Configura el escenario en el panel izquierdo y pulsa "EJECUTAR SIMULACION"

Contenido de `release/`:

| Fichero | Descripcion |
|---|---|
| `lanchester_gui.exe` | Aplicacion principal (GUI) |
| `SDL2.dll` | Runtime grafico (necesario) |
| `model_params.json` | Parametros del modelo |
| `vehicle_db.json` | Catalogo vehiculos azul (NATO) |
| `vehicle_db_en.json` | Catalogo vehiculos rojo (OPFOR) |

## Interfaz grafica

La aplicacion permite:

- **Configurar escenarios**: seleccionar vehiculos y cantidades para cada bando, terreno, distancia de enfrentamiento, estado tactico, movilidad
- **Modo determinista**: simulacion unica con resultado exacto (integrador RK4)
- **Modo Monte Carlo**: N replicas estocasticas con distribucion de resultados (probabilidad de victoria, percentiles de supervivientes)
- **Parametros avanzados**: bajas AFT pre-contacto, fraccion de empenamiento, factores de cadencia y efectivos
- **Visualizacion**: tablas de resultados y graficas de barras (supervivientes, distribucion de outcomes)

### Opciones de agregacion

- `PRE` (por defecto): media ponderada de parametros, una tasa unica
- `POST`: tasa individual por tipo de vehiculo, luego media ponderada. Mas realista para fuerzas heterogeneas

## Catalogos de vehiculos

**Bando azul** (`vehicle_db.json`):

| Nombre | Vehiculo | C/C |
|---|---|---|
| LEOPARDO_2E | Carro de combate Leopard 2E | No |
| PIZARRO | VCBR Pizarro | Si |
| TOA_SPIKE_I | TOA con misiles Spike | Si |
| VEC_25 | VEC 8x8 con canon 25mm | No |

**Bando rojo** (`vehicle_db_en.json`):

| Nombre | Vehiculo | C/C |
|---|---|---|
| T-80U | Carro de combate T-80U | No |
| T-72B3 | Carro de combate T-72B3 | No |
| BMP-3 | BMP-3 | Si |
| BTR-82A | BTR-82A | No |

## Parametros del modelo

Los parametros se cargan desde `model_params.json` (junto al .exe):

- **`kill_probability_slope`**: pendiente de la sigmoide de probabilidad de destruccion (175.0)
- **`distance_degradation_coefficients`**: polinomio de degradacion por distancia (6 coeficientes)
- **`tactical_multipliers`**: multiplicadores por estado tactico
- **`terrain_fire_effectiveness`**: efectividad del fuego por terreno (FACIL/MEDIO/DIFICIL)

## Referencia de campos del escenario

| Campo | Valores |
|---|---|
| `terrain` | `FACIL`, `MEDIO`, `DIFICIL` |
| `tactical_state` | `Ataque a posicion defensiva`, `Busqueda del contacto`, `En posicion de tiro`, `Defensiva condiciones minimas`, `Defensiva organizacion ligera`, `Defensiva organizacion media`, `Retardo`, `Retrocede` |
| `mobility` | `MUY_ALTA`, `ALTA`, `MEDIA`, `BAJA` |
| `engagement_fraction` | Fraccion de la fuerza que entra en combate [0.0-1.0] |
| `aft_casualties_pct` | Porcentaje de bajas por AFT pre-contacto [0.0-1.0] |
| `rate_factor` | Multiplicador de tasa de destruccion (defecto 1.0) |
| `count_factor` | Multiplicador de vehiculos efectivos (defecto 1.0) |

## Velocidades tacticas (km/h)

| Movilidad \ Terreno | FACIL | MEDIO | DIFICIL |
|---|---|---|---|
| MUY_ALTA | 40 | 25 | 12 |
| ALTA | 30 | 20 | 10 |
| MEDIA | 20 | 12 | 6 |
| BAJA | 10 | 6 | 3 |

## Compilacion (cross-compile desde Linux)

Requisitos: `g++-mingw-w64-x86-64` (MinGW-w64 cross-compiler).

```bash
# 1. Instalar cross-compiler
sudo apt install g++-mingw-w64-x86-64

# 2. Descargar dependencias (SDL2, Dear ImGui, implot)
bash setup_gui_deps.sh

# 3. Compilar
make
```

Genera `release/lanchester_gui.exe` + `release/SDL2.dll`.

## Decisiones de diseno

- **Integrador RK4.** Runge-Kutta de orden 4 para la integracion temporal.
- **Monte Carlo con Poisson.** Bajas discretas con distribuciones de Poisson. Subdivision automatica si lambda > 2.
- **Punteria (U) no aplica a C/C.** Los misiles guiados tienen probabilidad absorbida en la sigmoide.
- **Agregacion pre-tasa por defecto.** Usar POST para mayor realismo con fuerzas heterogeneas.
- **Distribucion de bajas por vulnerabilidad.** Vehiculos mas blindados sobreviven proporcionalmente mas en encadenamiento.
- **Parametros externalizados.** Todos calibrables desde `model_params.json`.

## Estructura

```
├── gui_main.cpp              # Aplicacion GUI (Dear ImGui + SDL2 + implot)
├── lanchester_types.h        # Tipos de datos y estructuras
├── lanchester_model.h        # Funciones matematicas, simulacion, Monte Carlo
├── lanchester_io.h           # I/O JSON/CSV, escenarios, batch, sweep, sensibilidad
├── model_params.json         # Parametros del modelo (calibrables)
├── vehicle_db.json           # Catalogo vehiculos azul
├── vehicle_db_en.json        # Catalogo vehiculos rojo
├── include/nlohmann/json.hpp # Dependencia JSON header-only
├── Makefile                  # Cross-compilacion MinGW-w64 -> Windows .exe
├── setup_gui_deps.sh         # Descarga dependencias (SDL2, ImGui, implot)
├── ejemplos/
│   ├── toa_vs_t80u.json      # Escenario simple
│   └── compania_mixta.json   # Cadena de 2 combates
├── tests/
│   └── test_*.json           # 9 escenarios de prueba
└── release/
    ├── lanchester_gui.exe    # Ejecutable Windows
    ├── SDL2.dll              # Runtime SDL2
    ├── model_params.json     # Parametros del modelo
    ├── vehicle_db.json       # Catalogo azul
    └── vehicle_db_en.json    # Catalogo rojo
```

---

*CIO / ET — Herramienta de investigacion interna*
