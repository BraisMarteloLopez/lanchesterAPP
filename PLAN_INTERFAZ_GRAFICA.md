# Plan de Interfaz Grafica — Lanchester-CIO

## Vision general

Wizard de configuracion paso a paso (estilo Civilization) con barra de navegacion superior persistente. Interfaz profesional, oscura, orientada a datos.

Framework: Dear ImGui + SDL2 + OpenGL3 + implot (sin cambios).

---

## Barra de navegacion (siempre visible)

```
┌──────────────────────────────────────────────────────────────────────────────┐
│  [1. Escenario]  ────  [2. Bando Azul]  ────  [3. Bando Rojo]  ────  [4. Simulacion]  │
└──────────────────────────────────────────────────────────────────────────────┘
```

### Comportamiento

| Estado del paso | Visual | Click |
|---|---|---|
| Completado | Texto normal + icono check | Navega directamente |
| Activo | Texto resaltado + subrayado/fondo | No (ya estas aqui) |
| Disponible (previos OK) | Texto normal | Navega directamente |
| Bloqueado (previos incompletos) | Texto gris/deshabilitado | No hace nada |

### Reglas de validacion

- Paso 2 accesible: siempre (paso 1 tiene defaults validos)
- Paso 3 accesible: paso 2 tiene al menos 1 vehiculo con count >= 1
- Paso 4 accesible: paso 3 tiene al menos 1 vehiculo con count >= 1

### Navegacion

- Click en cualquier paso accesible = navegacion directa
- Botones ANTERIOR / SIGUIENTE en la parte inferior de cada pantalla como atajo
- Cambiar de paso NO pierde el estado configurado (persistente en AppState)

---

## Pantalla 1 — Escenario

Configuracion del campo de batalla.

### Layout

```
┌─────────────────── ESCENARIO ───────────────────┐
│                                                  │
│  TERRENO                                         │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐        │
│  │  FACIL   │ │  MEDIO   │ │ DIFICIL  │        │
│  │ (desc.)  │ │ (desc.)  │ │ (desc.)  │        │
│  └──────────┘ └──────────┘ └──────────┘        │
│                                                  │
│  DISTANCIA DE EMPENAMIENTO                       │
│  ├──────────────●──────────────────┤            │
│  100m                          9000m   [2000 m]  │
│                                                  │
│  MODELO DE SIMULACION                            │
│  [ Lanchester Square Law (RK4)          ▼ ]     │
│                                                  │
│  MODO                                            │
│  ○ Determinista    ○ Monte Carlo                 │
│  (si MC: Replicas [1000]  Seed [42])            │
│                                                  │
│  AGREGACION                                      │
│  ○ PRE (por defecto)    ○ POST (mas realista)   │
│                                                  │
│  TIEMPO MAXIMO                                   │
│  ├────────●────────────────────────┤            │
│  5 min                      120 min   [30 min]   │
│                                                  │
│                              [ SIGUIENTE → ]     │
└──────────────────────────────────────────────────┘
```

### Elementos

| Elemento | Widget | Fuente de datos |
|---|---|---|
| Terreno | 3 tarjetas seleccionables (radio visual) | Enum Terrain |
| Distancia | Slider horizontal con valor numerico | 100–9000 m |
| Modelo | Dropdown | ModelFactory::availableModels() |
| Modo | Radio buttons | Determinista / Monte Carlo |
| Replicas, Seed | InputInt (solo visible si MC) | — |
| Agregacion | Radio buttons | PRE / POST |
| Tiempo maximo | Slider horizontal | 5–120 min |

---

## Pantalla 2 — Bando Azul (Propio)

Configuracion de las fuerzas propias. Estetica azul.

### Layout

```
┌──────────────── BANDO AZUL (Propio) ────────────────┐
│                                                      │
│  COMPOSICION DE FUERZAS                              │
│  ┌────────────────────────────────────────────────┐  │
│  │ Vehiculo              │ Cantidad │             │  │
│  ├───────────────────────┼──────────┼─────────────┤  │
│  │ [LEOPARDO_2E     ▼]  │ [  10  ] │     [X]     │  │
│  │ [PIZARRO         ▼]  │ [   5  ] │     [X]     │  │
│  │                       │          │             │  │
│  │ [ + Anadir unidad ]   │          │             │  │
│  └────────────────────────────────────────────────┘  │
│                                                      │
│  Tooltip al hover: D=900 P=850 U=0.85 c=6.0         │
│                    A_max=4000 CC=No                   │
│                                                      │
│  ESTADO TACTICO                                      │
│  [ Ataque a posicion defensiva              ▼ ]     │
│                                                      │
│  MOVILIDAD                                           │
│  [ ALTA                                     ▼ ]     │
│                                                      │
│  ▶ Parametros avanzados                              │
│    Bajas AFT (%)          ├──────●──┤  0%           │
│    Fraccion empenamiento  ├────────●┤  1.00         │
│    Factor cadencia        ├────────●┤  1.00         │
│    Factor efectivos       ├────────●┤  1.00         │
│                                                      │
│  [ ← ANTERIOR ]                  [ SIGUIENTE → ]    │
└──────────────────────────────────────────────────────┘
```

### Elementos

| Elemento | Widget | Notas |
|---|---|---|
| Composicion | Tabla editable (combo + input + boton eliminar) | Max 4 tipos. Tooltip con stats del vehiculo |
| Anadir unidad | Boton (si < 4 tipos) | — |
| Estado tactico | Dropdown | Nombres desde ModelParamsClass::tacticalStateNames() |
| Movilidad | Dropdown | MUY_ALTA / ALTA / MEDIA / BAJA |
| Parametros avanzados | Colapsable (TreeNode) | — |
| AFT, empenamiento, cadencia, efectivos | Sliders | Rangos definidos en ScenarioConfig::validate() |

---

## Pantalla 3 — Bando Rojo (Enemigo)

Identica a la Pantalla 2 pero con:

- Estetica roja (header rojo, acentos rojos)
- Titulo: "BANDO ROJO (Enemigo)"
- Catalogo de vehiculos enemigos (vehicle_db_en.json)
- Mismos widgets, misma estructura

---

## Pantalla 4 — Simulacion + Resultado

Pantalla principal. Dividida en 3 zonas.

### Layout

```
┌───────────────────── SIMULACION ──────────────────────┐
│                                                        │
│  ┌─ RESUMEN DEL ESCENARIO ──────────────────────────┐ │
│  │ Terreno: MEDIO  Distancia: 2000m  Modo: Determ.  │ │
│  │ Azul: 10 LEOPARDO + 5 PIZARRO (Ataque)           │ │
│  │ Rojo: 15 T-80U + 5 BMP-3 (Defensiva org. media)  │ │
│  └───────────────────────────────────────────────────┘ │
│                                                        │
│  ┌─ GRAFICA 2D EVOLUTIVA ───────────────────────────┐ │
│  │                                                   │ │
│  │  Efectivos                                        │ │
│  │  15 ┤ ████                                        │ │
│  │     │ █  █                                        │ │
│  │  10 ┤ █   ██ ← Azul                              │ │
│  │     │ █     ████                                  │ │
│  │   5 ┤ █         ████████████                      │ │
│  │     │  █  ← Rojo      ██████                      │ │
│  │   0 ┤───█──────────────────██──────               │ │
│  │     0    5    10    15    20    25  Tiempo (min)   │ │
│  │                                                   │ │
│  └───────────────────────────────────────────────────┘ │
│                                                        │
│  ┌─ METRICAS ────────────────────────────────────────┐ │
│  │                                                   │ │
│  │  RESULTADO: BLUE_WINS                             │ │
│  │                                                   │ │
│  │  Metrica           │ Azul    │ Rojo               │ │
│  │  Fuerzas iniciales │ 15.00   │ 20.00              │ │
│  │  Supervivientes    │  7.23   │  0.00              │ │
│  │  Bajas             │  7.77   │ 20.00              │ │
│  │  Duracion: 18.5 min  Ventaja: 1.23               │ │
│  │                                                   │ │
│  │  (Si MC: probabilidades + tabla percentiles)      │ │
│  └───────────────────────────────────────────────────┘ │
│                                                        │
│  [ ← VOLVER A CONFIGURACION ]                         │
│                                                        │
│       [ ▶▶  EJECUTAR SIMULACION  ▶▶ ]                 │
│                                                        │
└────────────────────────────────────────────────────────┘
```

### Grafica 2D evolutiva

**Contenido**: dos curvas (Azul y Rojo) mostrando la evolucion de efectivos A(t) y R(t) a lo largo del tiempo de combate.

**Requisito de datos**: `SquareLawModel::simulate()` debe devolver la serie temporal completa, no solo el resultado final. Nuevo campo en CombatResult o estructura separada:

```cpp
struct TimeStep {
    double t;           // minutos
    double blue_forces; // efectivos azul
    double red_forces;  // efectivos rojo
};

// En CombatResult o estructura de resultado extendida:
std::vector<TimeStep> time_series;
```

**Animacion**: la grafica se dibuja progresivamente tras pulsar EJECUTAR, simulando la evolucion en tiempo real. La velocidad de animacion NO es configurable por el usuario — se lee de un fichero de configuracion:

```json
// gui_config.json (junto al ejecutable)
{
    "animation_speed_ms_per_step": 50
}
```

| Parametro | Tipo | Default | Descripcion |
|---|---|---|---|
| `animation_speed_ms_per_step` | int | 50 | Milisegundos entre cada paso de la animacion. Menor = mas rapido. |

**Comportamiento**:
1. Usuario pulsa EJECUTAR
2. La simulacion se ejecuta completa en background (async)
3. Al recibir el resultado, la grafica comienza a animarse: dibuja progresivamente la serie temporal
4. Al completar la animacion, las metricas finales aparecen en el panel inferior
5. Si el usuario pulsa EJECUTAR de nuevo, se reinicia

**Monte Carlo**: en modo MC no hay animacion temporal. Se muestra la grafica de distribucion de outcomes + tabla de percentiles (como ahora).

### Panel de metricas

**Modo determinista**:
- Outcome con color (azul/rojo/amarillo)
- Tabla: fuerzas iniciales, supervivientes, bajas, municion
- Duracion, ventaja estatica

**Modo Monte Carlo**:
- Probabilidades P(Azul gana), P(Rojo gana), P(Empate)
- Grafica de distribucion de outcomes
- Tabla de estadisticas percentiles (media, std, p05-p95)
- Referencia determinista en gris

---

## Estetica

### Paleta de colores

| Elemento | Color |
|---|---|
| Fondo general | Gris muy oscuro (#1A1A1E) |
| Paneles/tarjetas | Gris oscuro (#252529) |
| Barra navegacion | Fondo ligeramente mas claro (#2A2A30) |
| Paso activo | Azul claro (#4A90D9) |
| Paso completado | Verde suave (#5DAA68) |
| Paso bloqueado | Gris (#666666) |
| Bando Azul | Azul NATO (#3380CC) |
| Bando Rojo | Rojo OPFOR (#CC3333) |
| Boton EJECUTAR | Verde prominente (#2D8E3D) |
| Texto principal | Blanco suave (#E0E0E0) |
| Texto secundario | Gris claro (#999999) |
| Error | Rojo (#FF6666) |
| Outcome DRAW | Amarillo (#CCCC00) |

### Tipografia

ImGui default (ProggyClean) para la fase inicial. Futuro: cargar fuente profesional (Roboto, Inter) via ImGui::GetIO().Fonts.

### Estilo de widgets

- Bordes redondeados (WindowRounding = 6, FrameRounding = 4)
- Padding generoso (FramePadding = 10,6)
- Separacion clara entre secciones
- Tarjetas de terreno con hover highlight
- Sliders con valor numerico visible

---

## Cambios requeridos en el backend

### 1. Serie temporal en CombatResult

`SquareLawModel::simulate()` y `simulateStochastic()` deben registrar A(t) y R(t) en cada paso del integrador para alimentar la grafica 2D.

```cpp
struct TimeStep {
    double t;
    double blue_forces;
    double red_forces;
};
```

Añadir `std::vector<TimeStep> time_series` al resultado o devolver una estructura extendida.

### 2. Fichero de configuracion GUI

Fichero `gui_config.json` (junto al ejecutable) con parametros de la interfaz no editables por el usuario:

```json
{
    "animation_speed_ms_per_step": 50
}
```

### 3. Estados tacticos desde datos

La GUI debe leer los nombres de estados tacticos de `ModelParamsClass::tacticalStateNames()` en vez de arrays hardcodeados. Ya disponible tras Fase A.

---

## Estructura de ficheros propuesta

```
src/ui/
├── gui_main.cpp            # Entry point, inicializacion SDL/ImGui, bucle principal
├── gui_state.h             # AppState, GuiSideConfig, enums de pantalla
├── gui_nav_bar.h/cpp       # Barra de navegacion superior (stepper)
├── gui_step_scenario.h/cpp # Pantalla 1: escenario
├── gui_step_side.h/cpp     # Pantalla 2/3: configuracion de bando (reutilizable)
├── gui_step_simulation.h/cpp # Pantalla 4: simulacion + resultado + grafica
└── gui_config.h/cpp        # Carga de gui_config.json
```

---

## Orden de implementacion

| Fase | Contenido |
|---|---|
| G1 | Backend: serie temporal en simulate(). Estructura AppState + GuiConfig |
| G2 | Barra de navegacion + esqueleto de wizard (4 pantallas vacias) |
| G3 | Pantalla 1 (Escenario) |
| G4 | Pantalla 2/3 (Bando Azul / Rojo) |
| G5 | Pantalla 4 (Simulacion + grafica 2D + metricas) |
| G6 | Animacion de la grafica + pulido visual |

---

*Documento de referencia para la implementacion de la interfaz grafica.*
