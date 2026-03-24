# Plan de Refactorizacion — Lanchester-CIO v2

## Objetivo

Transformar el prototipo actual (header-only, funciones libres, GUI monolitica) en una arquitectura orientada a objetos escalable, con una interfaz tipo wizard y una pantalla de presentacion 2D del resultado del combate.

Este documento define la arquitectura objetivo, las fases de migracion y los criterios de aceptacion de cada fase.

---

## 1. Estado actual y problemas que motivan la refactorizacion

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

### 1.2 Limitaciones para escalar

| Problema | Impacto |
|---|---|
| Global mutable `g_model_params` | No se pueden ejecutar simulaciones con parametros distintos en paralelo |
| Funciones libres sin encapsulacion | Dificil añadir variantes del modelo (Lanchester lineal, logaritmico, etc.) |
| GUI monolitica | Imposible cambiar la interfaz sin reescribir todo `gui_main.cpp` |
| Sin abstraccion de "pantalla" | No se puede implementar un wizard multi-paso |
| Sin capa de servicios | La GUI llama directamente a funciones de I/O y simulacion |
| Tests manuales | Sin framework de test ni validacion automatizada |

---

## 2. Arquitectura objetivo (OOP)

### 2.1 Diagrama de capas

```
┌─────────────────────────────────────────────────────────────┐
│                      CAPA DE PRESENTACION                   │
│                                                             │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌────────────┐  │
│  │ Screen   │  │ Screen   │  │ Screen   │  │ Screen     │  │
│  │ Terreno  │  │ Fuerzas  │  │ Params   │  │ Resultado  │  │
│  │ & Dist.  │  │ Azul/Rojo│  │ Avanzados│  │ 2D         │  │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └─────┬──────┘  │
│       └──────────────┴─────────────┴──────────────┘         │
│                          │                                  │
│                    WizardManager                            │
│                          │                                  │
├──────────────────────────┼──────────────────────────────────┤
│                   CAPA DE APLICACION                        │
│                          │                                  │
│                 SimulationService                           │
│              ┌───────────┼───────────┐                      │
│              │           │           │                       │
│        ScenarioBuilder  SimRunner  ResultExporter            │
│                          │                                  │
├──────────────────────────┼──────────────────────────────────┤
│                    CAPA DE DOMINIO                          │
│                          │                                  │
│              ┌───────────┼───────────┐                      │
│              │           │           │                       │
│       LanchesterModel  VehicleCatalog  ModelParams          │
│         (abstract)       (class)       (class)              │
│           │                                                 │
│     ┌─────┴──────┐                                          │
│     │            │                                          │
│  SquareLaw   LinearLaw   (futuras variantes)                │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

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
Permite futuras variantes del modelo sin tocar la GUI ni los servicios.

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
Contiene toda la logica de `lanchester_model.h` como metodos de clase. Recibe `ModelParams` por referencia constante en el constructor — sin globales.

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

    double killProbability(double D, double P) const;
    double distanceDegradation(double d, double f, double range) const;
    EffectiveRates computeRates(/* ... */) const;
    // ... funciones internas actuales convertidas a metodos privados
};
```

### 2.3 Capa de aplicacion (servicios)

#### `SimulationService`
Orquesta la ejecucion. Reemplaza las llamadas directas desde la GUI.

```cpp
class SimulationService {
public:
    SimulationService(std::shared_ptr<ILanchesterModel> model,
                      const VehicleCatalog& blueCat,
                      const VehicleCatalog& redCat);

    // Sincrono
    ScenarioOutput runScenario(const ScenarioConfig& config) const;
    MonteCarloOutput runMonteCarlo(const ScenarioConfig& config,
                                   int replicas, uint64_t seed) const;

    // Asincrono (devuelve future, sin race conditions)
    std::future<ScenarioOutput> runScenarioAsync(ScenarioConfig config) const;
    std::future<MonteCarloOutput> runMonteCarloAsync(ScenarioConfig config,
                                                      int replicas, uint64_t seed) const;

private:
    std::shared_ptr<ILanchesterModel> model_;
    VehicleCatalog blue_cat_;
    VehicleCatalog red_cat_;
};
```

**Nota**: Los metodos async reciben `ScenarioConfig` **por valor** (copia), eliminando la race condition actual.

#### `ScenarioConfig`
Struct de datos puros que reemplaza la construccion ad-hoc de JSON en la GUI.

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
};
```

### 2.4 Capa de presentacion (Wizard)

#### `IScreen` (interfaz de pantalla)

```cpp
class IScreen {
public:
    virtual ~IScreen() = default;

    virtual void render() = 0;
    virtual bool isComplete() const = 0;     // puede avanzar?
    virtual std::string title() const = 0;
    virtual void onEnter() {}                // al entrar en la pantalla
    virtual void onExit() {}                 // al salir de la pantalla
};
```

#### `WizardManager`
Controla la navegacion entre pantallas y mantiene el estado compartido del escenario.

```cpp
class WizardManager {
public:
    WizardManager(SimulationService& service);

    void addScreen(std::unique_ptr<IScreen> screen);
    void render();     // renderiza pantalla actual + barra de navegacion

    // Estado compartido (el wizard va construyendo el ScenarioConfig)
    ScenarioConfig& config();
    const ScenarioOutput* lastResult() const;
    const MonteCarloOutput* lastMCResult() const;

private:
    std::vector<std::unique_ptr<IScreen>> screens_;
    int current_ = 0;
    ScenarioConfig config_;
    SimulationService& service_;

    // Resultados (propiedad del wizard)
    std::optional<ScenarioOutput> result_;
    std::optional<MonteCarloOutput> mc_result_;

    void renderNavBar();   // [< Anterior]  Paso 2/5  [Siguiente >]
};
```

---

## 3. Diseño del Wizard (nueva interfaz)

### 3.1 Flujo de pantallas

```
 [1. Terreno]  ──>  [2. Fuerza Azul]  ──>  [3. Fuerza Roja]  ──>  [4. Parametros]  ──>  [5. Resultado 2D]
      │                    │                      │                      │                      │
  - Terreno            - Vehiculos            - Vehiculos           - Modo (det/MC)         - Mapa 2D
  - Distancia          - Cantidades           - Cantidades          - Agregacion            - Animacion
  - Movilidad          - Estado tactico       - Estado tactico      - t_max                 - Tablas
                       - Params avanzados     - Params avanzados    - Replicas/seed         - Graficas
```

### 3.2 Pantalla 1: Terreno y distancia

**Componentes:**
- Selector visual de terreno (3 iconos: llano, mixto, montañoso) con descripcion del efecto
- Slider de distancia de enfrentamiento (100m - 9000m) con escala visual
- Selectores de movilidad azul/rojo
- Preview: "Tiempo de desplazamiento estimado: X min"

**Datos que produce:** `config.terrain`, `config.distance_m`, `config.blue.mobility`, `config.red.mobility`

### 3.3 Pantalla 2: Fuerza azul

**Componentes:**
- Lista de vehiculos del catalogo con ficha descriptiva (al hacer hover/click)
- Para cada tipo seleccionado: slider de cantidad (1-50)
- Selector de estado tactico
- Panel colapsable "Parametros avanzados" (AFT, engagement fraction, factores)
- Resumen visual: barra con el total de vehiculos y composicion proporcional

**Datos que produce:** `config.blue`

### 3.4 Pantalla 3: Fuerza roja

Identico a pantalla 2 pero con catalogo rojo y colores rojos.

**Datos que produce:** `config.red`

### 3.5 Pantalla 4: Parametros de simulacion

**Componentes:**
- Selector de modo: Determinista / Monte Carlo
- Si MC: replicas (slider 100-10000) + seed
- Selector de agregacion: PRE / POST con explicacion breve
- t_max (slider 5-120 min)
- Resumen del escenario completo antes de ejecutar
- Boton grande: "EJECUTAR SIMULACION"

**Al pulsar ejecutar:** lanza la simulacion async y transiciona automaticamente a pantalla 5.

### 3.6 Pantalla 5: Resultado 2D (a detallar)

**Concepto inicial:**
- Vista de planta (2D top-down) del campo de batalla
- Iconos representando vehiculos azules y rojos
- Animacion temporal mostrando la progresion del combate (avance, bajas)
- Panel lateral con metricas clave (supervivientes, duracion, outcome)
- Controles de reproduccion: play/pause, velocidad, barra de tiempo

**Esta pantalla se detallara en un documento separado cuando se defina el diseño visual.**

---

## 4. Estructura de ficheros objetivo

```
src/
├── domain/
│   ├── types.h                    # Structs de datos (VehicleParams, CombatInput, etc.)
│   ├── model_params.h/cpp         # Clase ModelParams
│   ├── vehicle_catalog.h/cpp      # Clase VehicleCatalog
│   ├── lanchester_model.h         # Interfaz ILanchesterModel
│   └── square_law_model.h/cpp     # Implementacion SquareLawModel (RK4 + MC)
│
├── application/
│   ├── scenario_config.h/cpp      # ScenarioConfig + validacion
│   ├── simulation_service.h/cpp   # Orquestacion de simulaciones
│   └── result_exporter.h/cpp      # Serializacion JSON/CSV
│
├── ui/
│   ├── screen.h                   # Interfaz IScreen
│   ├── wizard_manager.h/cpp       # Navegacion del wizard
│   ├── screens/
│   │   ├── terrain_screen.h/cpp       # Pantalla 1
│   │   ├── blue_force_screen.h/cpp    # Pantalla 2
│   │   ├── red_force_screen.h/cpp     # Pantalla 3
│   │   ├── sim_params_screen.h/cpp    # Pantalla 4
│   │   └── result_2d_screen.h/cpp     # Pantalla 5
│   ├── widgets/
│   │   ├── vehicle_selector.h/cpp     # Widget reutilizable de seleccion de vehiculos
│   │   ├── force_summary_bar.h/cpp    # Barra visual de composicion
│   │   └── stats_table.h/cpp          # Tabla de resultados MC
│   └── app.h/cpp                  # Inicializacion SDL/ImGui, bucle principal
│
├── tests/
│   ├── test_main.cpp              # Entry point de tests (Catch2 o similar)
│   ├── test_square_law.cpp        # Tests unitarios del modelo
│   ├── test_scenario_config.cpp   # Tests de validacion
│   └── test_simulation_service.cpp
│
├── data/
│   ├── model_params.json
│   ├── vehicle_db.json
│   └── vehicle_db_en.json
│
├── main.cpp                       # Entry point de la aplicacion
├── CMakeLists.txt                 # Build system (reemplaza Makefile)
└── README.md
```

---

## 5. Fases de implementacion

### Fase 0: Preparacion (pre-requisito)

**Objetivo:** Infraestructura de build y testing sin romper la app actual.

| Tarea | Detalle |
|---|---|
| 0.1 | Migrar de Makefile a **CMake** con soporte para cross-compilacion MinGW y compilacion nativa Linux |
| 0.2 | Integrar framework de tests (**Catch2** header-only) |
| 0.3 | Crear estructura de directorios `src/` sin mover codigo aun |
| 0.4 | Verificar que la app compila y funciona identico tras la reorganizacion |

**Criterio de aceptacion:** `cmake --build . && ctest` funciona. La app GUI se genera igual que antes.

---

### Fase 1: Extraer el dominio (OOP core)

**Objetivo:** Convertir funciones libres en clases, eliminar globales.

| Tarea | Detalle |
|---|---|
| 1.1 | Crear clase `ModelParams` con factory `load()`. Mover logica de `load_model_params()` |
| 1.2 | Crear clase `VehicleCatalog` con `load()` y `find()`. Mover logica de `load_catalog()` y `find_vehicle()` |
| 1.3 | Crear interfaz `ILanchesterModel` y clase `SquareLawModel`. Mover toda la logica de `lanchester_model.h` a metodos de la clase. Recibir `ModelParams` en constructor |
| 1.4 | Eliminar `g_model_params` global. Pasar `ModelParams` por referencia |
| 1.5 | Crear `ScenarioConfig` como struct tipado (reemplaza construccion JSON ad-hoc en la GUI) |
| 1.6 | Crear `SimulationService` que orqueste la ejecucion |
| 1.7 | Escribir tests unitarios para `SquareLawModel` usando los 9 escenarios existentes como referencia |

**Criterio de aceptacion:** Todos los tests pasan. La GUI antigua sigue funcionando usando las nuevas clases internamente. Los resultados numericos son identicos al baseline.

---

### Fase 2: Desacoplar la GUI actual

**Objetivo:** Separar rendering de logica de negocio.

| Tarea | Detalle |
|---|---|
| 2.1 | Crear clase `App` que encapsule inicializacion SDL/ImGui/implot y el bucle de eventos |
| 2.2 | Extraer `build_scenario()` y mover a `ScenarioConfig::fromUIState()` |
| 2.3 | Reemplazar `std::async` con captura por referencia por `SimulationService::runAsync()` con captura por valor |
| 2.4 | Separar `render_side_config()` y `render_results()` en clases widget propias |

**Criterio de aceptacion:** La GUI funciona identico, pero el codigo esta organizado en `App`, widgets y servicios. Sin race conditions.

---

### Fase 3: Implementar el Wizard

**Objetivo:** Nueva interfaz de usuario tipo asistente paso a paso.

| Tarea | Detalle |
|---|---|
| 3.1 | Implementar `IScreen` y `WizardManager` con navegacion adelante/atras |
| 3.2 | Implementar `TerrainScreen` (pantalla 1) |
| 3.3 | Implementar `BlueForceScreen` (pantalla 2) con widget `VehicleSelector` |
| 3.4 | Implementar `RedForceScreen` (pantalla 3), reutilizando `VehicleSelector` |
| 3.5 | Implementar `SimParamsScreen` (pantalla 4) con resumen pre-ejecucion |
| 3.6 | Implementar `ResultScreen` basico (tablas + graficas, sin 2D aun) |
| 3.7 | Barra de progreso visual del wizard (paso actual resaltado) |

**Criterio de aceptacion:** El wizard completo permite configurar y ejecutar una simulacion. Los resultados son identicos a la GUI anterior.

---

### Fase 4: Pantalla de resultado 2D

**Objetivo:** Visualizacion de planta del campo de batalla con animacion.

| Tarea | Detalle |
|---|---|
| 4.1 | Diseñar el layout 2D (documento separado con mockups) |
| 4.2 | Implementar renderer 2D basico con ImGui/implot o OpenGL directo |
| 4.3 | Representar unidades como iconos posicionados en el espacio |
| 4.4 | Animar la progresion temporal (avance, bajas, desaparicion de unidades) |
| 4.5 | Controles de reproduccion (play/pause/velocidad/seek) |
| 4.6 | Panel lateral con metricas en tiempo real |

**Criterio de aceptacion:** Se detallara cuando se defina el diseño visual.

---

### Fase 5: Mejoras de calidad

| Tarea | Detalle |
|---|---|
| 5.1 | CI/CD con GitHub Actions (build + test en cada push) |
| 5.2 | Tests de regresion automatizados contra baseline numerico |
| 5.3 | Soporte para catalogos de vehiculos extensibles (carga desde directorio) |
| 5.4 | Exportar resultados desde la GUI (JSON/CSV) |
| 5.5 | Internacionalizacion (español/ingles) |

---

## 6. Principios de diseño a mantener

1. **Cero globales mutables.** Todo estado se pasa explicitamente.
2. **Modelo desacoplado de la UI.** `ILanchesterModel` no sabe que ImGui existe.
3. **Datos por valor en hilos.** Nunca capturar por referencia en `std::async`.
4. **Tests como ciudadanos de primera clase.** Cada clase del dominio tiene tests.
5. **Retrocompatibilidad numerica.** Cada fase debe producir resultados identicos a la anterior (dentro de epsilon).
6. **Compilacion nativa + cross.** CMake debe soportar compilacion Linux nativa (para desarrollo/testing) y cross-compilacion Windows (para distribucion).

---

## 7. Dependencias externas

| Dependencia | Version actual | Uso | Cambio propuesto |
|---|---|---|---|
| nlohmann/json | header-only | I/O JSON | Mantener, mover a CMake FetchContent |
| Dear ImGui | 1.91.7 | GUI | Mantener, mover a CMake FetchContent |
| implot | 0.16 | Graficas | Mantener |
| SDL2 | 2.30.10 | Ventana/eventos | Mantener |
| Catch2 | — | Tests | **Añadir** (header-only v3) |

---

## 8. Riesgos y mitigaciones

| Riesgo | Probabilidad | Impacto | Mitigacion |
|---|---|---|---|
| Regresion numerica al migrar | Media | Alto | Baseline de resultados antes de empezar. Tests de comparacion con epsilon |
| Complejidad de CMake cross-compile | Media | Medio | Mantener Makefile como fallback hasta que CMake este validado |
| Rendimiento del renderer 2D con ImGui | Baja | Medio | Limitar a ~200 unidades. Evaluar renderer OpenGL directo si necesario |
| Scope creep en la pantalla 2D | Alta | Alto | Diseñar en documento separado. Implementar version minima primero |

---

*Documento de planificacion interna. Lanchester-CIO v2.*
*Creado: 2026-03-24*
