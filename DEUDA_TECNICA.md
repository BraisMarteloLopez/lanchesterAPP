# Deuda Tecnica — Modelo Lanchester-CIO

Registro de limitaciones conocidas y mejoras pendientes.

Prioridades:
- **P0 (Critica)**: Invalida resultados o impide uso fiable.
- **P1 (Alta)**: Sesgo sistematico o limitacion estructural importante.
- **P2 (Media)**: Mejora de calidad, mantenibilidad o usabilidad.
- **P3 (Baja)**: Limpieza, conveniencia, minor.

---

## Resumen

| ID | Prioridad | Estado | Descripcion |
|---|---|---|---|
| DT-002 | P0 | Parcial | Constantes sin calibrar + global mutable activo |
| DT-008 | P2 | Parcial | Headers legacy internos no eliminados |
| DT-008b | P2 | Parcial | GUI acoplada al motor |
| DT-017 | P2 | Pendiente | Global mutable `g_model_params` no eliminado |
| DT-018 | P2 | Pendiente | SimulationService es un wrapper vacio |
| DT-019 | P2 | Pendiente | GUI ignora la capa de servicios |
| DT-020 | P2 | Pendiente | Duplicacion de logica dominio legacy / OOP |

---

## Items parciales

### DT-002 — Constantes sin calibrar + global mutable activo

| Campo | Valor |
|---|---|
| Prioridad | **P0** |
| Estado | **PARCIAL** |

**Lo resuelto:**
- Constantes externalizadas a `model_params.json` con metadatos de origen.
- Clase `ModelParamsClass` (`src/domain/model_params.h`) encapsula la carga con accessors tipados.

**Lo que falta:**
- `g_model_params` (global mutable) sigue activo. Declarado extern en `lanchester_types.h`, definido en `gui_main.cpp`, usado por las funciones inline de `lanchester_model.h`.
- `ModelParamsClass` coexiste con el struct `ModelParams` legacy. Tiene un metodo puente `applyToGlobal()`.
- La calibracion contra datos reales sigue pendiente (requiere datos de referencia externos).

---

### DT-008 — Headers legacy internos no eliminados

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Estado | **PARCIAL** |

**Lo resuelto:**
- Estructura de directorios `src/domain/`, `src/application/`, `src/ui/`, `src/tests/`.
- Clases OOP creadas: `ModelParamsClass`, `VehicleCatalogClass`, `SquareLawModel`, `SimulationService`, `ScenarioConfig`.
- 28 tests automatizados Catch2.

**Lo que falta:**
- `src/domain/lanchester_model.h`: 710 lineas de funciones inline que usan `g_model_params`.
- `src/application/lanchester_io.h`: 985 lineas con orquestacion, parseo, batch, sweep, serializacion.
- `SquareLawModel` reimplementa las funciones de `lanchester_model.h`, pero ambas coexisten.
- `SimulationService` delega en las funciones legacy, no tiene implementacion propia.

---

### DT-008b — GUI acoplada al motor

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Estado | **PARCIAL** |

**Lo resuelto:**
- `SimulationService` existe como API de orquestacion.
- `ScenarioConfig` existe como contrato de datos.

**Lo que falta:**
- `gui_main.cpp` ignora `SimulationService` y construye JSON manualmente para llamar a `run_scenario()` legacy.
- No hay widgets extraidos.
- `gui_main.cpp` tiene 699 lineas.

---

## Items pendientes

### DT-017 — Global mutable `g_model_params` no eliminado

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Estado | **PENDIENTE** |

`ModelParamsClass` existe pero no reemplaza al global. Las funciones inline de `lanchester_model.h` siguen leyendo `g_model_params` directamente. `SimulationService` llama a `params_.applyToGlobal()` como puente.

**Para resolver:** Que `SquareLawModel` sea el unico punto de calculo. Eliminar las funciones inline de `lanchester_model.h` que dependen del global. Hacer que `gui_main.cpp` use `SimulationService`.

---

### DT-018 — SimulationService es un wrapper vacio

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Estado | **PENDIENTE** |

`SimulationService::runScenario()` convierte `ScenarioConfig` a JSON y llama a `run_scenario()` de `lanchester_io.h`. No usa `SquareLawModel` directamente.

**Para resolver:** `SimulationService` debe usar `SquareLawModel` directamente, construyendo `CombatInput` desde `ScenarioConfig` sin pasar por JSON.

---

### DT-019 — GUI ignora la capa de servicios

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Estado | **PENDIENTE** |

`gui_main.cpp` construye `SimulationService` pero lo ignora. Llama a funciones legacy directamente en lambdas async con captura por referencia.

**Para resolver:** `gui_main.cpp` debe usar `SimulationService::runScenarioAsync()` / `runMonteCarloAsync()`.

---

### DT-020 — Duplicacion de logica dominio legacy / OOP

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Estado | **PENDIENTE** |

`SquareLawModel` y `lanchester_model.h` implementan la misma logica matematica por separado. Los tests validan `SquareLawModel`. La GUI usa `lanchester_model.h` via `lanchester_io.h`.

**Para resolver:** Eliminar `lanchester_model.h` como codigo activo. Que `lanchester_io.h` (o su reemplazo) use `SquareLawModel`.

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

*Ultima actualizacion: 2026-03-26.*
