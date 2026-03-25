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

### Fase 0: Infraestructura de build y tests — COMPLETADA

**Objetivo:** CMake + Catch2 + estructura de directorios, sin mover logica aun.

| Tarea | Detalle | Estado |
|---|---|---|
| 0.1 | Crear `CMakeLists.txt` con targets: `lanchester_core` (library), `lanchester_gui` (executable), `lanchester_tests` (test executable) | Hecho |
| 0.2 | Configurar cross-compilacion MinGW en CMake (`cmake/mingw-w64-toolchain.cmake`) | Hecho |
| 0.3 | Integrar Catch2 v3 header-only via FetchContent | Hecho |
| 0.4 | Crear estructura de directorios `src/domain/`, `src/application/`, `src/tests/` | Hecho |
| 0.5 | Mover ficheros actuales a `src/` sin cambiar su contenido (solo paths de include) | Hecho |
| 0.6 | Verificar: `cmake --build .` genera el .exe, `ctest` ejecuta 28 tests | Hecho |

---

### Fase 1: Dominio OOP — ModelParams y VehicleCatalog — PARCIAL

**Objetivo:** Extraer las dos primeras clases, eliminar el global.

| Tarea | Detalle | Estado |
|---|---|---|
| 1.1 | Crear `ModelParamsClass` con factory `load()` (`src/domain/model_params.h/cpp`) | Hecho |
| 1.2 | Crear `VehicleCatalogClass` con `load()`, `find()`, `names()`, `contains()` (`src/domain/vehicle_catalog.h/cpp`) | Hecho |
| 1.3 | Eliminar `extern ModelParams g_model_params` | **NO HECHO** — el global sigue activo en 9 ficheros. `ModelParamsClass` tiene un puente `applyToGlobal()` |
| 1.4 | Adaptar GUI para recibir dependencias inyectadas | Parcial — GUI crea `SimulationService` pero no lo usa |
| 1.5 | Tests unitarios para ambas clases | Hecho (test_model_params.cpp, test_vehicle_catalog.cpp) |

**Criterio de aceptacion original:** "`g_model_params` ya no existe" — **NO CUMPLIDO.**

**Deuda asociada:** DT-017, DT-020 en `DEUDA_TECNICA.md`.

---

### Fase 2: Dominio OOP — ILanchesterModel y SquareLawModel — PARCIAL

**Objetivo:** Encapsular el motor de simulacion en una clase con interfaz abstracta.

| Tarea | Detalle | Estado |
|---|---|---|
| 2.1 | Definir interfaz `ILanchesterModel` (`src/domain/lanchester_model_iface.h`) | Hecho |
| 2.2 | Implementar `SquareLawModel` con toda la logica matematica (`src/domain/square_law_model.h/cpp`) | Hecho |
| 2.3 | `SquareLawModel` usa `params_` en lugar de `g_model_params` | Hecho |
| 2.4 | Helpers convertidos a metodos o utilidades | Hecho |
| 2.5 | Tests contra baseline (test_square_law.cpp) | Hecho |
| 2.6 | Test analitico (incluido en test_square_law.cpp) | Hecho |

**Criterio de aceptacion original:** "Cero funciones libres de simulacion" — **NO CUMPLIDO.** Las funciones inline de `lanchester_model.h` (710 lineas) siguen existiendo y son usadas por `lanchester_io.h` y `gui_main.cpp`. `SquareLawModel` es una reimplementacion paralela, no un reemplazo.

**Deuda asociada:** DT-020 en `DEUDA_TECNICA.md`.

---

### Fase 3: Capa de aplicacion — SimulationService y ScenarioConfig — PARCIAL

**Objetivo:** Crear la capa de servicios que desacopla GUI del motor.

| Tarea | Detalle | Estado |
|---|---|---|
| 3.1 | Crear `ScenarioConfig` con `validate()`, `fromJson()`, `toJson()` | Hecho |
| 3.2 | `SimulationService` con `runScenario()` y `runMonteCarlo()` | Parcial — delega en funciones legacy via `applyToGlobal()` + `run_scenario()` |
| 3.3 | `runScenarioAsync()` y `runMonteCarloAsync()` con captura por valor | Hecho (la interfaz existe, captura por valor) |
| 3.4 | Crear `ResultExporter` | **NO HECHO** — no existe |
| 3.5 | Mover batch/sweep/sensitivity al servicio | **NO HECHO** — siguen en `lanchester_io.h` |
| 3.6 | Tests de integracion (test_simulation_service.cpp) | Hecho |
| 3.7 | `lanchester_io.h` eliminado | **NO HECHO** — sigue con 985 lineas |

**Criterio de aceptacion original:** "La GUI usa exclusivamente SimulationService" — **NO CUMPLIDO.** La GUI ignora el servicio.

**Deuda asociada:** DT-018, DT-019 en `DEUDA_TECNICA.md`.

---

### Fase 4: Desacoplar gui_main.cpp — NO INICIADA

**Objetivo:** Preparar gui_main.cpp para que sea reemplazable sin tocar dominio ni servicios.

| Tarea | Detalle | Estado |
|---|---|---|
| 4.1 | Extraer inicializacion SDL/ImGui/implot a clase `App` | No iniciada |
| 4.2 | gui_main.cpp reducido a <50 lineas | No iniciada (actualmente 699 lineas) |
| 4.3 | Widgets extraidos a `src/ui/widgets/` | No iniciada |
| 4.4 | `AppState` reemplazado por `ScenarioConfig` + estado UI minimo | No iniciada |
| 4.5 | Verificar GUI funcional con nueva estructura | No iniciada |

**Prerequisito:** Completar fases 1-3 primero (eliminar dependencia de legacy).

---

### Fase 5: Calidad y CI — NO INICIADA

| Tarea | Detalle | Estado |
|---|---|---|
| 5.1 | GitHub Actions: build + cross-compile + tests | No iniciada |
| 5.2 | Baseline numerico como ficheros de referencia | No iniciada |
| 5.3 | Test de regresion automatizado contra baseline | No iniciada |
| 5.4 | Catalogos extensibles (directorio `catalogs/`) | No iniciada |

**Prerequisito:** Completar fase 4.

---

### Resumen de fases

| Fase | Titulo | Estado |
|---|---|---|
| 0 | Infraestructura de build y tests | **Completada** |
| 1 | ModelParams y VehicleCatalog | **Parcial** — clases creadas, global no eliminado |
| 2 | ILanchesterModel y SquareLawModel | **Parcial** — clase creada, legacy no eliminado |
| 3 | SimulationService y ScenarioConfig | **Parcial** — wrapper sobre legacy, no usa SquareLawModel |
| 4 | Desacoplar gui_main.cpp | **No iniciada** |
| 5 | Calidad y CI | **No iniciada** |

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
| **DEUDA_TECNICA.md** | Registro de deuda tecnica (13/16 resueltos + 4 nuevos pendientes) |
| **PLAN_DE_PRUEBAS.md** | Guia de pruebas manuales en Windows |

---

*Documento de planificacion interna. Lanchester-CIO v2.*
*Creado: 2026-03-24. Actualizado: 2026-03-25 (estado real por fase).*
