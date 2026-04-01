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
| DT-021 | P1 | Pendiente | Velocidad proporcional produce cero en escenarios realistas |
| DT-022 | P2 | Pendiente | Formula de normalizacion de phi (OMML ambigua) |
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

### DT-021 — Velocidad proporcional produce cero en escenarios realistas

| Campo | Valor |
|---|---|
| Prioridad | **P1** |
| Estado | **PENDIENTE** |

**Descripcion:**

La formula de velocidad proporcional del docx (EQ-020/021) es:

```
v_A_prop = v_A * max(0, 0.1 * phi - 9 + 1)   = v_A * max(0, 0.1 * phi - 8)
v_R_prop = v_R * max(0, 0.1 / phi - 9 + 1)    = v_R * max(0, 0.1 / phi - 8)
```

El factor `0.1 * phi - 8` es positivo solo cuando `phi > 80`. En combate
realista, `phi` (proporcion estatica = ratio de potencia ponderado por
fuerza) toma valores en el rango [0, 10]. Esto significa que **ningun
bando se mueve** con la formula actual.

**Consecuencias:**
- `approach_speed = 0` en todos los escenarios → la distancia nunca cambia.
- `displacement_time = 0` siempre.
- `faster_team = EQUAL` siempre.

**Origen del problema:**

El cientifico del docx expresa incertidumbre sobre este valor (Nota 7, §106):
> "Al no venir esta formula en el Excel no tengo claro de donde sale 0.1,
> y sobre todo el 9 que se resta, el cual me preocupa porque parece un
> numero arbitrario."

**Hipotesis:** El "9" en la formula OMML del Word es probablemente "0.9"
con el punto decimal perdido en la extraccion. Con `0.9` la formula seria:

```
factor = 0.1 * phi - 0.9 + 1 = 0.1 * phi + 0.1 = 0.1 * (phi + 1)
```

Para `phi = 1` (equilibrio): `factor = 0.2` → v_prop = 20% de v_base.
Para `phi = 5` (ventaja azul): `factor = 0.6` → v_prop = 60% de v_base.

Esto produce velocidades razonables y proporcionales a la ventaja.

**Accion requerida:** Confirmar con el cientifico si el valor correcto
es `9` o `0.9`. Si es `0.9`, cambiar en `square_law_model.cpp` las
dos lineas:

```cpp
// Actual:
double factor = 0.1 * phi_raw - 9.0 + 1.0;
// Correccion probable:
double factor = 0.1 * phi_raw - 0.9 + 1.0;
```

**Implementacion actual:** Se ha implementado la formula literal del docx
con `max(0, ...)` para evitar velocidades negativas. Ficheros:
`src/domain/square_law_model.cpp` (simulate y simulateStochastic).

---

### DT-022 — Formula de normalizacion de phi (OMML ambigua)

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Estado | **PENDIENTE** |

**Descripcion:**

La formula de probabilidad estatica (docx §45, EQ-012) se extrajo del
OMML como `P_e = (phi - 10) / 20`. Esto daria valores negativos para
phi < 10, lo cual no tiene sentido como probabilidad.

Se ha implementado como `P_e = (phi + 10) / 20` que mapea [-10, 10] → [0, 1],
que es la unica normalizacion que produce una probabilidad valida.

**Accion requerida:** Confirmar con el cientifico la formula exacta
de normalizacion. La ambiguedad viene de la extraccion OMML del Word
donde los signos en fracciones pueden invertirse.

**Implementacion actual:** `src/domain/square_law_model.cpp`, variable
`P_e = (phi + 10.0) / 20.0`.

---

## Items resueltos

### DT-008 — Arquitectura OOP completa

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Estado | **RESUELTO** |

- Estructura de directorios `src/domain/`, `src/application/`, `src/ui/`, `src/tests/`.
- Clases OOP: `ModelParamsClass`, `VehicleCatalogClass`, `SquareLawModel`, `SimulationService`, `ScenarioConfig`.
- 27 tests automatizados Catch2.
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

*Ultima actualizacion: 2026-04-01.*
