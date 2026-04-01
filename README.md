# Modelo Lanchester-CIO

Herramienta de investigacion interna del CIO/ET. Simula enfrentamientos entre fuerzas terrestres usando ecuaciones de Lanchester (ley cuadrada): vencedor, bajas, duracion, municion consumida.

**Aplicacion de escritorio para Windows** con interfaz grafica (Dear ImGui + SDL2).

> **Aviso**: Todos los parametros del modelo estan marcados como `uncalibrated`. Los resultados no deben usarse para informar decisiones operativas sin una calibracion previa contra datos de referencia.

## Uso rapido

1. Copia la carpeta `release/` a un PC con Windows
2. Ejecuta `lanchester_gui.exe`
3. Configura el escenario en el panel izquierdo y pulsa "EJECUTAR SIMULACION"

Contenido de `release/`:

| Fichero | Descripcion |
|---|---|
| `lanchester_gui.exe` | Aplicacion principal (GUI) |
| `SDL2.dll` | Runtime grafico (necesario) |
| `model_params.json` | Parametros del modelo |
| `vehicle_db.json` | Catalogo vehiculos azul (NATO) |
| `vehicle_db_en.json` | Catalogo vehiculos rojo (OPFOR) |
| `gui_config.json` | Configuracion de la GUI (velocidad animacion) |

## Modelo matematico

### Ecuaciones base

El modelo implementa la **ley cuadrada de Lanchester** con integracion **Euler explicito** (docx §85, Anexo 2). Las fuerzas A (azul) y R (rojo) evolucionan segun:

```
dA/dt = -(S_red + S_red_Tcc(t)) * R(t)
dR/dt = -(S_blue + S_blue_Tcc(t)) * A(t)
```

Discretizacion Euler (h = 1/600 min = 0.1s):

```
A_{i+1} = max(0, A_i + h * dA)
R_{i+1} = max(0, R_i + h * dR)
```

Donde `S` es la tasa de destruccion convencional y `S_Tcc(t)` es la tasa contracarro temporal.

### Tasa de fuego convencional

```
S_conv = T(D, P) * G(d, f, A_max) * U * c * mult_tactico * mult_terreno * rate_factor
```

| Componente | Formula | Descripcion |
|---|---|---|
| `T(D, P)` | `1 / (1 + exp((D - P) / slope))` | Probabilidad de destruccion (sigmoide). D = proteccion del objetivo, P = penetracion del atacante |
| `G(d, f, A_max)` | Polinomio de 6 coeficientes | Degradacion por distancia. 0 si d > A_max |
| `U` | Escalar [0-1] | Punteria del artillero (no aplica a misiles C/C) |
| `c` | Disparos/min | Cadencia de fuego |

### Tasa de fuego C/C (misiles contracarro)

```
S_Tcc(t) = S_cc_static * max(0, (M - c_cc * t) / M)
```

Municion finita: la tasa decae linealmente con el tiempo (docx EQ-015/016). `M` es municion por vehiculo C/C, `c_cc` es cadencia C/C. La tasa se acota a [-10, 10] (docx §39). Resultado acotado a [-10, 10].

### Proporcion estatica y probabilidad estatica

Antes de ejecutar la simulacion (docx §41-46, EQ-008/012):

```
phi = ((S_A + S_A_cc) * V_A) / ((S_R + S_R_cc) * V_R)   acotada [-10, 10]
P_e = (phi + 10) / 20                                     normalizada [0, 1]
```

`phi > 1` favorece azul. `P_e > 0.5` indica ventaja azul.

### Velocidad proporcional y distancia variable

La velocidad de aproximacion depende de la proporcion estatica y del estado tactico (docx §63-73, EQ-020/021):

```
v_A_prop = v_A * max(0, 0.1 * phi - 8)    si situacion permite movilidad
v_R_prop = v_R * max(0, 0.1 / phi - 8)    si situacion permite movilidad
d(t) = max(50, d0 - (v_A_prop + v_R_prop) * t)
```

Cada estado tactico tiene un booleano `mobility_allowed` en `model_params.json`. Las tasas de destruccion se recalculan en cada paso al variar la distancia.

> **Nota**: La formula de velocidad proporcional (factor `0.1*phi - 8`) produce velocidad cero para todo `phi < 80`. Ver `DEUDA_TECNICA.md` DT-021.

### Monte Carlo

Modo estocastico con bajas discretas Poisson por paso temporal. Subdivision automatica del paso si lambda > 2 para preservar la fidelidad de la distribucion.

### Parametros de vehiculo

| Campo | Descripcion |
|---|---|
| `D` | Proteccion/blindaje (convencional) |
| `P` | Penetracion del armamento principal |
| `U` | Factor de punteria [0-1] |
| `c` | Cadencia de fuego (disparos/min) |
| `A_max` | Alcance maximo efectivo (m) |
| `f` | Factor de distancia del vehiculo |
| `CC` | Tiene capacidad contracarro (0/1) |
| `P_cc`, `D_cc`, `c_cc`, `A_cc`, `f_cc` | Parametros C/C equivalentes |
| `M` | Municion C/C por vehiculo |

## Interfaz grafica

La aplicacion permite:

- **Configurar escenarios**: seleccionar vehiculos y cantidades para cada bando, terreno, distancia de enfrentamiento, estado tactico, movilidad
- **Modo determinista**: simulacion unica con Euler explicito
- **Modo Monte Carlo** (extension): N replicas estocasticas con distribucion de resultados (probabilidad de victoria, percentiles de supervivientes)
- **Parametros avanzados**: bajas AFT pre-contacto, fraccion de empenamiento (defecto 2/3), factor de cadencia
- **Salidas**: proporcion estatica (phi), probabilidad estatica (P_e), bando ganador, bajas a escala original, tiempo de desplazamiento, equipo mas rapido, municion consumida, serie temporal
- **Visualizacion**: tablas de resultados y graficas de barras (supervivientes, distribucion de outcomes)

### Opciones de agregacion

- `PRE` (por defecto): media ponderada de parametros, una tasa unica
- `POST`: tasa individual por tipo de vehiculo, luego media ponderada. Mas realista para fuerzas heterogeneas

## Catalogos de vehiculos

**Bando azul** (`vehicle_db.json`):

| Nombre | Vehiculo | C/C |
|---|---|---|
| LEOPARDO_2E | Carro de combate Leopard 2E | No |
| PIZARRO | VCBR Pizarro | Si |
| TOA_SPIKE_I | TOA con misiles Spike | Si |
| VEC_25 | VEC 8x8 con canon 25mm | No |

**Bando rojo** (`vehicle_db_en.json`):

| Nombre | Vehiculo | C/C |
|---|---|---|
| T-80U | Carro de combate T-80U | No |
| T-72B3 | Carro de combate T-72B3 | No |
| BMP-3 | BMP-3 | Si |
| BTR-82A | BTR-82A | No |

## Parametros del modelo

Los parametros se cargan desde `model_params.json` (junto al .exe). **Todos estan sin calibrar** — requieren validacion contra datos reales o simulaciones de referencia.

| Parametro | Valor por defecto | Estado |
|---|---|---|
| `kill_probability_slope` | 175.0 | Sin calibrar |
| `distance_degradation_coefficients` | 6 coeficientes polinomiales | Sin calibrar |
| `tactical_multipliers` | Por estado tactico | Sin calibrar |
| `terrain_fire_effectiveness` | FACIL=1.0, MEDIO=0.85, DIFICIL=0.65 | Sin calibrar |

Cada parametro en el JSON incluye `origin`, `calibration_status` y `valid_range` para facilitar una futura calibracion.

## Referencia de campos del escenario

| Campo | Valores |
|---|---|
| `terrain` | `FACIL`, `MEDIO`, `DIFICIL` |
| `tactical_state` | `Ataque a posicion defensiva`, `Busqueda del contacto`, `En posicion de tiro`, `Defensiva condiciones minimas`, `Defensiva organizacion ligera`, `Defensiva organizacion media`, `Retardo`, `Retrocede` |
| `mobility` | `MUY_ALTA`, `ALTA`, `MEDIA`, `BAJA` |
| `engagement_fraction` | Fraccion de la fuerza que entra en combate [0.0-1.0] (defecto 2/3) |
| `aft_casualties_pct` | Porcentaje de bajas por AFT pre-contacto [0.0-1.0] |
| `rate_factor` | Multiplicador de tasa de destruccion (defecto 1.0) |

## Velocidades tacticas (km/h)

| Movilidad \ Terreno | FACIL | MEDIO | DIFICIL |
|---|---|---|---|
| MUY_ALTA | 40 | 25 | 12 |
| ALTA | 30 | 20 | 10 |
| MEDIA | 20 | 12 | 6 |
| BAJA | 10 | 6 | 3 |

## Compilacion y tests

### Tests (Linux nativo)

```bash
cmake -B build -DLANCHESTER_BUILD_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

27 tests unitarios con Catch2 v3 que cubren:
- Carga de parametros y catalogos (`ModelParamsClass`, `VehicleCatalogClass`)
- Modelo de simulacion (`SquareLawModel`) con validacion contra solucion analitica
- Servicio de simulacion (`SimulationService`) con ejecucion sincrona y asincrona
- Validacion de configuraciones (`ScenarioConfig`)

### GUI Windows (cross-compile desde Linux)

```bash
# 1. Instalar cross-compiler
sudo apt install g++-mingw-w64-x86-64

# 2. Descargar dependencias (SDL2, Dear ImGui, implot)
bash setup_gui_deps.sh

# 3. Compilar con CMake
cmake -B build-win \
  -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-toolchain.cmake \
  -DLANCHESTER_BUILD_GUI=ON -DLANCHESTER_BUILD_TESTS=OFF
cmake --build build-win
```

Genera `release/lanchester_gui.exe` + `release/SDL2.dll`.

## Arquitectura

Arquitectura OOP por capas. La GUI delega en `SimulationService`, que usa `SquareLawModel` (inyeccion de dependencias). Sin globals mutables.

### Capas

```
┌──────────────────────────────────────────────────────┐
│  PRESENTACION (src/ui/)                              │
│    gui_main.cpp — GUI (Dear ImGui + SDL2 + implot)   │
│      └─ usa SimulationService (async)                │
├──────────────────────────────────────────────────────┤
│  APLICACION (src/application/)                       │
│    SimulationService — orquestacion (sync + async)   │
│    ScenarioConfig    — configuracion tipada           │
│      └─ usa ILanchesterModel (inyectado)             │
├──────────────────────────────────────────────────────┤
│  DOMINIO (src/domain/)                               │
│    ILanchesterModel  — interfaz abstracta            │
│    SquareLawModel    — ley cuadrada (Euler + MC)      │
│    ModelParamsClass  — parametros del modelo          │
│    VehicleCatalogClass — catalogo de vehiculos       │
└──────────────────────────────────────────────────────┘
```

`SquareLawModel` esta completamente encapsulada y validada por 27 tests Catch2. `ILanchesterModel` permite futuras variantes (ej. ley lineal).

### Estructura de ficheros

```
src/
├── domain/
│   ├── lanchester_types.h           # Structs, enums y utilidades base
│   ├── lanchester_model_iface.h     # Interfaz abstracta ILanchesterModel
│   ├── model_params_iface.h         # Interfaz abstracta IModelParams
│   ├── vehicle_catalog_iface.h      # Interfaz abstracta IVehicleCatalog
│   ├── model_params.h/cpp           # Clase ModelParamsClass (inmutable)
│   ├── vehicle_catalog.h/cpp        # Clase VehicleCatalogClass
│   ├── square_law_model.h/cpp       # Clase SquareLawModel (Euler + MC Poisson)
│   ├── montecarlo_runner.h/cpp      # Orquestador de N replicas Monte Carlo
│   ├── model_factory.h/cpp          # Factory pattern para instanciacion de modelos
│   └── combat_utils.h/cpp           # Utilidades: agregacion (PRE/POST), distribucion de bajas
├── application/
│   ├── scenario_config.h/cpp        # ScenarioConfig + validacion
│   └── simulation_service.h/cpp     # SimulationService (sincrono + async)
├── ui/
│   ├── gui_main.cpp                 # Bucle principal, init SDL2/OpenGL, orquestacion wizard
│   ├── gui_config.h/cpp             # Preferencias GUI (ventana, fuentes, tema)
│   ├── gui_state.h                  # Estado global de la app (wizard, resultados, colores)
│   ├── gui_nav_bar.h                # Barra de navegacion por pasos
│   ├── gui_step_scenario.h          # Panel de configuracion de escenario
│   ├── gui_step_side.h              # Panel de composicion de fuerzas (azul/rojo)
│   ├── gui_step_simulation.h        # Ejecucion y visualizacion de resultados
│   └── gui_log.h                    # Panel de mensajes y errores
├── tests/
│   ├── test_main.cpp                # Tests base (smoke, params, catalogo, simulacion)
│   ├── test_model_params.cpp        # Tests de ModelParamsClass
│   ├── test_vehicle_catalog.cpp     # Tests de VehicleCatalogClass
│   ├── test_square_law.cpp          # Tests del modelo SquareLawModel
│   ├── test_simulation_service.cpp  # Tests de integracion del servicio
│   └── data/                        # JSONs de test (params, catalogos, escenarios)
└── include/nlohmann/json.hpp        # Dependencia JSON header-only

CMakeLists.txt                       # Build system (CMake unico)
cmake/mingw-w64-toolchain.cmake      # Toolchain cross-compilacion Windows
setup_gui_deps.sh                    # Descarga dependencias (SDL2, ImGui, implot)
data/
├── model_params.json                # Parametros del modelo
├── vehicle_db.json                  # Catalogo vehiculos azul
├── vehicle_db_en.json               # Catalogo vehiculos rojo
└── gui_config.json                  # Configuracion GUI (velocidad animacion)
ejemplos/
├── toa_vs_t80u.json                 # Escenario simple (TOA Spike vs T-80U)
└── compania_mixta.json              # Cadena de 2 combates (fuerza mixta)
release/                             # Binarios Windows distribuibles
```

## Decisiones de diseno

- **Ley cuadrada de Lanchester.** Las bajas son proporcionales al numero de efectivos enemigos y a su tasa de fuego. Valida para combates de fuego directo.
- **Integrador Euler explicito.** Metodo numerico de primer orden con h = 0.1s (docx §85). Referencia: `Resumen_tecnico_Lancaster_ANONIMIZADO.docx`, Anexo 2.
- **Proporcion estatica (phi).** Ratio de potencia de combate ponderado por fuerza, acotado [-10, 10]. Probabilidad estatica P_e normalizada [0, 1]. Se devuelve antes de simular (docx §92).
- **Velocidad proporcional.** La velocidad de aproximacion depende de phi y del booleano `mobility_allowed` del estado tactico (docx §63-73). Ver DT-021 para limitaciones.
- **Tasa C/C temporal.** Municion se depleta linealmente con el tiempo: `(M - c*t) / M` (docx EQ-015). Sin factor de fuerza superviviente.
- **Proporcion por defecto 2/3.** `engagement_fraction = 2/3` como indica el docx §18. AFT se aplica sobre las fuerzas asignadas.
- **Bajas a escala original.** Al reportar, se incluyen las fuerzas no participantes (docx §97): `casualties = total_original - survivors`.
- **Monte Carlo con Poisson (extension).** No forma parte del modelo base del docx (Nota 1). Bajas discretas por paso temporal. Subdivision automatica si lambda > 2.
- **Agregacion pre-tasa por defecto.** POST (extension) mas realista para fuerzas heterogeneas. No descrita en el docx.
- **Punteria (U) no aplica a C/C.** Los misiles guiados tienen probabilidad absorbida en la sigmoide de kill probability.
- **Parametros externalizados.** Todos en `model_params.json` con metadatos de origen y estado de calibracion.
- **Arquitectura OOP desacoplada.** Interfaz abstracta del modelo permite variantes futuras. Servicio con ejecucion async segura. GUI reemplazable.

## Limitaciones conocidas

- **Solo ley cuadrada.** No implementa la ley lineal de Lanchester (combate cuerpo a cuerpo / area) ni modelos mixtos.
- **Sin modelado de C2.** No representa cadena de mando, comunicaciones ni degradacion por perdida de puestos de mando.
- **Sin logistica.** No modela suministro de combustible, repuestos ni evacuacion sanitaria. La unica restriccion logistica es la municion C/C finita.
- **Sin efectos de red.** No hay sinergia entre sensores, comunicaciones y sistemas de armas (ej: datalink, guerra electronica).
- **Terreno abstracto.** Tres niveles discretos (FACIL/MEDIO/DIFICIL) sin modelado topografico, lineas de vista ni cobertura.
- **Combate directo unicamente.** No incluye fuegos indirectos (artilleria, morteros), apoyo aereo ni defensa antiaerea.
- **Parametros sin calibrar.** Todos los valores por defecto son estimaciones iniciales sin validacion contra datos historicos o simulaciones de referencia.

## Documentos relacionados

| Documento | Contenido |
|---|---|
| `Resumen_tecnico_Lancaster_ANONIMIZADO.docx` | **Fuente de verdad** del modelo matematico (documento del cientifico) |
| `PLAN_ADAPTACION_DOCX.md` | Plan de adaptacion al docx: cambios C1-C9, implementacion paralela pre-adaptacion (PA1-PA7) |
| `PLAN_DE_PRUEBAS.md` | Guia paso a paso para pruebas manuales en Windows |
| `PLAN_INTERFAZ_GRAFICA.md` | Especificacion de diseno de la GUI (layouts, colores, interacciones) |
| `DEUDA_TECNICA.md` | Registro de deuda tecnica (calibracion pendiente, velocidad proporcional) |

---

*CIO / ET — Herramienta de investigacion interna*
