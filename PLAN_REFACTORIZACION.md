# Plan de Refactorizacion — Lanchester-CIO v2

Refactorizacion de la arquitectura interna (dominio, servicios, build, tests).
Este plan **no** cubre la interfaz grafica — ver `PLAN_INTERFAZ.md` para eso.

---

## 1. Estado actual y problemas

### 1.1 Arquitectura actual

```
gui_main.cpp ──> lanchester_io.h ──> lanchester_model.h ──> lanchester_types.h
     |                                       |
     └── AppState (struct monolitico)        └── g_model_params (global mutable)
```

- **4 ficheros**, compilacion header-only con funciones `inline`.
- **Sin clases**: todo son structs de datos + funciones libres.
- **Estado global**: `g_model_params` leido implicitamente por el modelo.
- **GUI acoplada**: `gui_main.cpp` mezcla logica de construccion de escenarios, rendering y estado de la aplicacion.
- **Race condition**: `std::async` captura `&app` por referencia mientras la UI muta el estado.
- **Tests manuales**: 9 escenarios JSON sin runner automatizado ni validacion de resultados esperados.

### 1.2 Limitaciones para escalar

| Problema | Impacto |
|---|---|
| Global mutable `g_model_params` | No se pueden ejecutar simulaciones con parametros distintos en paralelo |
| Funciones libres sin encapsulacion | Dificil añadir variantes del modelo (Lanchester lineal, logaritmico, etc.) |
| GUI acoplada al motor | Imposible cambiar la interfaz sin reescribir toda la logica de escenarios |
| Sin capa de servicios | La GUI llama directamente a funciones de I/O y simulacion |
| Sin tests automatizados | No hay validacion de regresion al hacer cambios |
| `lanchester_io.h` ~970 lineas | Mezcla I/O, parseo, batch, sweep, sensibilidad y serializacion |

---

## 2. Arquitectura objetivo

### 2.1 Diagrama de capas

```
┌─────────────────────────────────────────────────────────────┐
│                CAPA DE PRESENTACION (ver PLAN_INTERFAZ.md)  │
│                          │                                  │
│              Cualquier GUI futura consume                   │
│              la API publica de la capa de aplicacion        │
│                          │                                  │
├──────────────────────────┼──────────────────────────────────┤
│                   CAPA DE APLICACION                        │
│                          │                                  │
│                 SimulationService                           │
│              ┌───────────┼───────────┐                      │
│              │           │           │                      │
│      ScenarioConfig   SimRunner   ResultExporter            │
│                          │                                  │
├──────────────────────────┼──────────────────────────────────┤
│                    CAPA DE DOMINIO                          │
│                          │                                  │
│              ┌───────────┼───────────┐                      │
│              │           │           │                      │
│       LanchesterModel  VehicleCatalog  ModelParams          │
│         (abstract)       (class)       (class)              │
│           │                                                 │
│     ┌─────┴──────┐                                          │
│     │            │                                          │
│  SquareLaw   LinearLaw   (futuras variantes)                │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

La capa de presentacion es un consumidor de `SimulationService`. Se puede implementar como GUI ImGui, CLI, API REST o tests — sin cambiar nada en dominio ni aplicacion.

### 2.2 Clases del dominio

#### `ModelParams`

Reemplaza el global `g_model_params`. Instancia inmutable pasada por referencia.

```cpp
class ModelParams {
public:
    static ModelParams load(const std::string& path);

    double killProbabilitySlope() const;
    const DistanceDegradationCoeffs& distCoeffs() const;
    double terrainFireMult(Terrain t) const;
    TacticalMult tacticalMult(const std::string& state) const;

private:
    double kill_probability_slope_;
    DistanceDegradationCoeffs dist_coeffs_;
    std::map<Terrain, double> terrain_mult_;
    std::map<std::string, TacticalMult> tactical_mult_;
};
```

#### `VehicleCatalog`

Encapsula la carga y busqueda de vehiculos.

```cpp
class VehicleCatalog {
public:
    static VehicleCatalog load(const std::string& path);

    const VehicleParams& find(const std::string& name) const;
    std::vector<std::string> names() const;
    bool contains(const std::string& name) const;

private:
    std::map<std::string, VehicleParams> vehicles_;
};
```

#### `ILanchesterModel` (interfaz abstracta)

Permite futuras variantes del modelo sin tocar servicios ni GUI.

```cpp
class ILanchesterModel {
public:
    virtual ~ILanchesterModel() = default;

    virtual CombatResult simulate(const CombatInput& input) const = 0;
    virtual CombatResult simulateStochastic(const CombatInput& input,
                                            std::mt19937& rng) const = 0;
    virtual std::string name() const = 0;
};
```

#### `SquareLawModel` (implementacion actual)

Contiene toda la logica de `lanchester_model.h` como metodos de clase.
Recibe `ModelParams` por referencia constante en el constructor — sin globales.

```cpp
class SquareLawModel : public ILanchesterModel {
public:
    explicit SquareLawModel(const ModelParams& params);

    CombatResult simulate(const CombatInput& input) const override;
    CombatResult simulateStochastic(const CombatInput& input,
                                    std::mt19937& rng) const override;
    std::string name() const override { return "Lanchester Square Law (RK4)"; }

private:
    const ModelParams& params_;

    // Funciones internas actuales convertidas a metodos privados
    double killProbability(double D, double P) const;
    double distanceDegradation(double d, double f, double range) const;
    EffectiveRates computeRates(const AggregatedParams& att,
                                const AggregatedParams& def,
                                double dist, const std::string& att_state,
                                const std::string& def_state,
                                double rate_factor) const;
    EffectiveRates computeRatesPost(const std::vector<CompositionEntry>& att_comp,
                                    const AggregatedParams& def,
                                    double dist, const std::string& att_state,
                                    const std::string& def_state,
                                    double rate_factor) const;
    double totalRate(const EffectiveRates& er, double N_att, double N_att0,
                     double cc_ammo_consumed, double cc_ammo_max) const;
    AggregatedParams aggregate(const std::vector<CompositionEntry>& comp) const;
};
```

### 2.3 Capa de aplicacion

#### `ScenarioConfig`

Struct de datos puros que reemplaza la construccion ad-hoc de JSON en la GUI.
Este es el contrato entre la UI (cualquiera) y el motor de simulacion.

```cpp
struct SideConfig {
    std::vector<CompositionEntry> composition;
    std::string tactical_state;
    Mobility mobility;
    double aft_pct = 0;
    double engagement_fraction = 1.0;
    double rate_factor = 1.0;
    double count_factor = 1.0;
};

struct ScenarioConfig {
    Terrain terrain = Terrain::MEDIO;
    double distance_m = 2000;
    double t_max = 30.0;
    AggregationMode aggregation = AggregationMode::PRE;
    SideConfig blue;
    SideConfig red;

    // Validacion integrada
    void validate() const;

    // Conversion desde/hacia JSON (para batch, tests, importar escenarios)
    static ScenarioConfig fromJson(const nlohmann::json& j,
                                    const VehicleCatalog& blue_cat,
                                    const VehicleCatalog& red_cat);
    nlohmann::json toJson() const;
};
```

#### `SimulationService`

Orquesta la ejecucion. Punto de entrada unico para cualquier UI o test.

```cpp
class SimulationService {
public:
    SimulationService(std::shared_ptr<ILanchesterModel> model,
                      VehicleCatalog blueCat,
                      VehicleCatalog redCat);

    // Sincrono
    ScenarioOutput runScenario(const ScenarioConfig& config) const;
    MonteCarloOutput runMonteCarlo(const ScenarioConfig& config,
                                   int replicas, uint64_t seed) const;

    // Asincrono — recibe por VALOR, sin race conditions
    std::future<ScenarioOutput> runScenarioAsync(ScenarioConfig config) const;
    std::future<MonteCarloOutput> runMonteCarloAsync(ScenarioConfig config,
                                                      int replicas,
                                                      uint64_t seed) const;

    // Batch y sweep
    void runBatch(const std::string& dir, const std::string& output) const;
    void runSweep(const ScenarioConfig& base, const std::string& param_path,
                  double start, double end, double step,
                  const std::string& output) const;

    // Acceso a catalogos (read-only, para que la UI pueda listar vehiculos)
    const VehicleCatalog& blueCatalog() const;
    const VehicleCatalog& redCatalog() const;

private:
    std::shared_ptr<ILanchesterModel> model_;
    VehicleCatalog blue_cat_;
    VehicleCatalog red_cat_;
};
```

#### `ResultExporter`

Serializacion de resultados (extrae logica de `lanchester_io.h`).

```cpp
class ResultExporter {
public:
    static nlohmann::json toJson(const CombatResult& r, int precision = 6);
    static nlohmann::json toJson(const ScenarioOutput& s);
    static nlohmann::json toJson(const MonteCarloOutput& mc);
    static void writeCsv(const ScenarioOutput& s, std::ostream& out);
};
```

---

## 3. Estructura de ficheros objetivo

```
src/
├── domain/
│   ├── types.h                      # Structs: VehicleParams, CombatInput, CombatResult, etc.
│   ├── model_params.h               # Clase ModelParams (declaracion)
│   ├── model_params.cpp             # ModelParams::load(), accessors
│   ├── vehicle_catalog.h            # Clase VehicleCatalog (declaracion)
│   ├── vehicle_catalog.cpp          # VehicleCatalog::load(), find()
│   ├── lanchester_model.h           # Interfaz abstracta ILanchesterModel
│   ├── square_law_model.h           # Clase SquareLawModel (declaracion)
│   └── square_law_model.cpp         # Implementacion: simulate(), simulateStochastic(), RK4, MC
│
├── application/
│   ├── scenario_config.h            # ScenarioConfig + SideConfig
│   ├── scenario_config.cpp          # validate(), fromJson(), toJson()
│   ├── simulation_service.h         # Clase SimulationService (declaracion)
│   ├── simulation_service.cpp       # Orquestacion: run, runMC, batch, sweep
│   ├── result_exporter.h            # Clase ResultExporter
│   └── result_exporter.cpp          # Serializacion JSON/CSV
│
├── ui/                              # --> Definido en PLAN_INTERFAZ.md
│   └── (...)
│
├── tests/
│   ├── test_main.cpp                # Catch2 entry point
│   ├── test_model_params.cpp        # Carga y validacion de parametros
│   ├── test_vehicle_catalog.cpp     # Carga y busqueda de vehiculos
│   ├── test_square_law.cpp          # Tests unitarios del modelo vs baseline
│   ├── test_square_law_analytical.cpp  # Test contra solucion cerrada
│   ├── test_scenario_config.cpp     # Validacion de configuraciones
│   ├── test_simulation_service.cpp  # Tests de integracion
│   └── data/                        # Copia de JSONs de test
│       ├── model_params.json
│       ├── vehicle_db.json
│       ├── vehicle_db_en.json
│       └── test_*.json              # Los 9 escenarios actuales
│
├── data/
│   ├── model_params.json
│   ├── vehicle_db.json
│   └── vehicle_db_en.json
│
├── include/
│   └── nlohmann/json.hpp
│
├── main.cpp                         # Entry point (instancia dominio + servicio + lanza UI)
├── CMakeLists.txt
└── Makefile                         # Mantenido como fallback hasta validar CMake
```

---

## 4. Fases de implementacion

### Fase 0: Infraestructura de build y tests

**Objetivo:** CMake + Catch2 + estructura de directorios, sin mover logica aun.

| Tarea | Detalle |
|---|---|
| 0.1 | Crear `CMakeLists.txt` con targets: `lanchester_core` (library), `lanchester_gui` (executable), `lanchester_tests` (test executable) |
| 0.2 | Configurar cross-compilacion MinGW en CMake (toolchain file) |
| 0.3 | Integrar Catch2 v3 header-only via FetchContent |
| 0.4 | Crear estructura de directorios `src/domain/`, `src/application/`, `src/tests/` |
| 0.5 | Mover ficheros actuales a `src/` sin cambiar su contenido (solo paths de include) |
| 0.6 | Verificar: `cmake --build .` genera el .exe identico, `ctest` ejecuta un test trivial |

**Criterio de aceptacion:** El build produce el mismo binario. Makefile original sigue funcionando como fallback.

**Entregable:** CMakeLists.txt funcional, estructura de directorios creada.

---

### Fase 1: Dominio OOP — ModelParams y VehicleCatalog

**Objetivo:** Extraer las dos primeras clases, eliminar el global.

| Tarea | Detalle |
|---|---|
| 1.1 | Crear `ModelParams` clase con factory `load()`. Mover logica de `load_model_params()` y `read_param()` |
| 1.2 | Crear `VehicleCatalog` clase con `load()`, `find()`, `names()`, `contains()`. Mover logica de `load_catalog()`, `vehicle_from_json()`, `find_vehicle()` |
| 1.3 | Eliminar `extern ModelParams g_model_params`. Instanciar en `main()` y pasar por referencia |
| 1.4 | Adaptar GUI para recibir `ModelParams` y `VehicleCatalog` como dependencias inyectadas (minimo cambio en gui_main.cpp) |
| 1.5 | Tests: `test_model_params.cpp` (carga correcta, defaults, fichero inexistente), `test_vehicle_catalog.cpp` (carga, busqueda, vehiculo inexistente lanza excepcion) |

**Criterio de aceptacion:** `g_model_params` ya no existe. Tests pasan. GUI funciona identico.

---

### Fase 2: Dominio OOP — ILanchesterModel y SquareLawModel

**Objetivo:** Encapsular el motor de simulacion en una clase con interfaz abstracta.

| Tarea | Detalle |
|---|---|
| 2.1 | Definir interfaz `ILanchesterModel` en `lanchester_model.h` |
| 2.2 | Implementar `SquareLawModel` moviendo las funciones de `lanchester_model.h` actual: `kill_probability`, `distance_degradation`, `static_rate_conv`, `static_rate_cc`, `dynamic_rate_cc`, `compute_effective_rates`, `compute_effective_rates_post`, `total_rate`, `initial_forces`, `aggregate`, `simulate_combat`, `simulate_combat_stochastic`, `run_montecarlo_combat`, `compute_stats`, `distribute_casualties_by_vulnerability` |
| 2.3 | Todas las funciones que leian `g_model_params` ahora usan `params_` (miembro de la clase) |
| 2.4 | Las funciones helper (`get_tactical_multipliers`, `terrain_fire_multiplier`, `parse_mobility`, `parse_terrain`, `tactical_speed`, `displacement_time_minutes`) se convierten en metodos o funciones de utilidad en `types.h` |
| 2.5 | Tests: `test_square_law.cpp` ejecuta los 9 escenarios JSON y compara resultados contra baseline capturado previamente (tolerancia epsilon=0.01) |
| 2.6 | Test: `test_square_law_analytical.cpp` valida contra solucion cerrada de Lanchester |

**Criterio de aceptacion:** Cero funciones libres de simulacion. Todos los tests pasan con resultados identicos al baseline.

---

### Fase 3: Capa de aplicacion — SimulationService y ScenarioConfig

**Objetivo:** Crear la capa de servicios que desacopla GUI del motor.

| Tarea | Detalle |
|---|---|
| 3.1 | Crear `ScenarioConfig` con `validate()`, `fromJson()`, `toJson()` |
| 3.2 | Mover logica de `run_scenario()` y `run_scenario_montecarlo()` a `SimulationService` como metodos que reciben `ScenarioConfig` |
| 3.3 | Implementar `runScenarioAsync()` y `runMonteCarloAsync()` con captura por **valor** |
| 3.4 | Crear `ResultExporter` con serializacion JSON/CSV (mover de `lanchester_io.h`) |
| 3.5 | Mover `run_batch()`, `run_sweep()`, `run_sensitivity()` al servicio |
| 3.6 | Tests de integracion: `test_simulation_service.cpp` valida escenarios completos via el servicio |
| 3.7 | En este punto, `lanchester_io.h` queda vacio o eliminado — toda su logica esta distribuida en las nuevas clases |

**Criterio de aceptacion:** La GUI usa exclusivamente `SimulationService` para ejecutar simulaciones. No hay imports directos al modelo. Race condition eliminada.

---

### Fase 4: Desacoplar gui_main.cpp

**Objetivo:** Preparar gui_main.cpp para que sea reemplazable sin tocar dominio ni servicios.

| Tarea | Detalle |
|---|---|
| 4.1 | Extraer inicializacion SDL/ImGui/implot a clase `App` en `src/ui/app.h/cpp` |
| 4.2 | gui_main.cpp solo contiene: instanciar `ModelParams`, `VehicleCatalog`, `SquareLawModel`, `SimulationService`, `App`, y llamar `app.run()` |
| 4.3 | Los widgets actuales (`render_side_config`, `render_results`) se mueven a `src/ui/widgets/` como clases |
| 4.4 | `AppState` se reemplaza por `ScenarioConfig` + estado de UI minimo |
| 4.5 | Verificar que la GUI existente funciona identico con la nueva estructura |

**Criterio de aceptacion:** `gui_main.cpp` < 50 lineas. Toda la logica de rendering esta en `src/ui/`. La UI se puede reemplazar completamente sin tocar `src/domain/` ni `src/application/`.

---

### Fase 5: Calidad y CI

| Tarea | Detalle |
|---|---|
| 5.1 | GitHub Actions: build Linux nativo + cross-compile Windows + run tests en cada push |
| 5.2 | Capturar baseline numerico de los 9 escenarios como ficheros de referencia en `tests/data/` |
| 5.3 | Test de regresion automatizado que compara output contra baseline |
| 5.4 | Soporte para catalogos extensibles (cargar todos los JSON de un directorio `catalogs/`) |

**Criterio de aceptacion:** CI verde. Cualquier cambio que altere resultados numericos falla en CI.

---

## 5. Mapa de migracion: ficheros actuales → nuevas clases

| Codigo actual | Ubicacion actual | Destino |
|---|---|---|
| `g_model_params`, `ModelParams`, `load_model_params()`, `read_param()` | `lanchester_types.h`, `lanchester_io.h` | `src/domain/model_params.h/cpp` |
| `VehicleParams`, `VehicleCatalog`, `load_catalog()`, `find_vehicle()`, `vehicle_from_json()` | `lanchester_types.h`, `lanchester_io.h` | `src/domain/vehicle_catalog.h/cpp` |
| Structs: `CombatInput`, `CombatResult`, `AggregatedParams`, `EffectiveRates`, `TacticalMult`, enums | `lanchester_types.h` | `src/domain/types.h` |
| `kill_probability()`, `distance_degradation()`, `static_rate_*()`, `dynamic_rate_cc()`, `aggregate()`, `compute_effective_rates*()`, `total_rate()`, `initial_forces()`, `simulate_combat()`, `simulate_combat_stochastic()`, `run_montecarlo_combat()`, `compute_stats()`, `distribute_casualties_by_vulnerability()` | `lanchester_model.h` | `src/domain/square_law_model.h/cpp` |
| `get_tactical_multipliers()`, `terrain_fire_multiplier()`, `parse_mobility()`, `parse_terrain()`, `tactical_speed()`, `displacement_time_minutes()`, `outcome_str()`, `total_count()` | `lanchester_model.h` | `src/domain/types.h` (funciones de utilidad) |
| `run_scenario()`, `run_scenario_montecarlo()`, `run_batch()`, `run_sweep()`, `run_sensitivity()` | `lanchester_io.h` | `src/application/simulation_service.h/cpp` |
| `parse_composition()`, `validate_side_params()`, `validate_solver_params()` | `lanchester_io.h` | `src/application/scenario_config.h/cpp` |
| `result_to_json()`, `scenario_to_json()`, `mc_*_to_json()`, `stats_to_json()`, `write_csv_*()` | `lanchester_io.h` | `src/application/result_exporter.h/cpp` |
| `parse_json_path()`, `resolve_json_path()` | `lanchester_io.h` | `src/application/simulation_service.cpp` (privado) |
| `exe_directory()` | `lanchester_io.h` | `src/ui/app.cpp` (utilidad de la UI) |
| `build_scenario()`, `render_side_config()`, `render_results()`, `AppState`, bucle SDL | `gui_main.cpp` | `src/ui/app.h/cpp` + `src/ui/widgets/` |

---

## 6. Principios de diseño

1. **Cero globales mutables.** Todo estado se pasa explicitamente por constructor o parametro.
2. **Modelo desacoplado de la UI.** `ILanchesterModel` no sabe que ImGui existe.
3. **Datos por valor en hilos.** Nunca capturar por referencia en `std::async`.
4. **Tests como ciudadanos de primera clase.** Cada clase del dominio tiene tests unitarios.
5. **Retrocompatibilidad numerica.** Cada fase debe producir resultados identicos (epsilon=0.01).
6. **Compilacion nativa + cross.** CMake soporta Linux nativo (desarrollo/tests) y cross-compile Windows (distribucion).
7. **La UI es reemplazable.** Todo lo que esta en `src/ui/` se puede borrar y reescribir sin tocar `src/domain/` ni `src/application/`.

---

## 7. Dependencias

| Dependencia | Version actual | Uso | Cambio propuesto |
|---|---|---|---|
| nlohmann/json | header-only | I/O JSON | Mantener, mover a CMake FetchContent |
| Dear ImGui | 1.91.7 | GUI | Mantener (se movera a `src/ui/`, solo linkea contra target GUI) |
| implot | 0.16 | Graficas | Mantener (idem, solo en target GUI) |
| SDL2 | 2.30.10 | Ventana/eventos | Mantener (idem) |
| Catch2 | — | Tests | **Añadir** v3 header-only via FetchContent |

**Nota:** Las dependencias de GUI (ImGui, implot, SDL2) solo las necesita el target `lanchester_gui`. El target `lanchester_core` (library) y `lanchester_tests` no dependen de ellas.

---

## 8. Riesgos y mitigaciones

| Riesgo | Probabilidad | Impacto | Mitigacion |
|---|---|---|---|
| Regresion numerica al migrar | Media | Alto | Capturar baseline antes de fase 1. Tests de comparacion con epsilon |
| Complejidad de CMake cross-compile | Media | Medio | Mantener Makefile como fallback hasta CMake validado |
| Rotura de GUI al refactorizar | Media | Medio | Cada fase mantiene la GUI funcional. No se modifica la UI hasta PLAN_INTERFAZ |
| Over-engineering de la interfaz abstracta | Baja | Bajo | Solo crear `ILanchesterModel`. No añadir mas abstracciones hasta que se necesiten |

---

## Relacion con otros documentos

| Documento | Contenido |
|---|---|
| **PLAN_REFACTORIZACION.md** (este) | Arquitectura OOP, dominio, servicios, build, tests |
| **PLAN_INTERFAZ.md** | Diseño de la nueva interfaz grafica (wizard, pantalla 2D). Pendiente de definir |
| **DEUDA_TECNICA.md** | Registro historico de deuda tecnica (16/16 resueltos) |

---

*Documento de planificacion interna. Lanchester-CIO v2.*
*Creado: 2026-03-24*
