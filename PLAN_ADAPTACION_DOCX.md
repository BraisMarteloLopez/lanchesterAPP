# Plan de Adaptacion al Documento Tecnico de Referencia

> **Fuente de verdad**: `Resumen_tecnico_Lancaster_ANONIMIZADO.docx`
>
> Este plan persigue la fidelidad estricta al documento del cientifico.
> Toda desviacion respecto al docx se considera un defecto a corregir,
> independientemente de si la implementacion actual es "mejor" en terminos
> numericos. El objetivo es que el codigo refleje exactamente el modelo
> descrito por el analista.

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

## Dependencias externas (requieren consulta al cientifico)

Varios cambios dependen de formulas que aparecen como objetos OLE
(ecuaciones de Word) no legibles en extraccion de texto:

| Cambio | Formula necesaria | Seccion docx |
|---|---|---|
| C4 | Normalizacion de phi a probabilidad estatica P_e | §45-46 |
| C5 | Formula de velocidad proporcional `v_prop = v * f(phi)` | §63-67 |
| C5 | Listado de que estados tacticos permiten movilidad | §69 |
| C7 | Formula exacta de tiempo de desplazamiento | §78-82 |

**Accion requerida**: Solicitar al cientifico las formulas en texto plano
o imagen, o acceso al Excel de referencia mencionado en el docx.

---

## Orden de ejecucion recomendado

Orden por dependencias tecnicas y riesgo:

```
FASE 1 — Sin dependencias externas (ejecutable ya)
├── C2. Default engagement_fraction = 2/3
├── C3. Eliminar count_factor
├── C6. Acotar tasa C/C a [-10, 10]
└── C1. Euler explicito en vez de RK4

FASE 2 — Requiere clarificacion parcial del cientifico
├── C9. Devolver bajas a escala original
├── C7. Tiempo de desplazamiento como output
└── C8. Equipo mas rapido como output

FASE 3 — Requiere formulas del cientifico (OLE)
├── C4. Proporcion estatica (phi) y probabilidad estatica (P_e)
└── C5. Velocidad proporcional con booleano de movilidad
```

---

## Criterio de aceptacion

Un cambio se considera completo cuando:

1. La implementacion reproduce el comportamiento descrito en el docx
2. Los tests unitarios validan el comportamiento esperado
3. El README se actualiza para reflejar el cambio
4. Si el cambio afecta a la GUI, la interfaz lo refleja

**Criterio global**: El proyecto se considera adaptado cuando los 9
cambios (C1-C9) estan implementados y validados, y las extensiones
(Monte Carlo, POST) estan marcadas como tal en la documentacion.

---

*Documento generado a partir del analisis cruzado entre
`Resumen_tecnico_Lancaster_ANONIMIZADO.docx` y el codigo fuente
del proyecto LanchesterAPP.*
