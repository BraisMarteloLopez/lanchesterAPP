# Modelo Lanchester-CIO

Herramienta de investigacion interna del CIO/ET. Simula enfrentamientos entre fuerzas terrestres usando ecuaciones de Lanchester (ley cuadrada): vencedor, bajas, duracion, municion consumida.

**Aplicacion de escritorio para Windows** con interfaz grafica (Dear ImGui + SDL2).

> **Aviso**: Todos los parametros del modelo estan marcados como `uncalibrated`. Los resultados no deben usarse para informar decisiones operativas sin una calibracion previa contra datos de referencia.

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

## Modelo matematico

### Ecuaciones base

El modelo implementa la **ley cuadrada de Lanchester** con integracion RK4. Las fuerzas A (azul) y R (rojo) evolucionan segun:

```
dA/dt = -S_red(t) * R(t)
dR/dt = -S_blue(t) * A(t)
```

Donde `S(t)` es la tasa efectiva de destruccion, que depende de:

### Tasa de fuego convencional

```
S_conv = T(D, P) * G(d, f, A_max) * U * c * mult_tactico * mult_terreno * rate_factor
```

| Componente | Formula | Descripcion |
|---|---|---|
| `T(D, P)` | `1 / (1 + exp((D - P) / slope))` | Probabilidad de destruccion (sigmoide). D = proteccion del objetivo, P = penetracion del atacante |
| `G(d, f, A_max)` | Polinomio de 6 coeficientes | Degradacion por distancia. 0 si d > A_max |
| `U` | Escalar [0-1] | Punteria del artillero (no aplica a misiles C/C) |
| `c` | Disparos/min | Cadencia de fuego |

### Tasa de fuego C/C (misiles contracarro)

```
S_cc = S_cc_static * (A_current / A0) * ammo_remaining_frac * rate_factor
```

Municion finita: el pool total es `M * n_cc_initial`. La tasa decae a medida que se consume municion.

### Distancia variable

Si un bando ataca, la distancia se reduce en cada paso temporal:

```
d(t) = max(50, d0 - v_approach * t)
```

Las tasas de destruccion se recalculan en cada paso del integrador.

### Monte Carlo

Modo estocastico con bajas discretas Poisson por paso temporal. Subdivision automatica del paso si lambda > 2 para preservar la fidelidad de la distribucion.

### Parametros de vehiculo

| Campo | Descripcion |
|---|---|
| `D` | Proteccion/blindaje (convencional) |
| `P` | Penetracion del armamento principal |
| `U` | Factor de punteria [0-1] |
| `c` | Cadencia de fuego (disparos/min) |
| `A_max` | Alcance maximo efectivo (m) |
| `f` | Factor de distancia del vehiculo |
| `CC` | Tiene capacidad contracarro (0/1) |
| `P_cc`, `D_cc`, `c_cc`, `A_cc`, `f_cc` | Parametros C/C equivalentes |
| `M` | Municion C/C por vehiculo |

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

Los parametros se cargan desde `model_params.json` (junto al .exe). **Todos estan sin calibrar** — requieren validacion contra datos reales o simulaciones de referencia.

| Parametro | Valor por defecto | Estado |
|---|---|---|
| `kill_probability_slope` | 175.0 | Sin calibrar |
| `distance_degradation_coefficients` | 6 coeficientes polinomiales | Sin calibrar |
| `tactical_multipliers` | Por estado tactico | Sin calibrar |
| `terrain_fire_effectiveness` | FACIL=1.0, MEDIO=0.85, DIFICIL=0.65 | Sin calibrar |

Cada parametro en el JSON incluye `origin`, `calibration_status` y `valid_range` para facilitar una futura calibracion.

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

## Compilacion y tests

### Tests (Linux nativo)

```bash
cmake -B build -DLANCHESTER_BUILD_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

28 tests unitarios con Catch2 v3 que cubren:
- Carga de parametros y catalogos (`ModelParamsClass`, `VehicleCatalogClass`)
- Modelo de simulacion (`SquareLawModel`) con validacion contra baseline legacy
- Servicio de simulacion (`SimulationService`) con ejecucion sincrona y asincrona
- Validacion de configuraciones (`ScenarioConfig`)

### GUI Windows (cross-compile desde Linux)

```bash
# 1. Instalar cross-compiler
sudo apt install g++-mingw-w64-x86-64

# 2. Descargar dependencias (SDL2, Dear ImGui, implot)
bash setup_gui_deps.sh

# 3a. Con CMake
cmake -B build-win \
  -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-toolchain.cmake \
  -DLANCHESTER_BUILD_GUI=ON -DLANCHESTER_BUILD_TESTS=OFF
cmake --build build-win

# 3b. O con Makefile (legacy)
make
```

Genera `release/lanchester_gui.exe` + `release/SDL2.dll`.

## Arquitectura

> **Estado: migracion en curso.** El codigo esta en transicion de una arquitectura monolitica (funciones inline + global) a una arquitectura OOP por capas. Ambas coexisten. Ver `PLAN_REFACTORIZACION.md` para el plan completo y `DEUDA_TECNICA.md` para la deuda pendiente.

### Capas (objetivo y estado actual)

```
┌──────────────────────────────────────────────────────┐
│  PRESENTACION (src/ui/)                              │
│    gui_main.cpp — GUI (Dear ImGui + SDL2)            │
│    [!] Llama a funciones legacy, ignora el servicio  │
├──────────────────────────────────────────────────────┤
│  APLICACION (src/application/)                       │
│    SimulationService — existe, delega en legacy      │
│    ScenarioConfig    — configuracion tipada (OK)     │
│    lanchester_io.h   — legacy activo (985 lineas)    │
├──────────────────────────────────────────────────────┤
│  DOMINIO (src/domain/)                               │
│    ILanchesterModel  — interfaz abstracta (OK)       │
│    SquareLawModel    — ley cuadrada (OK, usada en    │
│                        tests, no en GUI)             │
│    ModelParamsClass  — parametros (OK, con puente    │
│                        applyToGlobal())              │
│    VehicleCatalogClass — catalogo (OK, con raw())    │
│    lanchester_model.h — legacy activo (710 lineas,   │
│                         usa g_model_params global)   │
└──────────────────────────────────────────────────────┘
```

- **Lo que funciona:** `SquareLawModel` esta completamente encapsulada y validada por 28 tests Catch2. `ILanchesterModel` permite futuras variantes.
- **Lo que falta:** La GUI y `SimulationService` siguen usando las funciones legacy de `lanchester_model.h`/`lanchester_io.h` en lugar de `SquareLawModel`. El global `g_model_params` no ha sido eliminado. Ver DT-017 a DT-020 en `DEUDA_TECNICA.md`.

### Estructura de ficheros

```
src/
├── domain/
│   ├── lanchester_types.h           # Structs y enums base
│   ├── lanchester_model.h           # Funciones legacy (puente temporal)
│   ├── lanchester_model_iface.h     # Interfaz abstracta ILanchesterModel
│   ├── model_params.h/cpp           # Clase ModelParamsClass
│   ├── vehicle_catalog.h/cpp        # Clase VehicleCatalogClass
│   └── square_law_model.h/cpp       # Clase SquareLawModel (RK4 + MC Poisson)
├── application/
│   ├── lanchester_io.h              # Legacy: I/O JSON, batch, sweep, sensibilidad
│   ├── scenario_config.h/cpp        # ScenarioConfig + validacion + toJson()
│   └── simulation_service.h/cpp     # SimulationService (sincrono + async)
├── ui/
│   └── gui_main.cpp                 # GUI Windows (Dear ImGui + SDL2 + implot)
├── tests/
│   ├── test_main.cpp                # Entry point Catch2
│   ├── test_model_params.cpp        # Tests de ModelParamsClass
│   ├── test_vehicle_catalog.cpp     # Tests de VehicleCatalogClass
│   ├── test_square_law.cpp          # Tests del modelo + retrocompatibilidad
│   ├── test_simulation_service.cpp  # Tests de integracion del servicio
│   └── data/                        # JSONs de test (params, catalogos, escenarios)
└── include/nlohmann/json.hpp        # Dependencia JSON header-only

CMakeLists.txt                       # Build system principal
cmake/mingw-w64-toolchain.cmake      # Toolchain cross-compilacion
Makefile                             # Build legacy (fallback)
setup_gui_deps.sh                    # Descarga dependencias (SDL2, ImGui, implot)
model_params.json                    # Parametros del modelo
vehicle_db.json                      # Catalogo vehiculos azul
vehicle_db_en.json                   # Catalogo vehiculos rojo
ejemplos/
├── toa_vs_t80u.json                 # Escenario simple (TOA Spike vs T-80U)
└── compania_mixta.json              # Cadena de 2 combates (fuerza mixta)
tests/
├── test_01_symmetric.json           # 9 escenarios JSON de validacion
├── ...                              #   (simetria, fuera de alcance, AFT,
└── test_09_analytical.json          #    analitico, fuerzas mixtas, etc.)
release/                             # Binarios Windows distribuibles
```

## Decisiones de diseno

- **Ley cuadrada de Lanchester.** Las bajas son proporcionales al numero de efectivos enemigos y a su tasa de fuego. Valida para combates de fuego directo.
- **Integrador RK4.** Runge-Kutta de orden 4 con error validado (0.051 vehiculos vs solucion analitica cerrada).
- **Monte Carlo con Poisson.** Bajas discretas por paso temporal. Subdivision automatica si lambda > 2 para evitar distorsion.
- **Punteria (U) no aplica a C/C.** Los misiles guiados tienen probabilidad absorbida en la sigmoide de kill probability.
- **Agregacion pre-tasa por defecto.** POST mas realista para fuerzas heterogeneas (calcula tasa individual por tipo, luego pondera).
- **Distribucion de bajas por vulnerabilidad.** En encadenamiento de combates, vehiculos con menor D_cc reciben proporcionalmente mas bajas.
- **Parametros externalizados.** Todos en `model_params.json` con metadatos de origen y estado de calibracion.
- **Arquitectura OOP desacoplada.** Interfaz abstracta del modelo permite variantes futuras. Servicio con ejecucion async segura. GUI reemplazable.

## Limitaciones conocidas

- **Solo ley cuadrada.** No implementa la ley lineal de Lanchester (combate cuerpo a cuerpo / area) ni modelos mixtos.
- **Sin modelado de C2.** No representa cadena de mando, comunicaciones ni degradacion por perdida de puestos de mando.
- **Sin logistica.** No modela suministro de combustible, repuestos ni evacuacion sanitaria. La unica restriccion logistica es la municion C/C finita.
- **Sin efectos de red.** No hay sinergia entre sensores, comunicaciones y sistemas de armas (ej: datalink, guerra electronica).
- **Terreno abstracto.** Tres niveles discretos (FACIL/MEDIO/DIFICIL) sin modelado topografico, lineas de vista ni cobertura.
- **Combate directo unicamente.** No incluye fuegos indirectos (artilleria, morteros), apoyo aereo ni defensa antiaerea.
- **Parametros sin calibrar.** Todos los valores por defecto son estimaciones iniciales sin validacion contra datos historicos o simulaciones de referencia.

## Documentos relacionados

| Documento | Contenido |
|---|---|
| `PLAN_REFACTORIZACION.md` | Arquitectura OOP, fases de migracion (0 completada, 1-3 parciales, 4-5 pendientes) |
| `PLAN_INTERFAZ.md` | Diseño de la nueva interfaz wizard + presentacion 2D (pendiente, bloqueado por fase 4) |
| `PLAN_DE_PRUEBAS.md` | Guia paso a paso para pruebas manuales en Windows |
| `DEUDA_TECNICA.md` | Registro de deuda tecnica: 13/16 originales resueltos + 4 nuevos pendientes |

---

*CIO / ET — Herramienta de investigacion interna*
