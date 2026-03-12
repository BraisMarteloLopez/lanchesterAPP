# Informe de Validacion — Plan Lanchester-CIO

## Resumen ejecutivo

El plan es **solido y viable**. El modelo matematico esta bien definido, la estructura es clara y las sesiones de desarrollo estan bien secuenciadas. Se identifican **3 problemas que requieren resolucion antes de implementar**, **5 ambiguedades que necesitan clarificacion**, y **4 observaciones menores**.

---

## Hallazgos criticos (resolver antes de implementar)

### C1. Division por cero en agregacion C/C
**Seccion:** Agregacion de fuerzas mixtas
**Formula:** `P_param_cc = SUM(n_i * param_cc_i) / SUM(n_i * CC_i)`
**Problema:** Si ninguna unidad del bando tiene capacidad C/C (`SUM(n_i * CC_i) = 0`), se produce una division por cero.
**Solucion propuesta:** Si `SUM(n_i * CC_i) = 0`, la tasa C/C del bando es 0 (sin calculo de agregacion).

### C2. `rate_factor` sin formula de aplicacion
**Seccion:** Formato de entrada
**Problema:** El campo `rate_factor` aparece en el JSON de entrada con rango [0.0-2.0] pero no se menciona en ninguna formula del modelo. No queda claro donde se aplica.
**Solucion propuesta:** Documentar que `S_efectiva *= rate_factor` (se aplica como multiplicador directo de la tasa de destruccion de ese bando).

### C3. `count_factor` vs `factor_vehiculos` — inconsistencia de nombres
**Seccion:** Bucle de integracion / Formato de entrada
**Problema:** La formula del bucle usa `factor_vehiculos` pero el JSON de entrada define el campo como `count_factor`. Son el mismo valor pero con nombres distintos.
**Solucion propuesta:** Unificar como `count_factor` en la formula y en el codigo.

---

## Ambiguedades (clarificar antes o durante la implementacion)

### A1. Funcion de degradacion G_cc para el canal contracarro
**Seccion:** Funciones del modelo
**Problema:** Se dice que `G_cc` se calcula con "parametros C/C" pero no queda explicito si usa la misma funcion polinomial `g()` con `f_cc` y `A_cc` como parametros, o una funcion diferente.
**Recomendacion:** Confirmar que `G_cc = clamp(g(d, f_cc), 0, 1)` con corte en `A_cc` en lugar de `A_max`.

### A2. Punteria (U) ausente en tasa C/C
**Seccion:** Funciones del modelo
**Problema:** `S = T * G * U * c` incluye punteria, pero `S_cc = c_cc * T_cc * G_cc` no. Si los misiles contracarro son guiados (U~1.0), puede ser intencional.
**Recomendacion:** Documentar explicitamente que la punteria C/C se asume 1.0 (misil guiado), o agregar `U_cc` como parametro.

### A3. Tipo de agregacion: pre-tasa vs post-tasa
**Seccion:** Agregacion de fuerzas mixtas
**Problema:** La media ponderada de parametros **antes** de calcular la tasa (pre-tasa) produce resultados diferentes a sumar las tasas individuales (post-tasa) debido a la no-linealidad de la sigmoide T. La Sesion 3 pide documentar diferencias pero no define las opciones.
**Recomendacion:** Definir las dos opciones:
- **Opcion A (pre-tasa, actual):** agregar parametros, calcular una tasa unica.
- **Opcion B (post-tasa):** calcular tasa por tipo de vehiculo, sumar tasas ponderadas.
- Implementar ambas y comparar en Sesion 3.

### A4. Definicion de DRAW vs INDETERMINATE
**Seccion:** Formato de salida
**Problema:** No se define la diferencia entre estos dos outcomes.
**Recomendacion:**
- `DRAW`: ambas fuerzas llegan a 0 en el mismo paso temporal.
- `INDETERMINATE`: se alcanza `t_max` sin que ninguna fuerza llegue a 0.

### A5. Composicion de `static_advantage` con C/C
**Seccion:** Formato de salida
**Formula:** `static_advantage = (S_azul * A0^2) / (S_rojo * R0^2)`
**Problema:** S_cc varia con el tiempo, asi que no puede incluirse en un calculo "estatico" limpio.
**Recomendacion:** Calcular `static_advantage` solo con la componente convencional `S`, o usar `S + S_cc_static` (tasa C/C en t=0 con todos los vehiculos vivos).

---

## Observaciones menores

### O1. Consumo de municion convencional no formulado
El campo `blue_ammo_consumed` / `red_ammo_consumed` aparece en la salida pero no se da su formula. Sugerencia: integrar el consumo durante el bucle como `ammo += cadencia * n_vivos * h` por paso.

### O2. Consumo de municion C/C sobreestimado en la formula dinamica
La formula `S_cc(t,A) = ... * max(0, M - c_cc * t) / M` usa `t` absoluto, asumiendo que todos los vehiculos iniciales disparan continuamente. En realidad, los vehiculos destruidos dejan de consumir municion. Esto es una simplificacion conservadora aceptable, pero deberia documentarse.

### O3. Campo `aft_received` sin uso en formulas
El campo `aft_received` aparece en el JSON de entrada pero no se usa en ninguna ecuacion. Solo se usa `aft_casualties_pct`. Clarificar si es informativo o si deberia alimentar algun calculo.

### O4. Formato de refuerzos no especificado
Los arrays `reinforcements_blue` / `reinforcements_red` aparecen vacios en el ejemplo. Falta definir su estructura. Sugerencia: misma estructura que `composition` (array de `{vehicle, count}`).

---

## Faltantes menores para la implementacion

| Item | Detalle |
|---|---|
| Seleccion de catalogo | No se especifica como el programa sabe que `vehicle_db.json` es para azul y `vehicle_db_en.json` para rojo. Sugerencia: hardcodear por convencion o parametrizar en CLI. |
| Inclusion de nlohmann/json | No se indica si se incluye como header local (`json.hpp` en el directorio) o como dependencia del sistema. Sugerencia: incluirlo como header local y documentar en el Makefile. |
| Formato CSV | No se definen cabeceras, separador ni encoding para la salida CSV de `--batch` y `--sweep`. |
| Velocidades tacticas | La formula de desplazamiento usa `velocidad_tactica_kmh` que "depende del terreno y la movilidad", pero no se da la tabla de valores. |

---

## Validacion de coherencia entre sesiones

| Sesion | Dependencias | Estado |
|---|---|---|
| S1: Nucleo matematico | Ninguna | OK — autocontenida |
| S2: I/O | Requiere S1 | OK — secuencia correcta |
| S3: Validacion | Requiere S2 | OK — necesita definir casos de referencia |
| S4: Batch/Sweep | Requiere S2 | OK — podria hacerse en paralelo con S3 |

---

## Conclusion

El plan es implementable tal como esta, con las siguientes acciones previas:
1. Resolver C1, C2, C3 (correccion directa en el plan).
2. Tomar decisiones sobre A1-A5 (pueden resolverse durante la Sesion 1).
3. Los items menores (O1-O4 y faltantes) pueden abordarse durante la implementacion.

**Recomendacion:** Proceder con la Sesion 1 tras resolver los criticos C1-C3.
