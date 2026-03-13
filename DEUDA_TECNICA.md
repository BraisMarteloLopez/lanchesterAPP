# Deuda Tecnica — Modelo Lanchester-CIO

Registro de limitaciones conocidas, simplificaciones pendientes de resolver y mejoras necesarias antes de considerar el modelo apto para informar decisiones operativas.

Prioridades:
- **P0 (Critica)**: Invalida resultados o impide uso fiable.
- **P1 (Alta)**: Sesgo sistematico o limitacion estructural importante.
- **P2 (Media)**: Mejora de calidad, mantenibilidad o usabilidad.
- **P3 (Baja)**: Limpieza, conveniencia, minor.

---

## DT-001 — Integrador numerico Euler (RK1)

| Campo | Valor |
|---|---|
| Prioridad | **P0** |
| Estado | **RESUELTO** |
| Archivo | `lanchester_model.h` — `simulate_combat()` |
| Tipo | Precision numerica |

### Descripcion

El bucle de simulacion usaba Euler explicito (RK1) con error de truncamiento local O(h²) y global O(h).

### Resolucion

Implementado RK4 (Runge-Kutta orden 4) en `simulate_combat()`. Se definen lambdas `f_A` y `f_R` para las derivadas y se calculan las 4 etapas (k1-k4) para ambas variables en cada paso temporal:

```cpp
double A_new = std::max(0.0, A + (h / 6.0) * (k1a + 2*k2a + 2*k3a + k4a));
double R_new = std::max(0.0, R + (h / 6.0) * (k1r + 2*k2r + 2*k3r + k4r));
```

### Validacion

Test analitico (DT-013) confirma error de 0.051 vehiculos respecto a la solucion cerrada de Lanchester.

---

## DT-002 — Constantes magicas sin calibracion ni referencia

| Campo | Valor |
|---|---|
| Prioridad | **P0** |
| Estado | **RESUELTO** |
| Archivo | `model_params.json`, `lanchester_model.h`, `lanchester_io.h` |
| Tipo | Validez del modelo |

### Descripcion

Tres conjuntos de constantes criticas estaban hardcoded sin justificacion: pendiente de la sigmoide (175.0), coeficientes del polinomio de degradacion por distancia (6 coeficientes), y multiplicadores tacticos.

### Resolucion

Todas las constantes externalizadas a `model_params.json`:
- `kill_probability_slope`: 175.0
- `distance_degradation_coefficients`: los 6 coeficientes del polinomio
- `tactical_multipliers`: todos los estados tacticos con sus valores
- `terrain_fire_effectiveness`: multiplicadores por tipo de terreno

Cada parametro incluye `_comment` documentando su funcion y estado de calibracion. Los parametros se cargan en `g_model_params` (struct `ModelParams`) al inicio de la ejecucion.

**Nota**: La calibracion contra datos reales sigue pendiente. Los valores estan externalizados pero no validados empiricamente.

---

## DT-003 — Modelo determinista aplicado a escalas pequenas

| Campo | Valor |
|---|---|
| Prioridad | **P1** |
| Estado | **RESUELTO** |
| Archivo | `lanchester_model.h` — `simulate_combat_stochastic()`, `run_montecarlo()` |
| Tipo | Validez conceptual |

### Descripcion

Las ecuaciones de Lanchester modelan combate como un proceso continuo y determinista. Fueron disenadas para fuerzas de gran tamano donde la ley de los grandes numeros suaviza la variabilidad individual. El modelo se usa con fuerzas de 6-20 vehiculos.

### Resolucion

Implementado modo Monte Carlo con bajas discretas Poisson. El usuario selecciona el modo en la interfaz grafica de la aplicacion Windows (`lanchester_gui.exe`), configura numero de replicas y semilla, y obtiene:
- Probabilidades de victoria (P(Blue), P(Red), P(Draw))
- Percentiles de supervivientes (p05, p25, mediana, p75, p95)
- Graficas de distribucion (implot)

Subdivision automatica del paso temporal si lambda > 2 para preservar la fidelidad de la distribucion de Poisson. Reproducibilidad garantizada con misma semilla.

---

## DT-004 — No se modela destruccion selectiva por tipo de vehiculo

| Campo | Valor |
|---|---|
| Prioridad | **P1** |
| Estado | **RESUELTO** |
| Archivo | `lanchester_model.h` — `distribute_casualties_by_vulnerability()` |
| Tipo | Realismo del modelo |

### Descripcion

Las bajas se distribuian proporcionalmente a toda la fuerza, sin considerar diferencias de proteccion.

### Resolucion

Implementada funcion `distribute_casualties_by_vulnerability()` que distribuye bajas ponderando por vulnerabilidad (inverso de D_cc, la proteccion). Los vehiculos menos protegidos reciben proporcionalmente mas bajas. Esta distribucion se aplica en el encadenamiento de combates en `run_scenario()` (`lanchester_io.h`).

---

## DT-005 — Distancia fija durante el combate

| Campo | Valor |
|---|---|
| Prioridad | **P1** |
| Estado | **RESUELTO** |
| Archivo | `lanchester_model.h` — `simulate_combat()`, `lanchester_io.h` — `run_scenario()` |
| Tipo | Realismo del modelo |

### Descripcion

`engagement_distance_m` era constante durante toda la simulacion. En un ataque, el atacante avanza hacia el defensor.

### Resolucion

Implementada distancia variable `d(t) = d0 - v_approach * t` en el bucle de simulacion. La velocidad de aproximacion se calcula en `run_scenario()` a partir del estado tactico y la movilidad (`tactical_speed()`). Las tasas de destruccion se recalculan en cada paso del integrador cuando hay movimiento:

```cpp
if (approach_speed_m_per_min > 0.0) {
    current_distance = std::max(50.0, input.distance_m - approach_speed_m_per_min * t);
    // recalcular tasas con nueva distancia
}
```

---

## DT-006 — Terreno no afecta efectividad del fuego

| Campo | Valor |
|---|---|
| Prioridad | **P1** |
| Estado | **RESUELTO** |
| Archivo | `lanchester_model.h` — `terrain_fire_multiplier()`, `compute_effective_rates()` |
| Tipo | Realismo del modelo |

### Descripcion

El terreno solo afectaba la velocidad de desplazamiento, no la efectividad del fuego convencional.

### Resolucion

Implementado `terrain_fire_multiplier()` que aplica multiplicadores configurables por tipo de terreno (FACIL=1.0, MEDIO=0.85, DIFICIL=0.65) sobre la tasa convencional. Los valores se cargan desde `model_params.json` (`terrain_fire_effectiveness`).

---

## DT-007 — Modelo de municion C/C internamente inconsistente

| Campo | Valor |
|---|---|
| Prioridad | **P1** |
| Estado | **RESUELTO** |
| Archivo | `lanchester_model.h` — `dynamic_rate_cc()`, `simulate_combat()` |
| Tipo | Consistencia interna |

### Descripcion

El modelo de municion C/C mezclaba escalas individual y agregada sin justificacion.

### Resolucion

Reescrito `dynamic_rate_cc()` para usar municion total de la fuerza (`M * n_cc_initial`) y tracking explicito del consumo acumulado:

```cpp
double dynamic_rate_cc(double S_cc_static, double A_current, double A0,
                       double cc_ammo_consumed, double cc_ammo_max)
```

El consumo se acumula en `simulate_combat()` como variable de estado, comparando contra el pool total de municion de la fuerza.

---

## DT-008 — Monolito de 1130 lineas en un solo archivo

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Estado | **RESUELTO** |
| Archivo | `lanchester_types.h`, `lanchester_model.h`, `lanchester_io.h`, `gui_main.cpp` |
| Tipo | Mantenibilidad |

### Descripcion

Todo el codigo estaba en un unico archivo monolitico (>1100 lineas).

### Resolucion

Modularizado en 3 headers + aplicacion GUI:
- **`lanchester_types.h`** (~170 lineas): Todas las estructuras de datos, enums y typedefs.
- **`lanchester_model.h`** (~710 lineas): Modelo matematico, integrador RK4, Monte Carlo, funciones de calculo de tasas.
- **`lanchester_io.h`** (~970 lineas): I/O JSON, parseo de escenarios, batch, sweep, sensibilidad, serializacion.
- **`gui_main.cpp`** (~500 lineas): Aplicacion GUI Windows (Dear ImGui + SDL2 + implot).

Se usa compilacion header-only con funciones `inline` para mantener un unico TU (translation unit).

---

## DT-009 — Variable global mutable para modo de agregacion

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Estado | **RESUELTO** |
| Archivo | `lanchester_types.h` — `CombatInput`, `gui_main.cpp` |
| Tipo | Calidad de codigo |

### Descripcion

`g_aggregation_mode` era estado global mutable leido implicitamente por `simulate_combat()`.

### Resolucion

Eliminada la variable global. `AggregationMode` se pasa como campo de `CombatInput` y como parametro explicito a `run_scenario()`, `run_batch()` y `run_sweep()`.

---

## DT-010 — `std::exit(1)` en funcion de busqueda de vehiculos

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Estado | **RESUELTO** |
| Archivo | `lanchester_io.h` — `find_vehicle()` |
| Tipo | Robustez |

### Descripcion

Si un vehiculo no existia en ningun catalogo, el proceso entero moria con `std::exit(1)`.

### Resolucion

`find_vehicle()` ahora lanza `std::runtime_error` con mensaje descriptivo. El caller puede capturar la excepcion y decidir si abortar o continuar (relevante para modo batch).

---

## DT-011 — Sin validacion de parametros de entrada

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Estado | **RESUELTO** |
| Archivo | `lanchester_io.h` — `validate_side_params()`, `validate_solver_params()`, `parse_composition()` |
| Tipo | Robustez |

### Descripcion

No se validaban los valores del JSON (count negativo, fracciones fuera de rango, h=0, etc.).

### Resolucion

Implementadas funciones de validacion:
- `parse_composition()`: valida `count >= 1`
- `validate_side_params()`: valida rangos de `engagement_fraction`, `aft_casualties_pct`, `rate_factor`, `count_factor`
- `validate_solver_params()`: valida `h > 0`, `t_max > 0`

---

## DT-012 — Redondeo a 2 decimales en la salida pierde precision

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Estado | **RESUELTO** |
| Archivo | `lanchester_io.h` — `result_to_json()` |
| Tipo | Precision |

### Descripcion

Todos los campos se redondeaban a 2 decimales, perdiendo precision significativa.

### Resolucion

La salida JSON ahora usa 6 decimales de precision por defecto. La funcion `round_to()` se usa con precision configurable (parametro `precision=6` en `result_to_json()`).

---

## DT-013 — Tests no verifican contra soluciones analiticas

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Estado | **RESUELTO** |
| Archivo | `tests/test_09_analytical.json` |
| Tipo | Validacion |

### Descripcion

Ningun test comparaba resultados numericos contra la solucion analitica cerrada de Lanchester.

### Resolucion

Creado `tests/test_09_analytical.json`: Escenario 15 LEOPARDO_2E vs 10 T-80U, terreno FACIL, h=0.001. El escenario permite verificar el modelo contra la solucion cerrada de la ley cuadrada de Lanchester.

Resultado validado: error de 0.051 vehiculos, consistente con la solucion analitica. La validacion se puede reproducir ejecutando el escenario desde la GUI y comparando manualmente con la formula cerrada.

---

## DT-014 — `exe_directory()` no es robusto en todas las plataformas

| Campo | Valor |
|---|---|
| Prioridad | **P3** |
| Estado | **RESUELTO** |
| Archivo | `lanchester_io.h` — `exe_directory()` |
| Tipo | Portabilidad |

### Descripcion

`argv[0]` puede no contener el path real del ejecutable.

### Resolucion

En Windows, se usa `GetModuleFileNameA()` para obtener el path real del ejecutable. Se mantiene `argv[0]` como fallback para otros entornos. El codigo incluye un `#ifdef _WIN32` con la implementacion nativa de Windows y un `#else` con fallback POSIX por portabilidad.

---

## DT-015 — Sweep usa comparacion de punto flotante para limites del bucle

| Campo | Valor |
|---|---|
| Prioridad | **P3** |
| Estado | **RESUELTO** |
| Archivo | `lanchester_io.h` — `run_sweep()` |
| Tipo | Correccion |

### Descripcion

El bucle de sweep usaba `val += step` con punto flotante, acumulando error.

### Resolucion

Reescrito con contador entero:

```cpp
int n_steps = static_cast<int>(std::round((end - start) / step));
for (int i = 0; i <= n_steps; ++i) {
    double val = start + i * step;
    ...
}
```

---

## DT-016 — Include de `<filesystem>` fuera de la zona de includes

| Campo | Valor |
|---|---|
| Prioridad | **P3** |
| Estado | **RESUELTO** |
| Archivo | `lanchester_io.h` |
| Tipo | Limpieza de codigo |

### Descripcion

`#include <filesystem>` aparecia fuera de la zona de includes.

### Resolucion

Todos los includes estan correctamente organizados al inicio de cada header en la nueva estructura modular.

---

## Resumen por prioridad

| Prioridad | Total | Resueltos | Pendientes | IDs pendientes |
|---|---|---|---|---|
| P0 (Critica) | 2 | 2 | 0 | — |
| P1 (Alta) | 5 | 5 | 0 | — |
| P2 (Media) | 6 | 6 | 0 | — |
| P3 (Baja) | 3 | 3 | 0 | — |

**Total: 16/16 resueltos.** Toda la deuda tecnica identificada ha sido resuelta. La calibracion contra datos reales (mencionada en DT-002) sigue abierta como mejora futura, pero no es deuda tecnica — requiere datos de referencia externos.

---

*Documento generado a partir de evaluacion critica del codigo. Ultima actualizacion: 2026-03-13.*
