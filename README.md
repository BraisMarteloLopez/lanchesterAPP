# Modelo Lanchester-CIO

Herramienta de investigacion interna del CIO/ET. Calcula el resultado de enfrentamientos entre fuerzas terrestres usando ecuaciones de Lanchester: vencedor, bajas, duracion, municion consumida.

## Compilacion

```bash
make
```

Requisitos: compilador C++17 (g++ o clang++). Sin dependencias externas — `nlohmann/json` esta incluida localmente.

## Uso

### Escenario unico

```bash
./lanchester escenario.json
./lanchester escenario.json --output resultado.json
```

### Batch (directorio de escenarios)

```bash
./lanchester --batch escenarios/ --output resultados.csv
```

Procesa todos los `.json` del directorio. Salida CSV con separador `;`.

### Barrido parametrico (sweep)

```bash
# Barrer distancia de enfrentamiento
./lanchester --scenario base.json --sweep engagement_distance_m 500 5000 100

# Barrer numero de vehiculos
./lanchester --scenario base.json --sweep "combat_sequence[0].blue.composition[0].count" 1 20 1

# Barrer fraccion de empenamiento
./lanchester --scenario base.json --sweep "combat_sequence[0].blue.engagement_fraction" 0.1 1.0 0.1
```

Salida CSV. Soporta paths con notacion punto y corchetes.

### Monte Carlo (simulacion estocastica)

```bash
# 1000 replicas con proceso de Poisson
./lanchester escenario.json --montecarlo 1000

# Con semilla para reproducibilidad
./lanchester escenario.json --montecarlo 1000 --seed 123 --output mc_resultado.json
```

Ejecuta N replicas estocasticas usando un proceso de Poisson para las bajas (enteros discretos). La salida incluye el resultado determinista de referencia, distribucion de outcomes (probabilidad de victoria de cada bando), y estadisticas de supervivientes y duracion (media, std, percentiles p05/p25/mediana/p75/p95). Soporta cadenas de combates completas.

### Analisis de sensibilidad

```bash
./lanchester escenario.json --sensitivity
./lanchester escenario.json --sensitivity --output sensibilidad.csv
```

Perturba cada parametro del modelo (pendiente sigmoide, coeficientes de degradacion por distancia, multiplicadores de terreno) con factores 0.8, 0.9, 1.1, 1.2 y mide el impacto en supervivientes. Salida CSV con elasticidades por parametro.

### Modo de agregacion

```bash
./lanchester escenario.json --aggregation post
```

- `pre` (por defecto): media ponderada de parametros, una tasa unica.
- `post`: tasa individual por tipo de vehiculo, luego media ponderada de tasas. Mas realista para fuerzas heterogeneas.

## Formato de entrada

```json
{
  "scenario_id": "ALFA-001",
  "terrain": "MEDIO",
  "engagement_distance_m": 3000,
  "solver": { "h": 0.001666, "t_max_minutes": 30.0 },
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
          { "vehicle": "TOA_SPIKE_I", "count": 6 }
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
          { "vehicle": "T-80U", "count": 10 }
        ]
      },
      "reinforcements_blue": [],
      "reinforcements_red": [],
      "displacement_distance_m": 0
    }
  ]
}
```

| Campo | Valores |
|---|---|
| `terrain` | `FACIL`, `MEDIO`, `DIFICIL` |
| `tactical_state` | `Ataque a posicion defensiva`, `Busqueda del contacto`, `En posicion de tiro`, `Defensiva condiciones minimas`, `Defensiva organizacion ligera`, `Defensiva organizacion media`, `Retardo`, `Retrocede` |
| `mobility` | `MUY_ALTA`, `ALTA`, `MEDIA`, `BAJA` |
| `rate_factor` | Multiplicador de tasa de destruccion [0.0-2.0], defecto 1.0 |
| `count_factor` | Multiplicador de vehiculos efectivos [0.0-2.0], defecto 1.0 |
| `aft_casualties_pct` | Porcentaje de bajas por AFT pre-contacto [0.0-1.0] |
| `aft_received` | Numero de AFTs recibidas (campo informativo, no se usa en el calculo) |
| `engagement_fraction` | Fraccion de la fuerza que entra en combate [0.0-1.0] |
| `reinforcements_*` | Misma estructura que `composition`. Solo aplica a partir del combate 2 |

## Formato de salida

```json
{
  "scenario_id": "ALFA-001",
  "combats": [
    {
      "combat_id": 1,
      "outcome": "RED_WINS",
      "duration_contact_minutes": 1.12,
      "duration_total_minutes": 1.12,
      "blue_initial": 3.9,
      "red_initial": 6.66,
      "blue_survivors": 0.0,
      "red_survivors": 6.38,
      "blue_casualties": 3.9,
      "red_casualties": 0.28,
      "blue_ammo_consumed": 4.33,
      "red_ammo_consumed": 21.62,
      "blue_cc_ammo_consumed": 1.08,
      "red_cc_ammo_consumed": 0.0,
      "static_advantage": 0.1278
    }
  ]
}
```

| Outcome | Significado |
|---|---|
| `BLUE_WINS` | Fuerza roja eliminada, azul tiene supervivientes |
| `RED_WINS` | Fuerza azul eliminada, rojo tiene supervivientes |
| `DRAW` | Ambas fuerzas eliminadas simultaneamente |
| `INDETERMINATE` | Tiempo maximo alcanzado sin vencedor |

`static_advantage`: ratio de Lanchester en t=0 usando tasa total (convencional + C/C). Valores > 1 favorecen a azul.

### Salida Monte Carlo

```json
{
  "scenario_id": "ALFA-001",
  "mode": "montecarlo",
  "n_replicas": 1000,
  "seed": 42,
  "combats": [
    {
      "combat_id": 1,
      "deterministic": { "outcome": "RED_WINS", "blue_survivors": 0.0, "..." : "..." },
      "montecarlo": {
        "outcome_distribution": {
          "BLUE_WINS": 0.02,
          "RED_WINS": 0.95,
          "DRAW": 0.03,
          "INDETERMINATE": 0.0
        },
        "blue_survivors": { "mean": 0.1, "std": 0.5, "p05": 0.0, "p25": 0.0, "median": 0.0, "p75": 0.0, "p95": 1.0 },
        "red_survivors":  { "mean": 6.2, "std": 1.1, "p05": 4.0, "p25": 5.0, "median": 6.0, "p75": 7.0, "p95": 8.0 },
        "duration_minutes": { "mean": 1.5, "std": 0.3, "..." : "..." }
      }
    }
  ]
}
```

## Parametros del modelo

Los parametros del modelo se cargan desde `model_params.json`:

- **`kill_probability_slope`**: pendiente de la sigmoide de probabilidad de destruccion (175.0 por defecto)
- **`distance_degradation_coefficients`**: polinomio de degradacion por distancia (6 coeficientes)
- **`tactical_multipliers`**: multiplicadores por estado tactico (self y opponent)
- **`terrain_fire_effectiveness`**: efectividad del fuego por tipo de terreno

Cada parametro incluye metadatos de trazabilidad: origen, estado de calibracion, rango valido y sensibilidad.

## Calibracion

```bash
cd calibration
python3 calibrate.py
```

Framework de calibracion basado en optimizacion (scipy). Compara resultados del modelo contra datos de referencia (`reference_data.json`) y ajusta parametros automaticamente. Los escenarios de calibracion se almacenan en `calibration/scenarios/`.

## Catalogos de vehiculos

- `vehicle_db.json` — vehiculos propios (bando azul): TOA_SPIKE_I, VEC_25, LEOPARDO_2E, PIZARRO
- `vehicle_db_en.json` — vehiculos enemigos (bando rojo): T-80U, BMP-3, T-72B3, BTR-82A

Busqueda cruzada: si un vehiculo no se encuentra en el catalogo de su bando, se busca en el otro.

## Encadenamiento de combates

Se soportan cadenas de hasta 3 combates sucesivos. Los supervivientes del combate N pasan al N+1, sumando refuerzos declarados. La municion C/C de los refuerzos es completa. El tiempo total incluye desplazamiento entre posiciones segun la tabla de velocidades tacticas (km/h):

| Movilidad \ Terreno | FACIL | MEDIO | DIFICIL |
|---|---|---|---|
| MUY_ALTA | 40 | 25 | 12 |
| ALTA | 30 | 20 | 10 |
| MEDIA | 20 | 12 | 6 |
| BAJA | 10 | 6 | 3 |

Se usa la velocidad de la fuerza mas lenta.

## Validacion

```bash
bash tests/run_validation.sh
```

46 tests cubriendo: simetria, fuerza abrumadora, sin C/C, fuera de alcance, bajas AFT, multiplicadores defensivos, fuerzas mixtas, factores de ajuste, verificacion analitica. Incluye comparativa pre-tasa vs post-tasa.

Validacion adicional:

```bash
# Verificacion analitica contra solucion cerrada de Lanchester
python3 tests/test_analytical_verify.py

# Validacion estadistica de Monte Carlo
python3 tests/test_montecarlo_validation.py
```

## Decisiones de diseno

- **Punteria (U) no aplica a C/C.** Los sistemas contracarro modelados son misiles guiados; la probabilidad de impacto esta absorbida en la sigmoide T_cc. Solo el armamento convencional usa el parametro U.
- **Agregacion pre-tasa por defecto.** La media ponderada de parametros antes de la sigmoide sobreestima la efectividad de fuerzas mixtas heterogeneas (desigualdad de Jensen). Usar `--aggregation post` para mayor realismo con fuerzas muy dispares.
- **Consumo de municion C/C simplificado.** La formula asume que todos los vehiculos iniciales disparan continuamente. El factor `(A/A0)` atenua parcialmente el efecto de las bajas. Corregir esto requeriria tracking individual por vehiculo, incompatible con el modelo agregado.
- **Distribucion de bajas por vulnerabilidad en encadenamiento.** Las bajas se reparten proporcionalmente a la vulnerabilidad de cada tipo (count / (D_cc + 1)), priorizando vehiculos mas vulnerables. Vehiculos mas blindados sobreviven proporcionalmente mas.
- **`aft_received` es informativo.** Solo `aft_casualties_pct` se usa en el calculo. El campo existe para trazabilidad del escenario.
- **Integrador RK4.** Se usa Runge-Kutta de orden 4 para la integracion temporal, con mayor precision que Euler para el mismo paso temporal.
- **Parametros externalizados.** Todos los parametros del modelo se cargan desde `model_params.json`, con metadatos de trazabilidad y rangos validos para facilitar la calibracion.
- **Monte Carlo con Poisson.** Las bajas se discretizan con distribuciones de Poisson. Subdivision automatica de pasos si lambda > 2 para evitar distorsion.

## Estructura

```
├── main.cpp                  # CLI (~190 lineas)
├── lanchester_types.h        # Tipos de datos y estructuras
├── lanchester_model.h        # Funciones matematicas, agregacion, simulacion, Monte Carlo
├── lanchester_io.h           # I/O JSON/CSV, escenarios, batch, sweep, sensibilidad
├── model_params.json         # Parametros del modelo (externalizados, calibrables)
├── vehicle_db.json           # Catalogo vehiculos azul
├── vehicle_db_en.json        # Catalogo vehiculos rojo
├── include/nlohmann/json.hpp # Dependencia JSON header-only
├── Makefile
├── calibration/
│   ├── calibrate.py          # Framework de calibracion (scipy)
│   ├── reference_data.json   # Datos de referencia para calibracion
│   └── scenarios/            # Escenarios de calibracion
├── ejemplos/
│   ├── toa_vs_t80u.json      # Escenario simple
│   ├── compania_mixta.json   # Cadena de 2 combates
│   ├── sweep_distancia.sh    # Sensibilidad: distancia
│   ├── sweep_count.sh        # Sensibilidad: numero vehiculos
│   └── sweep_engagement.sh   # Sensibilidad: fraccion empenamiento
└── tests/
    ├── run_validation.sh           # Script de validacion
    ├── test_*.json                 # 9 escenarios de prueba
    ├── test_analytical_verify.py   # Verificacion analitica
    └── test_montecarlo_validation.py # Validacion Monte Carlo
```

---

*CIO / ET — Herramienta de investigacion interna*
