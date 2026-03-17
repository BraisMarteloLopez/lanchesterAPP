# Checkpoint - Estado Pendiente de la Solución

**Fecha**: 2026-03-17
**Rama**: `claude/checkpoint-pending-state-DksTf`

---

## Estado Completado

| Área | Estado |
|------|--------|
| Motor de simulación Lanchester (integrador RK4) | Completo |
| Modo determinista (distancia variable, terreno, multiplicadores tácticos) | Completo |
| Modo Monte Carlo (Poisson, semilla, percentiles, encadenado) | Completo |
| GUI Windows (Dear ImGui + SDL2 + implot) | Completo |
| Deuda técnica (16/16 ítems resueltos) | Completo |
| Tests de validación (9 escenarios, incluido analítico) | Completo |
| Parámetros externalizados en JSON (`model_params.json`) | Completo |
| Base de datos de vehículos (NATO + OPFOR) | Completo |
| Documentación (README, PLAN, DEUDA_TECNICA, PLAN_DE_PRUEBAS) | Completo |
| Release empaquetado (`.exe` + `SDL2.dll` + JSONs) | Completo |

## Pendiente

| # | Tarea | Prioridad | Bloqueante |
|---|-------|-----------|------------|
| 1 | **Calibración empírica** de `model_params.json` con datos reales (ejercicios, JANUS, doctrina) | Alta | Sí - esperando datos de referencia |
| 2 | Exponer batch/sweep en la GUI (actualmente solo vía JSON) | Media | No |
| 3 | Exponer análisis de sensibilidad en la GUI | Media | No |
| 4 | Validar multiplicadores tácticos contra doctrina OTAN | Media | No |
| 5 | Optimización de rendimiento para Monte Carlo >1000 réplicas | Baja | No |

## Resumen

El motor de simulación y la GUI están **completos y funcionales**. Todas las deudas técnicas están resueltas. El único bloqueo real es la **calibración empírica** de los parámetros del modelo (`model_params.json`), que requiere datos de referencia externos.

## Estructura del Proyecto

```
lanchester_types.h    - Estructuras de datos y enums (~175 líneas)
lanchester_model.h    - Motor matemático, RK4, Monte Carlo (~710 líneas)
lanchester_io.h       - I/O JSON, batch, sweep, sensibilidad (~970 líneas)
gui_main.cpp          - Aplicación GUI Dear ImGui + SDL2 (~700 líneas)
model_params.json     - Parámetros calibrables
vehicle_db.json       - Vehículos NATO
vehicle_db_en.json    - Vehículos OPFOR
tests/                - 9 escenarios de validación
ejemplos/             - Escenarios de ejemplo
release/              - Ejecutable Windows empaquetado
```
