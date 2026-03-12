# Plan de Trabajo: Modelo Lanchester-CIO
## Port a C++ | Herramienta de Investigacion Interna

---

## Descripcion del proyecto

Implementacion en C++ de un modelo de combate basado en las ecuaciones de Lanchester para uso como herramienta de investigacion interna del CIO/ET. El modelo calcula el resultado de un enfrentamiento entre dos fuerzas terrestres (vencedor, bajas, duracion, municion consumida) a partir de la composicion de unidades, el terreno y la situacion tactica.

El ejecutable soporta escenarios unicos, cadenas de hasta tres combates sucesivos, ejecucion en lote y barrido parametrico para analisis de sensibilidad.

---

## Estructura del proyecto

```
lanchester-cio/
├── main.cpp              # Todo el ejecutable (~800 lineas)
├── vehicle_db.json       # Catalogo vehiculos propios (bando azul)
├── vehicle_db_en.json    # Catalogo vehiculos enemigos (bando rojo)
├── include/
│   └── nlohmann/
│       └── json.hpp      # Header-only, descargado del release oficial
├── Makefile
├── README.md
└── ejemplos/
    ├── toa_vs_t80u.json
    └── compania_mixta.json
```

**Estandar:** C++17
**Dependencia externa:** `nlohmann/json` (header-only, incluida localmente en `include/`). Cero otras dependencias.
**El nucleo matematico usa solo la libreria estandar.**

**Seleccion de catalogo:** Por convencion, `vehicle_db.json` contiene vehiculos del bando azul (propios) y `vehicle_db_en.json` del bando rojo (enemigo). El programa busca cada nombre de vehiculo primero en el catalogo de su bando; si no lo encuentra, busca en el otro catalogo. Esto permite escenarios blue-vs-blue o vehiculos capturados.

---

## Modelo matematico

### Parametros de vehiculo

Cada vehiculo en el catalogo tiene los siguientes parametros:

| Parametro | Simbolo | Descripcion |
|---|---|---|
| Dureza | D | Resistencia al impacto [1-1000] |
| Potencia | P | Potencia del armamento principal [1-1000] |
| Punteria | U | Precision del armamento convencional [0.2-1.0] |
| Cadencia | c | Disparos por minuto [0.1-5] |
| Alcance maximo | A_max | Metros [200-5000] |
| Factor distancia | f | Coeficiente de degradacion por distancia [0-2] |
| Capacidad C/C | CC | Indicador binario {0,1} |
| Potencia C/C | P_cc | Potencia del armamento contracarro [1-1200] |
| Dureza C/C | D_cc | Dureza frente a impacto contracarro [1-1200] |
| Cadencia C/C | c_cc | Disparos C/C por minuto [0.1-1] |
| Alcance C/C | A_cc | Metros [200-5500] |
| Municion C/C | M | Misiles por vehiculo [1-10] |
| Factor distancia C/C | f_cc | Coeficiente degradacion C/C [0-2] |

> **Nota sobre punteria C/C:** El parametro `U` (punteria) solo aplica al armamento convencional. Los sistemas contracarro modelados son misiles guiados cuya probabilidad de impacto esta absorbida en la sigmoide `T_cc`. Si en el futuro se modelan sistemas C/C no guiados, se anadira un parametro `U_cc`.

### Funciones del modelo

**Probabilidad de destruccion al impactar:**
```
T = 1 / (1 + exp((D_objetivo - P_atacante) / 175))
```

**Probabilidad de destruccion C/C al impactar:**
```
T_cc = 1 / (1 + exp((D_cc_objetivo - P_cc_atacante) / 175))
```
Misma funcion sigmoide, usando los parametros de dureza y potencia contracarro.

**Degradacion del disparo por distancia (canal convencional):**
```
g = -0.188*(d/1000) - 0.865*f + 0.018*(d/1000)^2 - 0.162*(d/1000)*f + 0.755*f^2 + 1.295
```
Donde `d` es la distancia de enfrentamiento en metros y `f` es el factor de distancia del vehiculo.
Si `d > A_max`, entonces `g = 0`.

**Degradacion del disparo por distancia (canal C/C):**
```
g_cc = -0.188*(d/1000) - 0.865*f_cc + 0.018*(d/1000)^2 - 0.162*(d/1000)*f_cc + 0.755*f_cc^2 + 1.295
```
Misma funcion polinomial, usando `f_cc` como factor de distancia.
Si `d > A_cc`, entonces `g_cc = 0`.

**Degradacion acotada (ambos canales):**
```
G     = clamp(g,    0.0, 1.0)
G_cc  = clamp(g_cc, 0.0, 1.0)
```

**Tasa de destruccion estatica convencional:**
```
S = T * G * U * c
```

**Tasa de destruccion C/C estatica:**
```
S_cc_static = c_cc * T_cc * G_cc
```
No incluye punteria `U` (misiles guiados, ver nota anterior).

**Tasa de destruccion C/C dinamica** (decrece con el tiempo y con las bajas propias):
```
S_cc(t, A) = (A / A0) * S_cc_static * max(0, M - c_cc * t) / M    si M > 0
S_cc(t, A) = 0                                                       si M = 0
```
Donde `A` es el numero de unidades propias activas, `A0` el inicial, y `t` el tiempo transcurrido en minutos.

> **Limitacion conocida:** El termino `c_cc * t` asume que todos los vehiculos iniciales disparan continuamente. En realidad, los vehiculos destruidos dejan de consumir municion. El factor `(A/A0)` atenua parcialmente este efecto. Corregir completamente requeriria tracking individual de municion por vehiculo, lo que rompe el modelo agregado. Se acepta como simplificacion conservadora.

### Agregacion de fuerzas mixtas

Cuando una fuerza tiene composicion mixta (varios tipos de vehiculo), los parametros se agregan mediante media ponderada por numero de unidades **antes** de calcular la tasa (agregacion pre-tasa):

```
P_param = SUM(n_i * param_i) / SUM(n_i)
```

Para los parametros C/C, la ponderacion se realiza sobre el numero de unidades con capacidad C/C:

```
P_param_cc = SUM(n_i * param_cc_i) / SUM(n_i * CC_i)     si SUM(n_i * CC_i) > 0
```

**Caso sin capacidad C/C:** Si `SUM(n_i * CC_i) = 0` (ninguna unidad del bando tiene capacidad contracarro), toda la rama C/C del bando se desactiva: `S_cc_static = 0`, `n_cc = 0`. No se calculan parametros agregados C/C.

> **Sobre la no-linealidad de la agregacion:** La media ponderada de parametros antes de aplicar la sigmoide `T` produce resultados diferentes a calcular `T` por tipo de vehiculo y luego ponderar. Esto es una simplificacion inherente al modelo agregado. En la Sesion 3 se implementara opcionalmente la agregacion post-tasa (`--aggregation post`) para documentar las diferencias. El modo por defecto es pre-tasa.

### Multiplicadores tacticos

La tasa de destruccion efectiva incorpora multiplicadores de situacion tactica y factores de ajuste del escenario:

```
S_efectiva = S * mult_propio * mult_oponente * mult_estado * rate_factor
```

Donde `rate_factor` es el factor de ajuste de tasa definido en el escenario JSON (rango [0.0-2.0], valor por defecto 1.0). Permite al analista ajustar la efectividad global de un bando.

Los multiplicadores disponibles son:

| Situacion | Mult. fuerzas en esa situacion | Mult. fuerzas opuestas |
|---|---|---|
| Ataque a posicion defensiva | 1.0 | 1.0 |
| Busqueda del contacto | 0.9 | 1.0 |
| En posicion de tiro | 1.0 | 0.9 |
| Defensiva condiciones minimas | 1.0 | 1/2.25^2 |
| Defensiva organizacion ligera | 1.0 | 1/2.75^2 |
| Defensiva organizacion media | 1.0 | 1/4.25^2 |
| Retardo | 1.0 | 1/6^2 |
| Retrocede | 0.9 | 1.0 |

### Tasa total efectiva

La tasa total que aplica un bando combina las componentes convencional y C/C, ambas afectadas por sus respectivos multiplicadores:

```
S_total(t) = S_efectiva + S_cc(t, A) * rate_factor
```

Donde `S_efectiva` ya incluye `rate_factor` (ver formula anterior) y `S_cc` tambien se multiplica por `rate_factor`.

En forma expandida:
```
S_total(t) = (S_conv * mult_propio * mult_oponente * mult_estado + S_cc(t, A)) * rate_factor
```

### Bucle de integracion (Euler explicito)

```
A(i+1) = max(0, A(i) - S_total_rojo(i) * R(i) * h)
R(i+1) = max(0, R(i) - S_total_azul(i) * A(i) * h)
h = paso temporal en minutos (por defecto 1/600)
```

Condicion de parada: `A(i) = 0` OR `R(i) = 0` OR `t >= t_max`

**Determinacion del resultado:**
- `BLUE_WINS`: `R(t) = 0` y `A(t) > 0`
- `RED_WINS`: `A(t) = 0` y `R(t) > 0`
- `DRAW`: ambas fuerzas llegan a 0 en el mismo paso temporal (o ambas < 0.5 simultaneamente)
- `INDETERMINATE`: se alcanza `t_max` sin que ninguna fuerza llegue a 0

Las fuerzas iniciales del combate incorporan la fraccion de empenamiento y el efecto de las AFT recibidas antes del contacto:

```
A0 = (n_total - n_total * pct_bajas_AFT) * fraccion_empenamiento * count_factor
```

Donde `count_factor` es un factor arbitrario de ajuste del numero de vehiculos (rango [0.0-2.0], valor por defecto 1.0).

### Calculo de municion consumida

La municion consumida se acumula durante el bucle Euler:

**Municion convencional:**
```
ammo_conv += c_agregada * N_vivos(t) * h       (cada paso)
```

**Municion C/C:**
```
ammo_cc += c_cc_agregada * N_cc_vivos(t) * h   (cada paso, con tope en M * N_cc_inicial)
```

Donde `N_cc_vivos(t) = N_vivos(t) * (n_cc_inicial / n_total_inicial)` (proporcion constante de vehiculos con C/C).

### Indicador de ventaja estatica

```
static_advantage = (S_total_azul_t0 * A0^2) / (S_total_rojo_t0 * R0^2)
```

Donde `S_total_t0` es la tasa total en `t=0` con todas las unidades vivas: `S_total_t0 = S_conv_efectiva + S_cc_static * rate_factor`. Valores > 1 favorecen a Azul.

### Encadenamiento de combates

En una cadena de combates, las fuerzas iniciales del combate N+1 son los supervivientes del combate N mas los refuerzos declarados. La municion C/C disponible se reduce en la cantidad consumida en combates anteriores. La municion C/C de los refuerzos es completa (M misiles por vehiculo). Entre combates se calcula un tiempo de desplazamiento:

```
t_desplazamiento = (distancia_m / 1000) * 60 / velocidad_tactica_kmh
```

La velocidad tactica se determina por la fuerza **mas lenta** del enfrentamiento, segun la siguiente tabla (km/h):

| Movilidad \ Terreno | FACIL | MEDIO | DIFICIL |
|---|---|---|---|
| MUY_ALTA | 40 | 25 | 12 |
| ALTA | 30 | 20 | 10 |
| MEDIA | 20 | 12 | 6 |
| BAJA | 10 | 6 | 3 |

---

## Formato de entrada

```json
{
  "scenario_id": "ALFA-001",
  "terrain": "MEDIO",
  "engagement_distance_m": 3000,
  "solver": {
    "h": 0.001666,
    "t_max_minutes": 30.0
  },
  "combat_sequence": [
    {
      "combat_id": 1,
      "blue": {
        "tactical_state": "En posicion de tiro",
        "mobility": "ALTA",
        "aft_received": 5,
        "aft_casualties_pct": 0.025,
        "engagement_fraction": 0.666,
        "rate_factor": 1.0,
        "count_factor": 1.0,
        "composition": [
          {"vehicle": "TOA_SPIKE_I", "count": 6}
        ]
      },
      "red": {
        "tactical_state": "Ataque a posicion defensiva",
        "mobility": "ALTA",
        "aft_received": 0,
        "aft_casualties_pct": 0.0,
        "engagement_fraction": 0.666,
        "rate_factor": 1.0,
        "count_factor": 1.0,
        "composition": [
          {"vehicle": "T-80U", "count": 10}
        ]
      },
      "reinforcements_blue": [],
      "reinforcements_red": [],
      "displacement_distance_m": 0
    }
  ]
}
```

**Valores de `terrain`:** `FACIL`, `MEDIO`, `DIFICIL`
**Valores de `tactical_state`:** los de la tabla de multiplicadores tacticos
**Valores de `mobility`:** `MUY_ALTA`, `ALTA`, `MEDIA`, `BAJA`
**`rate_factor`:** multiplicador de la tasa de destruccion del bando [0.0-2.0], por defecto 1.0
**`count_factor`:** multiplicador del numero efectivo de vehiculos del bando [0.0-2.0], por defecto 1.0
**`aft_received`:** numero de AFTs recibidas antes del contacto (campo informativo, no se usa en el calculo)
**`aft_casualties_pct`:** porcentaje de bajas por AFT antes del contacto [0.0-1.0]
**`reinforcements_blue` / `reinforcements_red`:** array de refuerzos con misma estructura que `composition`: `[{"vehicle": "NOMBRE", "count": N}]`. Solo aplica a partir del combate 2 de una cadena.
**`displacement_distance_m`:** distancia en metros desde la posicion del combate anterior (solo combates 2+, por defecto 0)

---

## Formato de salida

```json
{
  "scenario_id": "ALFA-001",
  "combats": [
    {
      "combat_id": 1,
      "outcome": "RED_WINS",
      "duration_contact_minutes": 4.23,
      "duration_total_minutes": 4.23,
      "blue_initial": 4.0,
      "red_initial": 6.67,
      "blue_survivors": 0.0,
      "red_survivors": 3.21,
      "blue_casualties": 4.0,
      "red_casualties": 3.46,
      "blue_ammo_consumed": 12.53,
      "red_ammo_consumed": 8.40,
      "blue_cc_ammo_consumed": 3.2,
      "red_cc_ammo_consumed": 0.0,
      "static_advantage": 0.87
    }
  ]
}
```

**Valores de `outcome`:**
- `BLUE_WINS`: fuerza roja eliminada, azul tiene supervivientes
- `RED_WINS`: fuerza azul eliminada, rojo tiene supervivientes
- `DRAW`: ambas fuerzas eliminadas en el mismo paso temporal
- `INDETERMINATE`: se alcanzo `t_max` sin que ninguna fuerza llegue a 0

**`static_advantage`:** razon `(S_total_azul_t0 * A0^2) / (S_total_rojo_t0 * R0^2)` calculada con la tasa total en t=0. Valores > 1 favorecen a Azul.

---

## Interfaz de linea de comandos

```bash
# Escenario unico, salida por stdout
./lanchester escenario.json

# Escenario unico, salida a fichero
./lanchester escenario.json --output resultado.json

# Batch: directorio de JSONs, salida CSV
./lanchester --batch escenarios/ --output resultados.csv

# Barrido de un parametro escalar
./lanchester --scenario base.json --sweep engagement_distance_m 500 5000 100

# Barrido de conteo de vehiculos de un bando
./lanchester --scenario base.json --sweep blue.composition[0].count 1 20 1

# Seleccion de modo de agregacion (por defecto: pre)
./lanchester escenario.json --aggregation post
```

El modo `--sweep` genera una tabla CSV con una fila por valor del parametro barrido. Las columnas son el valor del parametro y los campos de salida del combate final de la secuencia.

**Formato CSV:**
- Separador: `;` (punto y coma, compatibilidad con Excel en locales europeos)
- Encoding: UTF-8
- Primera fila: cabeceras con nombres de campos
- `--batch`: una fila por escenario procesado
- `--sweep`: una fila por valor del parametro, con columna adicional para el valor barrido

**Parser de paths para `--sweep`:** Soporta acceso por clave (`field.subfield`), acceso por indice (`field[N]`), y combinaciones (`field.array[N].subfield`). No soporta wildcards ni expresiones complejas.

---

## Sesiones de desarrollo

### Sesion 1 — Nucleo matematico
**Objetivo:** Codigo compilable con las funciones del modelo y el bucle Euler.

Tareas:
1. Implementar las funciones matematicas T, T_cc, g, g_cc, G, G_cc, S, S_cc
2. Implementar agregacion ponderada de fuerzas mixtas (modo pre-tasa), con proteccion contra division por cero en rama C/C
3. Implementar multiplicadores tacticos y aplicacion de `rate_factor` y `count_factor`
4. Implementar el bucle Euler con condicion de parada y determinacion de outcome (BLUE_WINS, RED_WINS, DRAW, INDETERMINATE)
5. Implementar acumulacion de municion consumida (convencional y C/C) durante el bucle
6. Implementar calculo de `static_advantage` con tasa total en t=0
7. Compilar limpio

Entregable: `main.cpp` que compila. Sin I/O todavia.

---

### Sesion 2 — I/O y escenario completo
**Objetivo:** Ejecutable que lee JSON de entrada y escribe JSON de salida.

Tareas:
1. Integrar `nlohmann/json` (header local en `include/`)
2. Implementar carga de catalogos de vehiculos (`vehicle_db.json`, `vehicle_db_en.json`) con busqueda cruzada por bando
3. Implementar lectura del formato de entrada, incluyendo refuerzos (`reinforcements_blue/red` con formato `[{"vehicle", "count"}]`)
4. Implementar escritura del formato de salida
5. Implementar encadenamiento de combates con refuerzos y municion residual
6. Implementar calculo de tiempo de desplazamiento entre combates usando la tabla de velocidades tacticas
7. Crear los ficheros de ejemplo

Entregable: Ejecutable funcional end-to-end con los dos ejemplos.

---

### Sesion 3 — Validacion
**Objetivo:** Confirmar que el ejecutable reproduce resultados de referencia dentro de tolerancia.

Tareas:
1. Definir casos de prueba con resultados esperados
2. Ejecutar los casos contra el binario
3. Verificar tolerancia `< 1e-6` en unidades supervivientes
4. Implementar modo de agregacion post-tasa (`--aggregation post`) como alternativa
5. Ejecutar los mismos casos con ambos modos de agregacion
6. Documentar diferencias cuantitativas entre pre-tasa y post-tasa

Entregable: Tabla de validacion. Tabla comparativa de impacto de las opciones de agregacion.

---

### Sesion 4 — Batch y analisis de sensibilidad
**Objetivo:** Capacidad de ejecutar multiples escenarios y barrer parametros.

Tareas:
1. Implementar modo `--batch`
2. Implementar modo `--sweep` con parser de paths JSON (notacion punto y corchetes)
3. Implementar salida CSV para ambos modos (separador `;`, UTF-8, cabeceras)
4. Crear ejemplos de analisis de sensibilidad

Entregable: Ejecutable completo con capacidad batch y sweep.

---

## Criterio de exito

El ejecutable es util cuando puede responder esta pregunta con un fichero JSON en menos de 1 segundo:

> Dados estos vehiculos, este terreno y esta situacion tactica, cuantos vehiculos quedan operativos tras el combate, quien gana, en cuanto tiempo, y como cambia ese resultado al variar la distancia de enfrentamiento entre 500 y 5000 metros.

---

## Referencias

- `DEUDA_TECNICA.md` — Registro completo de decisiones de diseno, simplificaciones aceptadas y resoluciones adoptadas durante la validacion del plan.
- `VALIDACION_PLAN.md` — Informe de validacion original con hallazgos identificados.

---

*CIO / ET — Herramienta de investigacion interna*
