# Deuda Tecnica — Modelo Lanchester-CIO

Registro de limitaciones conocidas, simplificaciones pendientes de resolver y mejoras necesarias antes de considerar el modelo apto para informar decisiones operativas.

Prioridades:
- **P0 (Critica)**: Invalida resultados o impide uso fiable.
- **P1 (Alta)**: Sesgo sistematico o limitacion estructural importante.
- **P2 (Media)**: Mejora de calidad, mantenibilidad o usabilidad.
- **P3 (Baja)**: Limpieza, conveniencia, minor.

---

## Resumen por prioridad

| Prioridad | Total | Resueltos | Parciales | Pendientes |
|---|---|---|---|---|
| P0 (Critica) | 2 | 2 | 0 | 0 |
| P1 (Alta) | 5 | 5 | 0 | 0 |
| P2 (Media) | 6 | 3 | 3 | 0 |
| P3 (Baja) | 3 | 3 | 0 | 0 |
| **Nuevo** | 4 | 0 | 0 | 4 |

**Total original: 16 — 13 resueltos, 3 parciales.**
**Deuda nueva: 4 items pendientes (DT-017 a DT-020).**

---

## Items resueltos

### DT-001 — Integrador numerico Euler (RK1) — RESUELTO

| Campo | Valor |
|---|---|
| Prioridad | **P0** |
| Estado | **RESUELTO** |

Implementado RK4 en `simulate_combat()`. Error validado: 0.051 vehiculos vs solucion analitica cerrada (test_09_analytical.json).

---

### DT-003 — Modelo determinista aplicado a escalas pequenas — RESUELTO

| Campo | Valor |
|---|---|
| Prioridad | **P1** |
| Estado | **RESUELTO** |

Implementado modo Monte Carlo con bajas discretas Poisson. Subdivision automatica si lambda > 2. Reproducibilidad con semilla. Integrado en GUI.

---

### DT-004 — No se modela destruccion selectiva por tipo — RESUELTO

| Campo | Valor |
|---|---|
| Prioridad | **P1** |
| Estado | **RESUELTO** |

Implementada `distribute_casualties_by_vulnerability()` ponderando por inverso de D_cc.

---

### DT-005 — Distancia fija durante el combate — RESUELTO

| Campo | Valor |
|---|---|
| Prioridad | **P1** |
| Estado | **RESUELTO** |

Distancia variable `d(t) = max(50, d0 - v_approach * t)`. Tasas recalculadas en cada paso RK4.

---

### DT-006 — Terreno no afecta efectividad del fuego — RESUELTO

| Campo | Valor |
|---|---|
| Prioridad | **P1** |
| Estado | **RESUELTO** |

`terrain_fire_multiplier()` aplica FACIL=1.0, MEDIO=0.85, DIFICIL=0.65 sobre tasa convencional. Valores en `model_params.json`.

---

### DT-007 — Modelo de municion C/C inconsistente — RESUELTO

| Campo | Valor |
|---|---|
| Prioridad | **P1** |
| Estado | **RESUELTO** |

Reescrito `dynamic_rate_cc()` con municion total (`M * n_cc_initial`) y tracking de consumo acumulado.

---

### DT-009 — Variable global mutable para modo de agregacion — RESUELTO

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Estado | **RESUELTO** |

`g_aggregation_mode` eliminado. `AggregationMode` se pasa como campo de `CombatInput` y parametro explicito.

---

### DT-010 — `std::exit(1)` en busqueda de vehiculos — RESUELTO

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Estado | **RESUELTO** |

`find_vehicle()` ahora lanza `std::runtime_error`.

---

### DT-011 — Sin validacion de parametros de entrada — RESUELTO

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Estado | **RESUELTO** |

Implementadas `parse_composition()`, `validate_side_params()`, `validate_solver_params()`.

---

### DT-012 — Redondeo a 2 decimales pierde precision — RESUELTO

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Estado | **RESUELTO** |

Salida JSON con 6 decimales por defecto.

---

### DT-013 — Tests no verifican contra soluciones analiticas — RESUELTO

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Estado | **RESUELTO** |

`test_09_analytical.json` + test automatizado Catch2 en `test_square_law.cpp`. Error: 0.051 vehiculos.

---

### DT-014 — `exe_directory()` no robusto — RESUELTO

| Campo | Valor |
|---|---|
| Prioridad | **P3** |
| Estado | **RESUELTO** |

`GetModuleFileNameA()` en Windows, `argv[0]` como fallback POSIX.

---

### DT-015 — Sweep con comparacion de punto flotante — RESUELTO

| Campo | Valor |
|---|---|
| Prioridad | **P3** |
| Estado | **RESUELTO** |

Bucle con contador entero: `double val = start + i * step`.

---

### DT-016 — Include fuera de zona — RESUELTO

| Campo | Valor |
|---|---|
| Prioridad | **P3** |
| Estado | **RESUELTO** |

Includes organizados correctamente.

---

## Items parcialmente resueltos

### DT-002 — Constantes magicas sin calibracion ni referencia — PARCIAL

| Campo | Valor |
|---|---|
| Prioridad | **P0** |
| Estado | **PARCIAL** |

**Lo resuelto:**
- Constantes externalizadas a `model_params.json` con metadatos de origen.
- Clase `ModelParamsClass` (`src/domain/model_params.h`) encapsula la carga con accessors tipados.

**Lo que falta:**
- `g_model_params` (global mutable) **sigue existiendo** y activo. Declarado extern en `lanchester_types.h:38`, definido en `gui_main.cpp:28`, usado por las funciones inline de `lanchester_model.h` (lineas 17, 22, 54, 90-92).
- `ModelParamsClass` coexiste con el struct `ModelParams` legacy. Tiene un metodo puente `applyToGlobal()` que copia sus datos al global.
- La calibracion contra datos reales sigue pendiente (requiere datos de referencia externos).

---

### DT-008 — Monolito en un solo archivo — PARCIAL

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Estado | **PARCIAL** |

**Lo resuelto:**
- Estructura de directorios `src/domain/`, `src/application/`, `src/ui/`, `src/tests/`.
- Clases OOP creadas: `ModelParamsClass`, `VehicleCatalogClass`, `SquareLawModel`, `SimulationService`, `ScenarioConfig`.
- 28 tests automatizados Catch2.

**Lo que falta:**
- Los headers legacy **siguen existiendo con su contenido original**:
  - `lanchester_model.h`: 710 lineas de funciones inline que usan `g_model_params`
  - `lanchester_io.h`: 985 lineas con orquestacion, parseo, batch, sweep, serializacion
- Hay duplicacion de logica: `SquareLawModel` reimplementa las funciones de `lanchester_model.h`, pero ambas coexisten.
- `SimulationService` delega en las funciones legacy de `lanchester_io.h` (`run_scenario()`, `run_scenario_montecarlo()`), no tiene implementacion propia.
- `gui_main.cpp` (699 lineas) crea un `SimulationService` pero **no lo usa** — llama directamente a las funciones legacy.

---

### DT-008b — GUI acoplada al motor (derivado de DT-008) — PARCIAL

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Estado | **PARCIAL** |

**Lo resuelto:**
- `SimulationService` existe como API de orquestacion.
- `ScenarioConfig` existe como contrato de datos.

**Lo que falta:**
- `gui_main.cpp` ignora `SimulationService` y construye JSON manualmente para llamar a `run_scenario()` legacy.
- No hay `IScreen`, `WizardManager` ni widgets extraidos.
- `gui_main.cpp` tiene 699 lineas (objetivo era <50).

---

## Deuda tecnica nueva

### DT-017 — Global mutable `g_model_params` no eliminado

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Estado | **PENDIENTE** |
| Origen | Migracion OOP incompleta (fase 1 de PLAN_REFACTORIZACION) |

`ModelParamsClass` existe pero no reemplazo al global. Las funciones inline de `lanchester_model.h` siguen leyendo `g_model_params` directamente. `SimulationService` llama a `params_.applyToGlobal()` como puente.

**Para resolver:** Que `SquareLawModel` sea el unico punto de calculo (ya lo es en tests). Eliminar las funciones inline de `lanchester_model.h` que dependen del global. Hacer que `gui_main.cpp` use `SimulationService` en lugar de llamar a legacy.

---

### DT-018 — SimulationService es un wrapper vacio

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Estado | **PENDIENTE** |
| Origen | Migracion OOP incompleta (fase 3 de PLAN_REFACTORIZACION) |

`SimulationService::runScenario()` convierte `ScenarioConfig` a JSON y llama a `run_scenario()` de `lanchester_io.h`. No usa `SquareLawModel` directamente. Es un adaptador, no un servicio real.

**Para resolver:** `SimulationService` debe usar `SquareLawModel` directamente, construyendo `CombatInput` desde `ScenarioConfig` sin pasar por JSON.

---

### DT-019 — GUI ignora la capa de servicios

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Estado | **PENDIENTE** |
| Origen | Fase 4 de PLAN_REFACTORIZACION no ejecutada |

`gui_main.cpp` construye `SimulationService` pero lo ignora. Llama a funciones legacy (`run_scenario()`, `run_scenario_montecarlo()`) directamente en lambdas async con captura por referencia (la race condition que la refactorizacion pretendia eliminar).

**Para resolver:** `gui_main.cpp` debe usar `SimulationService::runScenarioAsync()` / `runMonteCarloAsync()` (que capturan por valor).

---

### DT-020 — Duplicacion de logica dominio legacy / OOP

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Estado | **PENDIENTE** |
| Origen | Coexistencia de dos implementaciones del modelo |

`SquareLawModel` (square_law_model.cpp) y `lanchester_model.h` implementan la misma logica matematica por separado. Los tests Catch2 validan `SquareLawModel`. La GUI usa `lanchester_model.h` via `lanchester_io.h`. Si se corrige un bug en uno, el otro queda desactualizado.

**Para resolver:** Eliminar `lanchester_model.h` como codigo activo. Que `lanchester_io.h` (o su reemplazo) use `SquareLawModel` como motor de calculo.

---

## Dependencias entre items pendientes

```
DT-020 (eliminar duplicacion)
  └──> DT-017 (eliminar g_model_params)
         └──> DT-018 (SimulationService usa SquareLawModel)
                └──> DT-019 (GUI usa SimulationService)
```

El orden natural de resolucion es: DT-020 → DT-017 → DT-018 → DT-019.

---

*Ultima actualizacion: 2026-03-25.*
