# Deuda Tecnica — Modelo Lanchester-CIO

Registro de decisiones de diseno, simplificaciones aceptadas y deuda tecnica
identificada durante la validacion del plan. Cada item incluye severidad,
estado y, cuando aplica, la resolucion adoptada.

---

## Indice de severidad

| Nivel | Significado |
|---|---|
| **CRITICO** | Bloquea la implementacion o produce resultados incorrectos |
| **ALTO** | Ambiguedad que puede llevar a implementacion divergente del objetivo |
| **MEDIO** | Simplificacion aceptada conscientemente; puede revisarse en el futuro |
| **BAJO** | Detalle menor de documentacion o conveniencia |

---

## DT-001 — Division por cero en agregacion C/C
**Severidad:** CRITICO
**Origen:** Agregacion de fuerzas mixtas
**Descripcion:** La formula `P_param_cc = SUM(n_i * param_cc_i) / SUM(n_i * CC_i)` produce division por cero cuando ninguna unidad del bando tiene capacidad contracarro (`CC_i = 0` para todo `i`).
**Resolucion:** Si `SUM(n_i * CC_i) = 0`, toda la rama C/C del bando se desactiva: `S_cc_static = 0`, `n_cc = 0`. No se calculan parametros agregados C/C.
**Estado:** RESUELTO — incorporado al plan.

---

## DT-002 — `rate_factor` sin formula de aplicacion
**Severidad:** CRITICO
**Origen:** Formato de entrada / Modelo matematico
**Descripcion:** El campo `rate_factor` aparece en el JSON de entrada con rango [0.0-2.0] y valor por defecto 1.0, pero no se referencia en ninguna formula del modelo.
**Resolucion:** Se aplica como multiplicador directo de la tasa de destruccion efectiva total del bando: `S_total_efectiva = (S_conv_efectiva + S_cc(t)) * rate_factor`. Actua despues de los multiplicadores tacticos y antes del bucle Euler.
**Estado:** RESUELTO — incorporado al plan.

---

## DT-003 — Inconsistencia de nombre: `factor_vehiculos` vs `count_factor`
**Severidad:** CRITICO
**Origen:** Bucle de integracion / Formato de entrada
**Descripcion:** La formula de fuerzas iniciales usa `factor_vehiculos`, pero el JSON de entrada define el campo como `count_factor`.
**Resolucion:** Se unifica el nombre como `count_factor` en todo el plan, formulas y codigo.
**Estado:** RESUELTO — incorporado al plan.

---

## DT-004 — Funcion de degradacion G_cc para el canal contracarro
**Severidad:** ALTO
**Origen:** Funciones del modelo
**Descripcion:** El plan indica que `G_cc` se calcula con "parametros C/C" pero no especifica si usa la misma funcion polinomial `g()` con parametros diferentes o una funcion distinta.
**Resolucion:** `G_cc` usa la misma funcion polinomial `g(d, f_cc)` con el factor de distancia C/C (`f_cc`). El corte por alcance usa `A_cc`: si `d > A_cc`, entonces `g_cc = 0`. Despues se aplica `G_cc = clamp(g_cc, 0.0, 1.0)`.
**Estado:** RESUELTO — incorporado al plan.

---

## DT-005 — Punteria (U) ausente en tasa C/C
**Severidad:** ALTO
**Origen:** Funciones del modelo
**Descripcion:** La tasa convencional incluye punteria (`S = T * G * U * c`), pero la tasa C/C no (`S_cc = c_cc * T_cc * G_cc`). No queda claro si es intencional.
**Resolucion:** Intencional. Los sistemas contracarro modelados son misiles guiados cuya probabilidad de impacto esta absorbida en `T_cc` (la sigmoide ya modela si el impacto destruye). La punteria `U` del vehiculo aplica solo al armamento convencional (canon, ametralladora). Si en el futuro se modelan sistemas C/C no guiados, se anadira un parametro `U_cc`.
**Estado:** RESUELTO — documentado como decision de diseno.

---

## DT-006 — Tipo de agregacion: pre-tasa vs post-tasa
**Severidad:** ALTO
**Origen:** Agregacion de fuerzas mixtas
**Descripcion:** La media ponderada de parametros **antes** de calcular la tasa (pre-tasa) produce resultados matematicamente diferentes a calcular tasas individuales por tipo y sumarlas (post-tasa), debido a la no-linealidad de la sigmoide `T`. El plan no define cual es la opcion principal.
**Resolucion:** La implementacion principal usa **agregacion pre-tasa** (media ponderada de parametros, una unica tasa). Es el enfoque mas simple y coherente con el plan original. En la Sesion 3, se implementara opcionalmente la agregacion post-tasa como modo alternativo (`--aggregation post`) para documentar las diferencias. El modo por defecto es pre-tasa.
**Estado:** RESUELTO — incorporado al plan.

---

## DT-007 — Definicion de DRAW vs INDETERMINATE
**Severidad:** ALTO
**Origen:** Formato de salida
**Descripcion:** No se define la diferencia entre los outcomes `DRAW` e `INDETERMINATE`.
**Resolucion:**
- `BLUE_WINS`: `R(t) = 0` y `A(t) > 0`
- `RED_WINS`: `A(t) = 0` y `R(t) > 0`
- `DRAW`: ambas fuerzas llegan a 0 en el mismo paso temporal (o ambas < 0.5 en el mismo paso)
- `INDETERMINATE`: se alcanza `t_max` sin que ninguna fuerza llegue a 0
**Estado:** RESUELTO — incorporado al plan.

---

## DT-008 — `static_advantage` con componente C/C variable
**Severidad:** ALTO
**Origen:** Formato de salida
**Descripcion:** `static_advantage = (S_azul * A0^2) / (S_rojo * R0^2)` no tiene sentido claro cuando S_cc varia en el tiempo.
**Resolucion:** `static_advantage` se calcula usando la tasa total en t=0 con todas las unidades vivas: `S_total_t0 = S_conv_efectiva + S_cc_static`. Esto da una fotografia del balance de fuerzas al inicio del combate, que es la interpretacion mas util del indicador.
**Estado:** RESUELTO — incorporado al plan.

---

## DT-009 — Consumo de municion convencional no formulado
**Severidad:** MEDIO
**Origen:** Formato de salida
**Descripcion:** Los campos `blue_ammo_consumed` / `red_ammo_consumed` aparecen en la salida pero no se define su formula de calculo.
**Resolucion:** Se acumula durante el bucle Euler: `ammo_conv += c_agregada * N_vivos(t) * h` en cada paso. Para C/C: `ammo_cc += c_cc_agregada * N_cc_vivos(t) * h`, con tope en `M * N_cc_inicial`.
**Estado:** RESUELTO — incorporado al plan.

---

## DT-010 — Sobreestimacion del consumo de municion C/C
**Severidad:** MEDIO
**Origen:** Funciones del modelo
**Descripcion:** La formula `S_cc(t,A) = ... * max(0, M - c_cc * t) / M` usa `t` absoluto, asumiendo que todos los vehiculos iniciales disparan continuamente. En realidad, los vehiculos destruidos dejan de consumir municion, lo que hace que la formula sobreestime el consumo y subestime la tasa C/C en fases avanzadas.
**Resolucion:** Simplificacion aceptada. El factor `(A/A0)` ya atenua parcialmente este efecto. Corregir esto requeriria tracking individual de municion por vehiculo, lo que rompe el modelo agregado. Se documenta como limitacion conocida. Posible mejora futura: usar `c_cc * t_efectivo` donde `t_efectivo = integral(A(s)/A0, 0, t)`.
**Estado:** ACEPTADO como simplificacion. Documentado.

---

## DT-011 — Campo `aft_received` sin uso en formulas
**Severidad:** MEDIO
**Origen:** Formato de entrada
**Descripcion:** `aft_received` (numero de AFTs recibidas) aparece en el JSON pero solo se usa `aft_casualties_pct` (porcentaje de bajas por AFT) en las formulas.
**Resolucion:** `aft_received` es un campo informativo/de trazabilidad para el analista. No se usa en el calculo; el usuario introduce directamente el porcentaje de bajas estimado. Se mantiene en el JSON para documentacion del escenario y posible uso futuro (modelo de letalidad AFT).
**Estado:** ACEPTADO como campo informativo.

---

## DT-012 — Formato de refuerzos no especificado
**Severidad:** MEDIO
**Origen:** Formato de entrada / Encadenamiento de combates
**Descripcion:** Los arrays `reinforcements_blue` / `reinforcements_red` aparecen vacios en el ejemplo pero no se define su estructura.
**Resolucion:** Misma estructura que `composition`: array de objetos `{"vehicle": "NOMBRE", "count": N}`. Los refuerzos se suman a los supervivientes del combate anterior. La municion C/C de los refuerzos es completa (M misiles por vehiculo).
**Estado:** RESUELTO — incorporado al plan.

---

## DT-013 — Seleccion de catalogo de vehiculos
**Severidad:** BAJO
**Origen:** Estructura del proyecto
**Descripcion:** No se especifica como el programa asocia cada catalogo a un bando.
**Resolucion:** Por convencion: `vehicle_db.json` contiene vehiculos del bando azul (propios), `vehicle_db_en.json` contiene vehiculos del bando rojo (enemigo). El programa busca cada nombre de vehiculo primero en el catalogo de su bando; si no lo encuentra, busca en el otro. Esto permite escenarios "blue vs blue" o vehiculos capturados.
**Estado:** RESUELTO — incorporado al plan.

---

## DT-014 — Inclusion de nlohmann/json
**Severidad:** BAJO
**Origen:** Estructura del proyecto
**Descripcion:** No se indica si la libreria se incluye como fichero local o como dependencia del sistema.
**Resolucion:** Se incluye como header local (`include/nlohmann/json.hpp`) descargado directamente del release oficial. El Makefile referencia la ruta local. Cero dependencias de sistema mas alla del compilador C++17.
**Estado:** RESUELTO — incorporado al plan.

---

## DT-015 — Formato CSV no definido
**Severidad:** BAJO
**Origen:** Interfaz CLI
**Descripcion:** No se definen cabeceras, separador ni encoding para la salida CSV.
**Resolucion:** Separador: `;` (punto y coma, compatibilidad con Excel en locales europeos). Encoding: UTF-8. Primera fila: cabeceras con los nombres de los campos de salida. Para `--batch`: una fila por escenario. Para `--sweep`: una fila por valor del parametro barrido, con columna adicional para el valor del parametro.
**Estado:** RESUELTO — incorporado al plan.

---

## DT-016 — Tabla de velocidades tacticas no definida
**Severidad:** BAJO
**Origen:** Encadenamiento de combates
**Descripcion:** La formula de desplazamiento usa `velocidad_tactica_kmh` que depende del terreno y la movilidad, pero no se da la tabla de valores.
**Resolucion:** Tabla de velocidades tacticas (km/h):

| Movilidad \ Terreno | FACIL | MEDIO | DIFICIL |
|---|---|---|---|
| MUY_ALTA | 40 | 25 | 12 |
| ALTA | 30 | 20 | 10 |
| MEDIA | 20 | 12 | 6 |
| BAJA | 10 | 6 | 3 |

Se usa la velocidad de la fuerza **mas lenta** del enfrentamiento (conservador: el combate ocurre cuando ambos llegan).
**Estado:** RESUELTO — incorporado al plan.

---

## DT-017 — Parser de paths JSON para --sweep
**Severidad:** BAJO
**Origen:** Interfaz CLI
**Descripcion:** `--sweep blue.composition[0].count 1 20 1` requiere un parser de paths JSON con notacion de punto y corchetes.
**Resolucion:** Implementar un parser minimo que soporte: acceso por clave (`field.subfield`), acceso por indice de array (`field[N]`), y combinaciones (`field.array[N].subfield`). Alcance limitado a los campos del JSON de entrada. No se soportan wildcards ni expresiones complejas.
**Estado:** RESUELTO — complejidad asumida en Sesion 4.

---

## DT-018 — Agregacion pre-tasa sobreestima efectividad en fuerzas mixtas heterogeneas
**Severidad:** MEDIO
**Origen:** Validacion Sesion 3 (TEST-07)
**Descripcion:** La agregacion pre-tasa (por defecto) promedia parametros antes de aplicar la sigmoide `T`. Por la desigualdad de Jensen, cuando la sigmoide opera en zona convexa (`P < D`), `T(mean(P)) > mean(T(P))`. Esto **sobreestima** la efectividad de bandos que mezclan vehiculos con potencias muy distintas. En TEST-07 (LEO2E+PIZARRO vs T-80U+BMP-3) la diferencia cambia el vencedor: pre-tasa da BLUE_WINS (1.82 superv.), post-tasa da RED_WINS (4.37 superv.).
**Resolucion:** Implementado `--aggregation post` como modo alternativo. El modo por defecto sigue siendo pre-tasa por compatibilidad con el modelo original. El usuario debe ser consciente de esta limitacion al analizar fuerzas con vehiculos muy heterogeneos.
**Estado:** RESUELTO — modo post-tasa disponible. Documentado en VALIDACION_SESION3.md.

---

## DT-019 — Encadenamiento de combates: proporcion de tipos se mantiene constante
**Severidad:** MEDIO
**Origen:** Implementacion Sesion 2
**Descripcion:** En el encadenamiento de combates, las bajas se aplican proporcionalmente a toda la fuerza agregada. No se modela que tipo de vehiculo sufre mas bajas (los mas debiles deberian destruirse primero). La composicion de la fuerza se mantiene constante entre combates para la agregacion de parametros.
**Resolucion:** Simplificacion aceptada. Modelar destruccion selectiva por tipo requeriria multiples ecuaciones Euler simultaneas (una por tipo de vehiculo), lo que cambia fundamentalmente la arquitectura del modelo agregado.
**Estado:** ACEPTADO como simplificacion.

---

## Resumen

| Severidad | Total | Resueltos | Aceptados | Pendientes |
|---|---|---|---|---|
| CRITICO | 3 | 3 | 0 | 0 |
| ALTO | 5 | 5 | 0 | 0 |
| MEDIO | 6 | 2 | 4 | 0 |
| BAJO | 5 | 5 | 0 | 0 |
| **Total** | **19** | **15** | **4** | **0** |

Todos los items estan resueltos o aceptados como simplificaciones documentadas.
No hay deuda tecnica pendiente que bloquee la implementacion.
