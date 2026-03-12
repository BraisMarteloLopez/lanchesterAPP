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
├── vehicle_db.json       # Catalogo vehiculos propios
├── vehicle_db_en.json    # Catalogo vehiculos enemigos
├── Makefile
├── README.md
└── ejemplos/
    ├── toa_vs_t80u.json
    └── compania_mixta.json
```

**Estandar:** C++17  
**Dependencia externa:** `nlohmann/json` (header-only). Cero otras dependencias.  
**El nucleo matematico usa solo la libreria estandar.**

---

## Modelo matematico

### Parametros de vehiculo

Cada vehiculo en el catalogo tiene los siguientes parametros:

| Parametro | Simbolo | Descripcion |
|---|---|---|
| Dureza | D | Resistencia al impacto [1-1000] |
| Potencia | P | Potencia del armamento principal [1-1000] |
| Punteria | U | Precision [0.2-1.0] |
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

### Funciones del modelo

**Probabilidad de destruccion al impactar:**
```
T = 1 / (1 + exp((D_objetivo - P_atacante) / 175))
```

**Degradacion del disparo por distancia:**
```
g = -0.188*(d/1000) - 0.865*f + 0.018*(d/1000)^2 - 0.162*(d/1000)*f + 0.755*f^2 + 1.295
```
Donde `d` es la distancia de enfrentamiento en metros y `f` es el factor de distancia del vehiculo.  
Si `d > A_max`, entonces `g = 0`.

**Degradacion acotada:**
```
G = clamp(g, 0.0, 1.0)
```

**Tasa de destruccion estatica convencional:**
```
S = T * G * U * c
```

**Tasa de destruccion C/C estatica:**
```
S_cc_static = c_cc * T_cc * G_cc
```
Donde `T_cc` y `G_cc` se calculan con los parametros C/C del vehiculo y la dureza C/C del objetivo.

**Tasa de destruccion C/C dinamica** (decrece con el tiempo y con las bajas propias):
```
S_cc(t, A) = (A / A0) * S_cc_static * max(0, M - c_cc * t) / M    si M > 0
S_cc(t, A) = 0                                                       si M = 0
```
Donde `A` es el numero de unidades propias activas, `A0` el inicial, y `t` el tiempo transcurrido en minutos.

### Agregacion de fuerzas mixtas

Cuando una fuerza tiene composicion mixta (varios tipos de vehiculo), los parametros se agregan mediante media ponderada por numero de unidades antes de calcular la tasa:

```
P_param = SUM(n_i * param_i) / SUM(n_i)
```

Para los parametros C/C, la ponderacion se realiza sobre el numero de unidades con capacidad C/C:

```
P_param_cc = SUM(n_i * param_cc_i) / SUM(n_i * CC_i)
```

### Multiplicadores tacticos

La tasa de destruccion efectiva incorpora multiplicadores de situacion tactica:

```
S_efectiva = S * mult_propio * mult_oponente * mult_estado
```

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

### Bucle de integracion (Euler explicito)

```
A(i+1) = max(0, A(i) - (S_cc_rojo(i) + S_rojo) * R(i) * h)
R(i+1) = max(0, R(i) - (S_cc_azul(i) + S_azul) * A(i) * h)
h = paso temporal en minutos (por defecto 1/600)
```

Condicion de parada: `A(i) = 0` OR `R(i) = 0` OR `t >= t_max`

Las fuerzas iniciales del combate incorporan la fraccion de empeñamiento y el efecto de las AFT recibidas antes del contacto:

```
A0 = (n_total - n_total * pct_bajas_AFT) * fraccion_empeñamiento * factor_vehiculos
```

### Encadenamiento de combates

En una cadena de combates, las fuerzas iniciales del combate N+1 son los supervivientes del combate N mas los refuerzos declarados. La municion disponible se reduce en la cantidad consumida en combates anteriores. Entre combates se calcula un tiempo de desplazamiento:

```
t_desplazamiento = (distancia_m / 1000) * 60 / velocidad_tactica_kmh
```

La velocidad tactica depende del terreno y la movilidad de la fuerza mas rapida del enfrentamiento.

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
      "reinforcements_red": []
    }
  ]
}
```

**Valores de `terrain`:** `FACIL`, `MEDIO`, `DIFICIL`  
**Valores de `tactical_state`:** los de la tabla de multiplicadores tacticos  
**Valores de `mobility`:** `MUY_ALTA`, `ALTA`, `MEDIA`, `BAJA`  
**`rate_factor` y `count_factor`:** factores arbitrarios [0.0-2.0], valor por defecto 1.0

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
      "blue_ammo_consumed": 0.0,
      "red_ammo_consumed": 8.40,
      "blue_cc_ammo_consumed": 0.0,
      "red_cc_ammo_consumed": 0.0,
      "static_advantage": 0.87
    }
  ]
}
```

**Valores de `outcome`:** `BLUE_WINS`, `RED_WINS`, `DRAW`, `INDETERMINATE`  
**`static_advantage`:** razon `(S_azul * A0^2) / (S_rojo * R0^2)`. Valores > 1 favorecen a Azul.

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
```

El modo `--sweep` genera una tabla CSV con una fila por valor del parametro barrido. Las columnas son el valor del parametro y los campos de salida del combate final de la secuencia.

---

## Sesiones de desarrollo

### Sesion 1 — Nucleo matematico
**Objetivo:** Codigo compilable con las funciones del modelo y el bucle Euler.

Tareas:
1. Implementar las funciones matematicas T, g, G, S, S_cc
2. Implementar agregacion ponderada de fuerzas mixtas
3. Implementar el bucle Euler con condicion de parada
4. Compilar limpio

Entregable: `main.cpp` que compila. Sin I/O todavia.

---

### Sesion 2 — I/O y escenario completo
**Objetivo:** Ejecutable que lee JSON de entrada y escribe JSON de salida.

Tareas:
1. Integrar `nlohmann/json`
2. Implementar lectura del formato de entrada
3. Implementar escritura del formato de salida
4. Implementar encadenamiento de combates
5. Implementar calculo de tiempo de desplazamiento entre combates
6. Crear los ficheros de ejemplo

Entregable: Ejecutable funcional end-to-end con los dos ejemplos.

---

### Sesion 3 — Validacion
**Objetivo:** Confirmar que el ejecutable reproduce resultados de referencia dentro de tolerancia.

Tareas:
1. Definir casos de prueba con resultados esperados
2. Ejecutar los casos contra el binario
3. Verificar tolerancia `< 1e-6` en unidades supervivientes
4. Documentar diferencias entre configuraciones de agregacion en los mismos escenarios

Entregable: Tabla de validacion. Tabla de impacto de las distintas opciones de agregacion.

---

### Sesion 4 — Batch y analisis de sensibilidad
**Objetivo:** Capacidad de ejecutar multiples escenarios y barrer parametros.

Tareas:
1. Implementar modo `--batch`
2. Implementar modo `--sweep`
3. Implementar salida CSV para ambos modos
4. Crear ejemplos de analisis de sensibilidad

Entregable: Ejecutable completo con capacidad batch y sweep.

---

## Criterio de exito

El ejecutable es util cuando puede responder esta pregunta con un fichero JSON en menos de 1 segundo:

> Dados estos vehiculos, este terreno y esta situacion tactica, cuantos vehiculos quedan operativos tras el combate, quien gana, en cuanto tiempo, y como cambia ese resultado al variar la distancia de enfrentamiento entre 500 y 5000 metros.

---

*CIO / ET — Herramienta de investigacion interna*
