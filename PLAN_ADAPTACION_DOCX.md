# Plan de Adaptacion al Documento Tecnico de Referencia

> **Fuente de verdad**: `Resumen_tecnico_Lancaster_ANONIMIZADO.docx`
>
> Este plan persigue la fidelidad estricta al documento del cientifico.
> Toda desviacion respecto al docx se considera un defecto a corregir,
> independientemente de si la implementacion actual es "mejor" en terminos
> numericos. El objetivo es que el codigo refleje exactamente el modelo
> descrito por el analista.

---

## Implementacion actual documentada (pre-adaptacion)

Esta seccion registra el estado de la implementacion vigente **antes** de
aplicar los cambios de adaptacion. Sirve como referencia de la alternativa
paralela: un modelo que diverge del docx pero que aporta mejoras tecnicas
validadas. Si en el futuro se decide revertir algun cambio de adaptacion,
este registro permite recuperar la logica original.

### Evaluacion global pre-adaptacion

| Dimension | Nota | Comentario |
|---|---|---|
| Ecuaciones base | 9/10 | Kill probability, degradacion, tasa de destruccion: identicas al docx |
| Metodo numerico | 7/10 | RK4 en vez de Euler (mejor, pero no es lo especificado) |
| Variables de entrada | 8/10 | Todas presentes. Default de proporcion (1.0 vs 2/3) difiere |
| Movimiento/velocidad | 5/10 | Velocidad proporcional y tiempo de desplazamiento no implementados |
| Proporcion/probabilidad estatica | 4/10 | Formula diferente, sin normalizacion, sin acotacion |
| Outputs | 7/10 | Falta devolucion de bajas a escala original (pre-proporcion) |
| Funcionalidad extra | 10/10 | Monte Carlo, POST aggregation, Poisson subdivisions: todo valor anadido |

---

### PA1. Integrador RK4 (sera sustituido por Euler en C1)

**Ubicacion**: `src/domain/square_law_model.cpp:260-288`

**Descripcion**: Runge-Kutta de orden 4 con 4 evaluaciones de pendiente
por paso temporal. Implementacion clasica del metodo:

```cpp
// Paso RK4 actual
k1a = f_A(t, A, R);
k1r = f_R(t, A, R);

A2 = max(0, A + 0.5 * h * k1a);
R2 = max(0, R + 0.5 * h * k1r);
k2a = f_A(t + 0.5*h, A2, R2);
k2r = f_R(t + 0.5*h, A2, R2);

A3 = max(0, A + 0.5 * h * k2a);
R3 = max(0, R + 0.5 * h * k2r);
k3a = f_A(t + 0.5*h, A3, R3);
k3r = f_R(t + 0.5*h, A3, R3);

A4 = max(0, A + h * k3a);
R4 = max(0, R + h * k3r);
k4a = f_A(t + h, A4, R4);
k4r = f_R(t + h, A4, R4);

A_new = max(0, A + (h/6) * (k1a + 2*k2a + 2*k3a + k4a));
R_new = max(0, R + (h/6) * (k1r + 2*k2r + 2*k3r + k4r));
```

**Propiedades tecnicas**:
- Error local: O(h⁵), error global: O(h⁴)
- Con h = 1/600 min (0.1s), error validado contra solucion analitica
  cerrada: **0.051 vehiculos** (`test_09_analytical`)
- 4x mas costoso por paso que Euler, pero permite pasos mas grandes
  para la misma precision

**Razon de la desviacion**: Mejora de precision sin coste computacional
perceptible (la simulacion completa tarda < 1ms incluso con RK4).

**Impacto de eliminar**: Con Euler explicito y el mismo h = 0.1s,
el error aumentara ~100x (de O(h⁴) a O(h)). Para mantener precision
comparable se necesitarian pasos ~10x menores (h ≈ 0.01s), lo que
multiplicaria el tiempo de simulacion por 10.

**Test afectado**: `test_square_law.cpp` — test "Validacion vs solucion
analitica" con tolerancia actual de 0.06 vehiculos. Con Euler la
tolerancia debera relajarse a ~5-10 vehiculos.

---

### PA2. count_factor (sera eliminado en C3)

**Ubicacion**: 8 ficheros a lo largo de domain, application, ui y tests.

**Descripcion**: Multiplicador de vehiculos efectivos que escala la
fuerza inicial. Firma actual:

```cpp
// combat_utils.cpp:56-58
double initialForces(int n_total, double aft_pct, double eng_frac, double cnt_fac) {
    double n = static_cast<double>(n_total);
    return (n - n * aft_pct) * eng_frac * cnt_fac;
}
```

**Rango validado**: [0.0, 10.0] (`scenario_config.cpp:21-22`).
Default: 1.0 (neutro).

**Presencia en GUI**: Slider "Factor efectivos" en el panel de
composicion de fuerzas (`gui_step_side.h:139`), rango [0.1, 3.0].

**Proposito original**: Ponderar variables no cuantificables como
entrenamiento, moral o cohesion de unidad. Permite al usuario
simular que una fuerza "vale" mas o menos que su numero nominal.

**Razon de la eliminacion**: El docx (Nota 2) lo descarta explicitamente.
El factor arbitrario de tasa de destruccion (`rate_factor`/lambda, §21)
ya cubre parcialmente esta funcion al multiplicar la tasa S.

**Test afectado**: `test_08_engagement_fraction.json` usa `count_factor: 2.0`
para validar que `10 vehiculos * 0.5 fraccion * 2.0 count = 10 iniciales`.
Este test debera eliminarse o reformularse.

---

### PA3. Ventaja estatica como ratio S*N² (sera sustituida en C4)

**Ubicacion**: `src/domain/square_law_model.cpp:216-220`

**Descripcion**: La implementacion actual calcula la ventaja estatica
usando la ley cuadratica de Lanchester clasica:

```cpp
double S_blue_t0 = totalRate(blue_rates, A0, A0, 0.0, 1.0);
double S_red_t0  = totalRate(red_rates,  R0, R0, 0.0, 1.0);
double static_adv = 0.0;
if (S_red_t0 * R0 * R0 > 0.0)
    static_adv = (S_blue_t0 * A0 * A0) / (S_red_t0 * R0 * R0);
```

**Propiedades**:
- Resultado en (0, +inf): valores > 1 favorecen azul, < 1 favorecen rojo
- Incluye tasas totales (convencional + C/C) al instante t=0
- No tiene acotacion ni normalizacion

**Diferencia con el docx**: El docx define phi (proporcion estatica)
acotada [-10, 10] y normalizada a probabilidad P_e en [0, 1].
La formula exacta no es legible (OLE). La implementacion actual
usa una formula diferente que captura el mismo concepto (ratio de
potencia de combate) pero con distinta escala y sin limites.

**Presencia en GUI**: Se muestra como "Ventaja estatica" en la tabla
de resultados (`gui_step_simulation.h:208`) con formato `%.4f`.

**Presencia en tests**: `test_square_law.cpp:92` valida que en combate
asimetrico `static_advantage > 1.0`.

---

### PA4. Velocidad directa de tabla (sera ampliada en C5)

**Ubicacion**: `src/application/simulation_service.cpp:48-54`

**Descripcion**: El calculo de velocidad de aproximacion actual:

```cpp
bool blue_attacks = (config.blue.tactical_state == lanchester::ATTACKING_STATE);
bool red_attacks  = (config.red.tactical_state  == lanchester::ATTACKING_STATE);
double v = 0;
if (blue_attacks) v += params_->tacticalSpeed(config.blue.mobility, config.terrain);
if (red_attacks)  v += params_->tacticalSpeed(config.red.mobility, config.terrain);
input.approach_speed_kmh = v;
```

**Logica**:
- Solo se mueven bandos en estado "Ataque a posicion defensiva"
- La velocidad se toma directamente de la tabla Movilidad×Terreno
- Si ambos atacan, las velocidades se suman (se acercan mutuamente)
- Si ninguno ataca, `approach_speed = 0` y la distancia permanece fija

**Diferencia con el docx**: El docx describe un sistema mas complejo:
1. Cada estado tactico tiene un booleano `movilidad` (no solo "Ataque")
2. La velocidad se modula por la proporcion estatica phi: `v_prop = v * f(phi)`
3. Se calcula un "equipo mas rapido" como output
4. Se calcula un "tiempo de desplazamiento" separado del tiempo de combate

**Lo que se pierde al adaptar**: La simplicidad del modelo actual
(si atacas te mueves, si no no) se reemplaza por un sistema acoplado
donde la velocidad depende de la ventaja tactica previa al combate.

---

### PA5. Default engagement_fraction = 1.0 (sera cambiado a 2/3 en C2)

**Ubicacion**:
- `src/application/scenario_config.h:16` — `double engagement_fraction = 1.0;`
- `src/domain/lanchester_types.h:166` — `double blue_engagement_fraction = 1.0`
- `src/ui/gui_state.h:53` — `float engagement_fraction = 1.0f;`

**Razon del valor actual**: Un default de 1.0 significa "todas las fuerzas
participan", lo cual es el caso mas simple y no sorpresivo para el
usuario. El slider GUI permite ajustar entre 0.1 y 1.0.

**Impacto del cambio a 2/3**: Todos los escenarios de test que no
especifiquen `engagement_fraction` explicitamente usaran 2/3, lo que
reducira las fuerzas iniciales un 33%. Esto afecta:
- 9 ficheros JSON de test en `src/tests/data/` que usan 1.0 explicitamente
  (estos no cambian si el valor esta escrito)
- El slider GUI debera mostrarse inicializado en 0.67 en vez de 1.0
- Cualquier simulacion con defaults dara resultados diferentes

---

### PA6. Bajas sobre fuerzas efectivas (sera ampliado en C9)

**Ubicacion**: `src/domain/square_law_model.cpp:304-309`

**Descripcion**:

```cpp
res.blue_initial    = A0;    // A0 ya tiene aplicada proporcion y AFT
res.red_initial     = R0;
res.blue_survivors  = max(0.0, A);
res.red_survivors   = max(0.0, R);
res.blue_casualties = A0 - res.blue_survivors;
res.red_casualties  = R0 - res.red_survivors;
```

**Semantica actual**: Las bajas reportadas son sobre las fuerzas que
realmente participaron en combate (post proporcion + AFT). Si de 30
vehiculos solo participaron 20 (proporcion 2/3) y murieron 5, se
reporta: `initial=20, casualties=5, survivors=15`.

**Lo que pide el docx** (§97): "Sumaremos lo que hemos restado antes
por proporcion." Es decir, el reporte deberia escalar las bajas al
total original. En el ejemplo anterior: `initial=30, casualties=5+10
(los 10 no participantes), survivors=15`.

**Implicacion**: El docx quiere que el usuario vea el impacto total
sobre su fuerza, no solo sobre la fraccion combatiente. Los no
participantes se contabilizan como "no presentes" pero no como "bajas".
La logica exacta de como sumarlos requiere definicion.

---

### PA7. Tasa C/C sin acotar (sera acotada en C6)

**Ubicacion**: `src/domain/square_law_model.cpp:54-59`

**Descripcion**:

```cpp
double SquareLawModel::dynamicRateCc(double S_cc_static, double A_current,
                                      double A0, double cc_ammo_consumed,
                                      double cc_ammo_max) const {
    if (cc_ammo_max <= 0.0 || A0 <= 0.0) return 0.0;
    double ammo_frac = std::max(0.0, cc_ammo_max - cc_ammo_consumed) / cc_ammo_max;
    return (A_current / A0) * S_cc_static * ammo_frac;
}
```

**Rango actual**: La tasa resultante es siempre >= 0 (todos los
componentes son no negativos). En la practica, los valores tipicos
estan en el rango [0, 2] kills/min/unidad. No existe escenario
realista donde la tasa C/C alcance valores cercanos a 10.

**Impacto del clamp**: Marginal. El clamp a [-10, 10] no afectara
ningun escenario realista, pero garantiza estabilidad numerica
ante parametros extremos.

---

## Cambios obligatorios

### C1. Sustituir RK4 por Euler explicito

**Referencia docx**: §83-90, Anexo 2 (§125-136)

> "Buscamos aproximar las ecuaciones de Lancaster mediante el metodo
> de Euler explicito."

**Estado actual**: `square_law_model.cpp:260-278` implementa RK4 (4 evaluaciones
de pendiente por paso: k1, k2, k3, k4).

**Cambio requerido**: Reemplazar el bucle RK4 por Euler explicito:

```
A_new = A + h * f_A(t, A, R)
R_new = R + h * f_R(t, A, R)
```

Donde `f_A = -S_red * R` y `f_R = -S_blue * A`, con `h = 1/600 min = 0.1s`.

**Ficheros afectados**:
- `src/domain/square_law_model.cpp` — metodo `simulate()`
- `src/tests/test_square_law.cpp` — test `test_09_analytical` (tolerancia de error
  cambiara: Euler tiene error O(h) vs O(h⁴) de RK4)

**Riesgo**: La precision numerica se degrada. Los tests actuales que validan
contra la solucion analitica cerrada necesitaran tolerancias mas amplias.

---

### C2. Cambiar default de engagement_fraction a 2/3

**Referencia docx**: §18

> "Proporcion: por defecto 2/3, el numero de fuerzas efectivas que
> participaran en este combate."

**Estado actual**: Default = `1.0` en:
- `src/application/scenario_config.h:16` — `double engagement_fraction = 1.0;`
- `src/domain/lanchester_types.h:166` — `double blue_engagement_fraction = 1.0`
- `src/ui/gui_state.h:53` — `float engagement_fraction = 1.0f;`

**Cambio requerido**: Cambiar todos los defaults a `2.0/3.0` (~0.6667).

**Ficheros afectados**:
- `src/application/scenario_config.h`
- `src/domain/lanchester_types.h`
- `src/ui/gui_state.h`
- `src/tests/data/*.json` — revisar todos los escenarios de test que asumen 1.0

---

### C3. Eliminar count_factor

**Referencia docx**: Nota 2 (§101)

> "El factor arbitrario de vehiculos, que multiplica el numero de
> vehiculos, se descarta por el momento."

**Estado actual**: `count_factor` esta implementado y expuesto en:
- `src/domain/combat_utils.cpp:58` — `return (n - n * aft_pct) * eng_frac * cnt_fac;`
- `src/domain/combat_utils.h:13` — parametro `cnt_fac`
- `src/domain/lanchester_types.h:168` — `double blue_count_factor = 1.0`
- `src/application/scenario_config.h:18` — `double count_factor = 1.0;`
- `src/application/scenario_config.cpp:21-22` — validacion
- `src/application/simulation_service.cpp:40-41` — asignacion
- `src/ui/gui_state.h:55` — `float count_factor = 1.0f;`
- `src/ui/gui_step_side.h:139` — slider GUI

**Cambio requerido**: Eliminar `count_factor` de toda la cadena:
- Eliminar parametro de `initialForces()`
- Eliminar campo de `CombatInput`, `SideConfig`, `SideGuiState`
- Eliminar slider de la GUI
- Eliminar validacion en `scenario_config.cpp`
- Eliminar asignacion en `simulation_service.cpp`
- Limpiar ficheros JSON de test

**Ficheros afectados**: 8+ ficheros en domain, application, ui, tests.

---

### C4. Implementar proporcion estatica (phi) segun docx

**Referencia docx**: §41-46

> "Proporcion estatica (phi): es una medida derivada que permite evaluar
> el exito de una mision a priori. [...] acotada entre [-10 y 10].
> La probabilidad estatica (P_e) la definimos normalizando phi."

**Estado actual**: `square_law_model.cpp:218-220` calcula:
```cpp
static_adv = (S_blue_t0 * A0 * A0) / (S_red_t0 * R0 * R0);
```
Ratio sin acotar, sin normalizacion. No es la formula del docx.

**Cambio requerido**:

1. Calcular phi como define el docx (ratio de tasas de destruccion
   incluyendo C/C), acotada a [-10, 10]:
   ```
   phi_raw = S_blue_total / S_red_total   (o la formula exacta del docx)
   phi = clamp(phi_raw, -10.0, 10.0)
   ```

2. Normalizar phi a probabilidad estatica P_e en [0, 1]:
   ```
   P_e = (phi + 10) / 20    (normalizacion lineal simple)
   ```
   Nota: la formula exacta de normalizacion no es legible en el docx
   (ecuaciones embebidas como objetos OLE). Se debe **confirmar con el
   cientifico** la formula de normalizacion antes de implementar.

3. Renombrar `static_advantage` a `proporcion_estatica` y anadir campo
   `probabilidad_estatica` en `CombatResult`.

4. Devolver P_e como **primera salida** de la simulacion (§92: "Primero
   la probabilidad estatica, por lo que no hace falta ni ejecutar la
   simulacion").

**Ficheros afectados**:
- `src/domain/lanchester_types.h` — struct CombatResult
- `src/domain/square_law_model.cpp` — simulate() y simulateStochastic()
- `src/ui/gui_step_simulation.h` — mostrar P_e antes de ejecutar simulacion

**Dependencia**: Requiere clarificacion del cientifico sobre la formula
exacta de normalizacion (contenido OLE no legible en el .docx).

---

### C5. Implementar velocidad proporcional

**Referencia docx**: §53-73

> "La velocidad proporcional depende de la velocidad y de la proporcion
> estatica. [...] Datos.Situacion.movilidad es una columna de la Tabla
> Situacion que indica si una Situacion de combate permite la movilidad
> o no. Es un booleano."

**Estado actual**: `simulation_service.cpp:49-54` usa velocidad directa
de la tabla Movilidad×Terreno, solo cuando el estado tactico es
"Ataque a posicion defensiva". No existe concepto de velocidad proporcional
ni booleano de movilidad por situacion.

**Cambio requerido**:

1. Anadir booleano `mobility_allowed` a cada estado tactico en
   `model_params.json`:
   ```json
   "Ataque a posicion defensiva": { "self": 1.0, "opponent": 1.0, "mobility": true },
   "Defensiva condiciones minimas": { "self": 1.0, "opponent_base": 2.25, "mobility": false },
   ...
   ```

2. Calcular velocidad proporcional segun docx:
   ```
   v_prop_blue = v_blue * f(phi)     si mobility_allowed_blue
   v_prop_blue = 0                   si no
   ```
   Donde `f(phi)` es una funcion de la proporcion estatica. La formula
   exacta no es completamente legible en el docx (§65-67 contienen
   ecuaciones OLE). Se debe **confirmar con el cientifico**.

3. Usar `v_prop` en vez de `v` para el calculo de aproximacion.

**Ficheros afectados**:
- `data/model_params.json` — anadir campo `mobility` por estado tactico
- `src/domain/model_params.h/cpp` — parsear y exponer `mobility_allowed`
- `src/application/simulation_service.cpp` — calcular v_prop
- `src/domain/square_law_model.cpp` — usar v_prop en la simulacion

**Dependencia**: Requiere clarificacion del cientifico sobre la formula
exacta de `f(phi)`.

---

### C6. Acotar tasa de destruccion C/C a [-10, 10]

**Referencia docx**: §39

> "NUEVO: Acotada entre [-10 y 10]."

**Estado actual**: `square_law_model.cpp:54-59` en `dynamicRateCc()` no
tiene acotacion.

**Cambio requerido**: Anadir clamp al resultado de `dynamicRateCc()`:
```cpp
return std::clamp(result, -10.0, 10.0);
```

**Fichero afectado**: `src/domain/square_law_model.cpp`

---

### C7. Implementar tiempo de desplazamiento como output separado

**Referencia docx**: §78-82

> "Devuelve el tiempo dedicado al desplazamiento de las unidades."

**Estado actual**: El desplazamiento esta embebido en la simulacion
(la distancia decrece con t), pero no se reporta como metrica separada.

**Cambio requerido**:

1. Calcular tiempo de desplazamiento:
   ```
   t_desplazamiento = distancia_inicial / velocidad_equipo_mas_rapido
   ```
   Nota del cientifico (§107): "Extranamente, el tiempo de
   desplazamiento solo tiene en cuenta la velocidad del mejor equipo."
   Implementar tal cual dice el docx pese a la reserva del analista.

2. Anadir campo `displacement_time_minutes` a `CombatResult`.

3. Anadir `duration_total_minutes = displacement_time + duration_contact_minutes`.

**Ficheros afectados**:
- `src/domain/lanchester_types.h` — anadir campo
- `src/domain/square_law_model.cpp` — calcular y asignar
- `src/ui/gui_step_simulation.h` — mostrar en resultados

---

### C8. Implementar "equipo mas rapido" como output

**Referencia docx**: §75-76

> "Basicamente devuelve si el equipo Azul o Rojo es mas rapido."

**Estado actual**: No existe como output.

**Cambio requerido**: Anadir campo `faster_team` (enum: BLUE/RED/EQUAL)
a `CombatResult`, calculado comparando las velocidades proporcionales
de ambos bandos.

**Ficheros afectados**:
- `src/domain/lanchester_types.h`
- `src/domain/square_law_model.cpp`

---

### C9. Devolver bajas a escala original (pre-proporcion)

**Referencia docx**: §97

> "Al devolver las bajas en cada bando, sumaremos lo que hemos restado
> antes por proporcion."

**Estado actual**: `square_law_model.cpp:308-309`:
```cpp
res.blue_casualties = A0 - res.blue_survivors;
res.red_casualties  = R0 - res.red_survivors;
```
Donde A0 y R0 ya tienen aplicada proporcion y AFT. Las bajas reportadas
son solo sobre fuerzas efectivas, no sobre el total original.

**Cambio requerido**:

1. Preservar `n_total_original` (antes de aplicar proporcion/AFT) en el
   flujo de simulacion.

2. Calcular bajas totales:
   ```
   bajas_reportadas = bajas_combate + bajas_por_proporcion_no_participante
   ```
   O alternativamente:
   ```
   blue_casualties_total = n_total_blue_original - blue_survivors
   ```

3. Anadir campos `blue_initial_total` / `red_initial_total` a CombatResult
   para distinguir fuerzas totales de fuerzas efectivas.

**Ficheros afectados**:
- `src/domain/lanchester_types.h` — nuevos campos
- `src/domain/square_law_model.cpp` — recibir y usar n_total_original
- `src/domain/combat_utils.h/cpp` — pasar n_total_original
- `src/application/simulation_service.cpp` — pasar n_total a CombatInput

---

## Elementos a conservar sin cambio

Los siguientes aspectos del codigo **ya son fieles al docx**:

| Elemento | Referencia docx | Estado |
|---|---|---|
| Parametros de vehiculo (D, P, U, c, A_max, f, CC, M...) | §3-6 | Correcto |
| Kill probability sigmoide `1/(1+exp((D-P)/slope))` | §27 | Correcto |
| Degradacion por distancia: polinomio acotado [0,1], 0 si d > A_max | §29-31 | Correcto |
| Tasa de destruccion convencional `S = T * G * U * c` | §32-33 | Correcto |
| Tasa C/C con municion finita decreciente | §34-38 | Correcto |
| Agregacion por media ponderada (modo PRE) | §25 | Correcto |
| 3 terrenos: FACIL, MEDIO, DIFICIL | §9 | Correcto |
| 4 movilidades: MUY_ALTA, ALTA, MEDIA, BAJA | §8 | Correcto |
| 8 estados tacticos | §10 | Correcto |
| Tabla de velocidades Movilidad×Terreno | §57-61 | Correcto |
| Timestep h = 0.1s (1/600 min) | §84 | Correcto |
| Condicion de parada: A o R llega a 0 | §90 | Correcto |
| Variables por bando independientes | §23 | Correcto |
| Proporcion y AFT como variables avanzadas | §17-19 | Correcto |
| Factor arbitrario de tasa de destruccion (rate_factor/lambda) | §21 | Correcto |

---

## Elementos extra no contemplados en el docx

Los siguientes elementos **no estan en el docx**. Se conservan como
extensiones, pero deben marcarse claramente en la UI y documentacion
como funcionalidad **fuera de especificacion**:

| Elemento | Justificacion para conservar |
|---|---|
| **Monte Carlo (Poisson estocastico)** | Docx lo menciona como mejora futura (Nota 1, §100). Ya implementado. Conservar pero marcar como "Extension: no forma parte del modelo base." |
| **Agregacion POST** | No mencionada en el docx. Atiende la preocupacion de Nota 3 (§102). Conservar como opcion avanzada, default debe ser PRE (media ponderada). |
| **Subdivision adaptativa de Poisson** | Mejora interna de fidelidad estadistica. Sin impacto en la interfaz. Conservar. |
| **count_factor** | Eliminado por C3. Si en el futuro el cientifico lo reintroduce, se reimplementa. |

---

## Estado de implementacion

Todos los cambios C1-C9 han sido implementados. Las formulas OLE se
extrajeron del XML interno del docx (116 ecuaciones OMML).

```
FASE 1 — COMPLETADA
├── C2. Default engagement_fraction = 2/3               ✓
├── C3. Eliminar count_factor                           ✓
├── C6. Acotar tasa C/C a [-10, 10]                     ✓
└── C1. Euler explicito en vez de RK4                   ✓

FASE 2 — COMPLETADA
├── C9. Devolver bajas a escala original                ✓
├── C7. Tiempo de desplazamiento como output            ✓
└── C8. Equipo mas rapido como output                   ✓

FASE 3 — COMPLETADA (con reservas documentadas)
├── C4. Proporcion estatica (phi) y probabilidad (P_e)  ✓  (ver DT-022)
├── C5. Velocidad proporcional + mobility_allowed       ✓  (ver DT-021)
└── Fix. dynamicRateCc sin A/A0, ammo temporal          ✓

DOCUMENTACION — COMPLETADA
├── README.md actualizado (15 discrepancias corregidas) ✓
├── DEUDA_TECNICA.md (DT-021, DT-022 añadidos)         ✓
├── PLAN_DE_PRUEBAS.md (count_factor eliminado)         ✓
└── PLAN_INTERFAZ_GRAFICA.md (RK4→Euler, slider)       ✓
```

27/27 tests pasan tras todos los cambios.

---

## Deuda tecnica pendiente de clarificacion

Dos formulas del docx presentan ambiguedad en la extraccion OMML.
Se han implementado con la interpretacion mas razonable, pero requieren
confirmacion del cientifico. Ver `DEUDA_TECNICA.md`:

| ID | Descripcion | Impacto |
|---|---|---|
| DT-021 | Velocidad proporcional: `0.1*phi - 8` siempre negativo → v=0. Probable "0.9" en vez de "9". | P1: ningun bando se mueve |
| DT-022 | Normalizacion phi: implementado `(phi+10)/20` pero OMML muestra `(phi-10)/20`. | P2: signo podria estar invertido |

---

*Documento generado a partir del analisis cruzado entre
`Resumen_tecnico_Lancaster_ANONIMIZADO.docx` y el codigo fuente
del proyecto LanchesterAPP. Ultima actualizacion: 2026-04-01.*
