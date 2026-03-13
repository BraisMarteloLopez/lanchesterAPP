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
| Archivo | `main.cpp:425-441` |
| Tipo | Precision numerica |

### Descripcion

El bucle de simulacion usa Euler explicito (RK1):

```cpp
double A_new = std::max(0.0, A - Sr * R * input.h);
double R_new = std::max(0.0, R - Sb * A * input.h);
```

Euler tiene error de truncamiento local O(h²) y global O(h). Para un sistema de ecuaciones diferenciales acopladas no lineales, acumula deriva numerica. El paso actual `h = 1/600` compensa parcialmente forzando muchas iteraciones, pero es ineficiente y no garantiza precision.

### Impacto

- Resultados numericamente imprecisos, especialmente en combates largos (t_max alto) o con tasas de destruccion altas.
- No hay forma de estimar el error numerico de una ejecucion concreta.
- Combates con ratios de fuerza extremos (20:1) pueden acumular error significativo.

### Solucion propuesta

Implementar RK4 (Runge-Kutta orden 4) como minimo. Idealmente un metodo adaptativo (Dormand-Prince / RK45) que ajuste el paso automaticamente segun tolerancia de error. Esto permitiria ademas eliminar el parametro `h` del solver (el metodo lo ajusta solo) y reducir el numero total de evaluaciones.

### Validacion

Comparar contra solucion analitica cerrada del caso simetrico de Lanchester (test_01) para cuantificar el error del integrador actual.

---

## DT-002 — Constantes magicas sin calibracion ni referencia

| Campo | Valor |
|---|---|
| Prioridad | **P0** |
| Archivo | `main.cpp:96, 102-107, 130-139` |
| Tipo | Validez del modelo |

### Descripcion

Tres conjuntos de constantes criticas no tienen justificacion documentada:

**a) Pendiente de la sigmoide de kill probability (175.0)**

```cpp
return 1.0 / (1.0 + std::exp((D_target - P_attacker) / 175.0));
```

El valor 175.0 controla la transicion entre "no penetra" y "penetra siempre". Es el parametro mas sensible del modelo: cambiarlo a 100 o 250 altera drasticamente todos los resultados. No hay referencia a datos balisticos, manuales de tiro o estudios empiricos que justifiquen este valor.

**b) Coeficientes del polinomio de degradacion por distancia**

```cpp
double g = -0.188 * dk - 0.865 * f_dist + 0.018 * dk * dk
           - 0.162 * dk * f_dist + 0.755 * f_dist * f_dist + 1.295;
```

Seis coeficientes sin origen documentado. Un polinomio de segundo grado con 6 parametros libres puede ajustarse a casi cualquier curva, lo cual hace imposible evaluar si refleja la realidad sin conocer los datos de origen.

**c) Multiplicadores tacticos cuadraticos**

```cpp
"Defensiva organizacion media"  → 1/(4.25²) = 0.0554
"Retardo"                        → 1/(6.0²)  = 0.0278
```

Un multiplicador de 0.028 implica que un atacante necesita ~36x mas fuerza para superar una posicion de retardo. Los valores base (2.25, 2.75, 4.25, 6.0) y la decision de elevarlos al cuadrado no estan referenciados contra doctrina (FM 5-0, ATP 3-90, manuales OTAN de ratios atacante/defensor).

### Impacto

Sin calibracion, todos los resultados del modelo son no verificables. Los outputs pueden parecer razonables pero estar sistematicamente sesgados. Es imposible distinguir un resultado correcto de uno incorrecto.

### Solucion propuesta

1. Documentar el origen de cada constante (referencia bibliografica, datos experimentales, juicio de experto con nombre y fecha).
2. Ejecutar analisis de sensibilidad formal: barrer cada constante magica en un rango amplio y documentar como cambian los resultados.
3. Si las constantes provienen de juicio de experto, marcarlas explicitamente como parametros configurables en el escenario JSON, no como hardcoded.
4. Considerar mover `175.0` y los coeficientes del polinomio a un fichero de configuracion del modelo (`model_params.json`).

---

## DT-003 — Modelo determinista aplicado a escalas pequenas

| Campo | Valor |
|---|---|
| Prioridad | **P1** |
| Archivo | Diseno del modelo |
| Tipo | Validez conceptual |

### Descripcion

Las ecuaciones de Lanchester modelan combate como un proceso continuo y determinista. Fueron disenadas para fuerzas de gran tamano (batallones, divisiones) donde la ley de los grandes numeros suaviza la variabilidad individual.

El modelo se usa habitualmente con fuerzas de 6-20 vehiculos. A esta escala, los efectos estocasticos dominan: un unico disparo acertado o fallido cambia el resultado. Los "supervivientes" se reportan como 3.47 vehiculos, una cifra sin significado fisico.

### Impacto

- Los resultados para fuerzas < 20 vehiculos carecen de significado estadistico.
- No hay barras de error, intervalos de confianza ni distribucion de resultados posibles.
- Un usuario puede interpretar "3.47 supervivientes" como una prediccion precisa cuando en realidad el resultado real podria oscilar entre 0 y 7.

### Solucion propuesta

- **Corto plazo**: Documentar explicitamente en la salida la escala minima recomendada y advertir cuando las fuerzas sean pequenas.
- **Medio plazo**: Implementar un modo Monte Carlo que ejecute N replicas con variabilidad estocastica (distribucion Poisson o binomial para impactos) y reporte media, desviacion tipica e intervalos de confianza.
- **Largo plazo**: Considerar un modelo basado en agentes (ABM) para escalas de seccion/peloton.

---

## DT-004 — No se modela destruccion selectiva por tipo de vehiculo

| Campo | Valor |
|---|---|
| Prioridad | **P1** |
| Archivo | `main.cpp:688-694`, diseno del modelo |
| Tipo | Realismo del modelo |

### Descripcion

Las bajas se distribuyen proporcionalmente a toda la fuerza. En un combate donde 5 T-80U y 5 BTR-82A sufren 4 bajas, el modelo asigna 2 a cada tipo. En la realidad:

- Los sistemas contracarro priorizan blancos de alto valor (MBTs sobre APCs).
- Los vehiculos menos protegidos (BTR) mueren mas rapido que los blindados (T-80U) ante el mismo fuego.
- La composicion de supervivientes importa tanto como su numero.

### Impacto

En encadenamiento de combates, la composicion arrastrada al combate siguiente es incorrecta. Los supervivientes se recalculan con la misma proporcion de tipos, distorsionando las tasas del siguiente enfrentamiento.

### Solucion propuesta

Implementar un sistema de ecuaciones por tipo de vehiculo dentro de cada bando. En lugar de una unica variable A(t), usar A_1(t), A_2(t), ... A_k(t) por cada tipo. Las tasas de destruccion de cada subtipo se calcularian segun la vulnerabilidad diferencial del blanco. Esto es compatible con el modelo agregado si se mantiene la estructura de ODE pero con dimension mayor.

---

## DT-005 — Distancia fija durante el combate

| Campo | Valor |
|---|---|
| Prioridad | **P1** |
| Archivo | `main.cpp:368-468` |
| Tipo | Realismo del modelo |

### Descripcion

`engagement_distance_m` es constante durante toda la simulacion. En un ataque a posicion defensiva, el atacante avanza hacia el defensor: la distancia disminuye con el tiempo. Esto afecta directamente a `distance_degradation()`, que se calcula una sola vez al inicio.

### Impacto

- En ataques, la efectividad del fuego deberia aumentar a medida que el atacante se acerca. El modelo actual infravalora al atacante en combates que comienzan a larga distancia.
- En retardos, el defensor se retira (la distancia aumenta). El modelo no captura esto.

### Solucion propuesta

Hacer que `distance_m` sea funcion del tiempo: `d(t) = d_0 - v_approach * t` para ataques, con `v_approach` derivada del estado tactico. Recalcular `distance_degradation()` en cada paso del integrador.

---

## DT-006 — Terreno no afecta proteccion ni efectividad del fuego

| Campo | Valor |
|---|---|
| Prioridad | **P1** |
| Archivo | `main.cpp:159-167` |
| Tipo | Realismo del modelo |

### Descripcion

El terreno solo afecta la velocidad de desplazamiento entre posiciones. No modifica:

- Cobertura/proteccion (un combate en bosque deberia reducir la precision y la distancia efectiva).
- Observabilidad (terreno dificil limita la capacidad de adquirir blancos).
- Capacidad de maniobra durante el combate.

### Impacto

Un combate en terreno "DIFICIL" a 3000m produce las mismas tasas de destruccion que en terreno "FACIL" a 3000m. Esto no refleja la doctrina.

### Solucion propuesta

Introducir un multiplicador de terreno sobre la precision (U) y/o la degradacion por distancia. Ejemplo: terreno DIFICIL podria aplicar un factor 0.6-0.8 a la tasa convencional (reduce linea de vision, obstaculos).

---

## DT-007 — Modelo de municion C/C internamente inconsistente

| Campo | Valor |
|---|---|
| Prioridad | **P1** |
| Archivo | `main.cpp:119-124` |
| Tipo | Consistencia interna |

### Descripcion

```cpp
double ammo_factor = std::max(0.0, M - c_cc * t) / M;
return (A_current / A0) * S_cc_static * ammo_factor;
```

El `ammo_factor` modela el agotamiento de municion de **un vehiculo individual** (`M` misiles, `c_cc` disparos/min). Pero `t` es el tiempo global de combate, y `A_current/A0` es la fraccion de supervivencia del bando completo. Se mezclan escalas individual y agregada sin justificacion.

Problemas concretos:
- Si `c_cc * t > M`, el factor es 0 (correcto para un vehiculo, pero en la fuerza agregada distintos vehiculos pueden haber empezado a disparar en momentos diferentes).
- El factor `A_current/A0` asume que la capacidad C/C se degrada linealmente con las bajas, pero las bajas pueden afectar desproporcionadamente a vehiculos sin C/C (ver DT-004).

### Impacto

El agotamiento de municion C/C puede ser demasiado rapido o demasiado lento dependiendo del escenario, sin forma de predecir la direccion del sesgo.

### Solucion propuesta

Opciones por orden de complejidad:
1. Documentar la aproximacion y sus limitaciones (minimo).
2. Usar municion total de la fuerza (`M * n_cc`) y consumo total (`c_cc * n_cc_surviving * dt`) con tracking explicito.
3. Si se implementa DT-004 (destruccion por tipo), el tracking de municion por tipo se resuelve naturalmente.

---

## DT-008 — Monolito de 1130 lineas en un solo archivo

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Archivo | `main.cpp` |
| Tipo | Mantenibilidad |

### Descripcion

`main.cpp` contiene todo: estructuras de datos, modelo matematico, I/O JSON, parseo CLI, batch processing, sweep, serializacion CSV, utilidades de paths. Viola separacion de responsabilidades.

### Impacto

- Dificil de navegar, modificar y revisar.
- Imposible reutilizar la logica de simulacion como libreria.
- No se pueden compilar tests unitarios contra funciones individuales sin compilar todo.

### Solucion propuesta

Separar en modulos:
- `model.h/cpp` — funciones del modelo matematico (kill_probability, distance_degradation, static_rate_*, dynamic_rate_cc, total_rate)
- `simulation.h/cpp` — simulate_combat, run_scenario, estructuras CombatInput/CombatResult
- `catalog.h/cpp` — VehicleParams, carga de catalogos JSON, find_vehicle
- `io.h/cpp` — serializacion JSON/CSV, parseo de escenarios
- `cli.cpp` — main, parseo de argumentos, dispatch de modos

---

## DT-009 — Variable global mutable para modo de agregacion

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Archivo | `main.cpp:183` |
| Tipo | Calidad de codigo |

### Descripcion

```cpp
AggregationMode g_aggregation_mode = AggregationMode::PRE;
```

El modo de agregacion es estado global mutable. `simulate_combat()` lee implicitamente esta variable.

### Impacto

- Impide ejecutar simulaciones con diferentes modos en paralelo (relevante para comparativas automatizadas).
- Dificulta testing: los tests dependen de estado global.
- Efecto invisible: una funcion que no recibe el modo como parametro lo usa sin que el caller lo sepa.

### Solucion propuesta

Pasar `AggregationMode` como parametro de `CombatInput` o de `simulate_combat()`. Eliminar la variable global.

---

## DT-010 — `std::exit(1)` en funcion de busqueda de vehiculos

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Archivo | `main.cpp:527-530` |
| Tipo | Robustez |

### Descripcion

```cpp
std::fprintf(stderr, "Error: vehiculo '%s' no encontrado en catalogos.\n", name.c_str());
std::exit(1);
```

Si un vehiculo no existe en ningun catalogo, el proceso entero muere.

### Impacto

En modo batch con 50 escenarios, un error de tipografia en el escenario 3 aborta los 47 restantes. Los recursos de RAII no se liberan correctamente.

### Solucion propuesta

Lanzar una excepcion (`std::runtime_error`) o devolver `std::optional<VehicleParams>`. El caller (parse_composition o run_scenario) decide si abortar o continuar al siguiente escenario.

---

## DT-011 — Sin validacion de parametros de entrada

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Archivo | Parseo de escenarios JSON |
| Tipo | Robustez |

### Descripcion

No se valida que los valores del JSON esten en rangos sensatos:

- `count` podria ser negativo o cero.
- `engagement_fraction` podria ser > 1.0 o negativo.
- `aft_casualties_pct` podria ser > 1.0 o negativo.
- `rate_factor`, `count_factor` sin limites.
- Parametros de vehiculo (D, P, U, c) sin verificacion de rango.
- `h` (paso del solver) podria ser 0 o negativo (bucle infinito).

### Impacto

Un escenario mal escrito produce resultados que parecen validos pero son basura. No hay mensaje de error ni advertencia. El usuario puede confiar en resultados generados con inputs invalidos.

### Solucion propuesta

Validar en el parseo:
- `count >= 1`
- `engagement_fraction` en [0.0, 1.0]
- `aft_casualties_pct` en [0.0, 1.0]
- `rate_factor`, `count_factor` en [0.0, limite_razonable]
- `h > 0`, `t_max > 0`
- Parametros de vehiculo no negativos donde aplique
- Emitir warnings para valores en limites (ej: `engagement_fraction = 0.0` elimina toda la fuerza).

---

## DT-012 — Redondeo a 2 decimales en la salida pierde precision

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Archivo | `main.cpp:710-722` |
| Tipo | Precision |

### Descripcion

Todos los campos de salida se redondean con `std::round(x * 100) / 100`. En combates con fuerzas pequenas, 0.005 vehiculos de diferencia puede ser significativo. En modo sweep, el redondeo acumulado entre iteraciones puede ocultar tendencias reales.

### Solucion propuesta

Usar precision completa internamente y en la salida JSON (6+ decimales). Ofrecer redondeo solo para display humano (modo `--pretty` o similar). La precision de la salida no deberia ser menor que la del modelo.

---

## DT-013 — Tests no verifican contra soluciones analiticas

| Campo | Valor |
|---|---|
| Prioridad | **P2** |
| Archivo | `tests/run_validation.sh` |
| Tipo | Validacion |

### Descripcion

Los 46 tests verifican invariantes (bajas = inicial - supervivientes, supervivientes >= 0) y comportamiento cualitativo (BLUE_WINS, DRAW, etc.). Ningun test compara resultados numericos contra la solucion analitica cerrada de las ecuaciones de Lanchester.

Para el caso simetrico puro (ley cuadrada de Lanchester, sin C/C, sin multiplicadores), las ecuaciones tienen solucion exacta:

```
A(t) = A0 * cosh(sigma*t) - R0*sqrt(beta/alpha) * sinh(sigma*t)
```

donde `sigma = sqrt(alpha * beta)`, y alpha/beta son las tasas.

### Impacto

No es posible cuantificar el error numerico del integrador. Los tests validan "parece correcto" pero no "es preciso a X decimales".

### Solucion propuesta

Anadir al menos un test que compare el resultado numerico contra la solucion cerrada para un caso simetrico simple, verificando precision a 3-4 decimales significativos. Esto servira tambien para validar la mejora a RK4 (DT-001).

---

## DT-014 — `exe_directory()` no es robusto en todas las plataformas

| Campo | Valor |
|---|---|
| Prioridad | **P3** |
| Archivo | `main.cpp:991-996` |
| Tipo | Portabilidad |

### Descripcion

```cpp
std::string exe_directory(const char* argv0) {
    std::string path(argv0);
    auto pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return ".";
    return path.substr(0, pos);
}
```

`argv[0]` puede no contener el path real del ejecutable (ej: si esta en PATH, puede ser solo "lanchester" sin directorio). En ese caso devuelve "." que puede no ser el directorio del ejecutable.

### Solucion propuesta

En Linux, usar `std::filesystem::read_symlink("/proc/self/exe")` para obtener el path real del ejecutable. Mantener argv[0] como fallback para otras plataformas.

---

## DT-015 — Sweep usa comparacion de punto flotante para limites del bucle

| Campo | Valor |
|---|---|
| Prioridad | **P3** |
| Archivo | `main.cpp:954` |
| Tipo | Correccion |

### Descripcion

```cpp
for (double val = start; val <= end + step * 0.001; val += step) {
```

La acumulacion de `val += step` con punto flotante introduce error. El hack `+ step * 0.001` funciona en la mayoria de casos pero no es correcto formalmente.

### Solucion propuesta

Usar un contador entero y calcular el valor en cada iteracion:

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
| Archivo | `main.cpp:847` |
| Tipo | Limpieza de codigo |

### Descripcion

`#include <filesystem>` aparece en la linea 847, junto a la funcion `run_batch`, separado del resto de includes (lineas 5-14). Probablemente anadido en una sesion posterior sin reorganizar.

### Solucion propuesta

Mover a la zona de includes al inicio del archivo.

---

## Resumen por prioridad

| Prioridad | Items | IDs |
|---|---|---|
| P0 (Critica) | 2 | DT-001, DT-002 |
| P1 (Alta) | 5 | DT-003, DT-004, DT-005, DT-006, DT-007 |
| P2 (Media) | 6 | DT-008, DT-009, DT-010, DT-011, DT-012, DT-013 |
| P3 (Baja) | 3 | DT-014, DT-015, DT-016 |

## Orden de ejecucion recomendado

1. **DT-002** (calibrar constantes) — Sin esto, todo lo demas es optimizar un modelo de validez desconocida.
2. **DT-001** (RK4) — Eliminar la fuente de error numerico mas obvia.
3. **DT-013** (tests analiticos) — Verificar que el modelo + integrador producen resultados correctos.
4. **DT-011** (validacion de inputs) — Evitar resultados silenciosamente incorrectos.
5. **DT-004** (destruccion por tipo) — La simplificacion mas danina para el realismo.
6. **DT-005** (distancia variable) — Segunda simplificacion mas danina.
7. **DT-008** (modularizar) — Habilita desarrollo paralelo y testing granular.
8. Todo lo demas por conveniencia.

---

*Documento generado a partir de evaluacion critica del codigo. Ultima actualizacion: 2026-03-13.*
