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
| DT-002 | P0 | Parcial | Constantes sin calibrar |
| DT-008 | P2 | Resuelto | Arquitectura OOP completa |
| DT-008b | P2 | Resuelto | GUI desacoplada del motor |
| DT-017 | P2 | Resuelto | Global mutable eliminado |
| DT-018 | P2 | Resuelto | SimulationService funcional |
| DT-019 | P2 | Resuelto | GUI usa SimulationService |
| DT-020 | P2 | Resuelto | Duplicacion eliminada |

---

## Items pendientes

### DT-002 — Constantes sin calibrar

| Campo | Valor |
|---|---|
| Prioridad | **P0** |
| Estado | **PARCIAL** |

**Lo resuelto:**
- Constantes externalizadas a `data/model_params.json` con metadatos de origen.
- Clase `ModelParamsClass` encapsula la carga con accessors tipados e inmutables.
- Sin global mutable — parametros inyectados via constructor.

**Lo que falta:**
- La calibracion contra datos reales sigue pendiente (requiere datos de referencia externos al proyecto).

---

## Items resueltos

### DT-008 — Arquitectura OOP completa

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Estado | **RESUELTO** |

- Estructura de directorios `src/domain/`, `src/application/`, `src/ui/`, `src/tests/`.
- Clases OOP: `ModelParamsClass`, `VehicleCatalogClass`, `SquareLawModel`, `SimulationService`, `ScenarioConfig`.
- 26 tests automatizados Catch2.
- Headers legacy (`lanchester_model.h`, `lanchester_io.h`) eliminados.
- Sin duplicacion de logica de dominio.

---

### DT-008b — GUI desacoplada del motor

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Estado | **RESUELTO** |

- `gui_main.cpp` usa `SimulationService::runScenarioAsync()` / `runMonteCarloAsync()`.
- `ScenarioConfig` tipado como contrato de datos (sin JSON intermedio).
- GUI no incluye ningun header de dominio directamente excepto a traves de `SimulationService`.

---

### DT-017 — Global mutable eliminado

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Estado | **RESUELTO** |

- `g_model_params` eliminado (extern, definiciones, y `applyToGlobal()`).
- `SquareLawModel` almacena `ModelParamsClass` por valor (inyeccion de dependencias).

---

### DT-018 — SimulationService funcional

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Estado | **RESUELTO** |

- `SimulationService` construye `CombatInput` desde `ScenarioConfig` directamente.
- Usa `model_->simulate()` / `runMonteCarlo()` sin puente JSON.
- Ejecucion asincrona con captura por valor (sin race conditions).

---

### DT-019 — GUI usa SimulationService

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Estado | **RESUELTO** |

- Boton EJECUTAR llama a `service->runScenarioAsync()` / `runMonteCarloAsync()`.
- `buildScenarioConfig()` reemplaza la construccion manual de JSON.
- Catalogos accedidos via `service->blueCatalog()` / `redCatalog()`.

---

### DT-020 — Duplicacion eliminada

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Estado | **RESUELTO** |

- `lanchester_model.h` (711 lineas) eliminado. Toda la logica matematica esta unicamente en `SquareLawModel`.
- `lanchester_io.h` (986 lineas) eliminado. Orquestacion unicamente en `SimulationService`.
- Tests migrados a clases OOP.

---

*Ultima actualizacion: 2026-03-26.*
