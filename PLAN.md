# Plan de Diseno — DT-002 y DT-003

## Contexto

Quedan dos problemas fundamentales que invalidan la fiabilidad del modelo:

1. **DT-002**: Las constantes del modelo no estan calibradas. Los resultados son numeros sin significado verificable.
2. **DT-003**: El modelo es determinista aplicado a fuerzas de 6-20 vehiculos. No hay incertidumbre en los resultados.

Ambos estan relacionados: no tiene sentido calibrar un modelo determinista si la escala de uso requiere un modelo estocastico. El plan los aborda en orden.

---

## PARTE A — DT-002: Calibracion de constantes

### A.1 — Analisis de sensibilidad automatizado

**Objetivo**: Cuantificar cuanto cambia el resultado al variar cada parametro. Identificar cuales dominan y cuales son irrelevantes.

**Diseno**:

Nuevo modo CLI: `--sensitivity <escenario.json> [--output <file>]`

```
./lanchester --sensitivity test_01_symmetric.json
```

Funcionamiento:
1. Ejecutar el escenario base (resultado de referencia).
2. Para cada parametro en `model_params.json`, barrer ±20% en 5 pasos (0.8x, 0.9x, 1.0x, 1.1x, 1.2x).
3. Para cada vehiculo en los catalogos, barrer D, P, U, c individualmente ±20%.
4. Registrar: `delta_blue_survivors`, `delta_red_survivors`, `delta_outcome` respecto a la referencia.
5. Salida: tabla CSV con columnas `[parametro, valor_base, valor_test, blue_surv_ref, blue_surv_test, delta, pct_change]`.

**Archivos a modificar**:
- `lanchester_io.h`: nueva funcion `run_sensitivity()`.
- `main.cpp`: nuevo flag `--sensitivity` en el parser CLI.

**Metricas de sensibilidad a calcular**:
- **Elasticidad**: `(delta_output / output_ref) / (delta_param / param_ref)` — cuanto cambia el resultado (%) por 1% de cambio en el parametro.
- Ordenar parametros por elasticidad absoluta descendente.

**Resultado esperado**: Una tabla que diga, por ejemplo:
```
kill_probability_slope    elasticidad = 2.3   (muy sensible)
c_dk                      elasticidad = 0.8   (moderado)
terrain_MEDIO             elasticidad = 0.1   (poco sensible)
```

Esto indica donde concentrar el esfuerzo de calibracion.

---

### A.2 — Framework de calibracion contra datos de referencia

**Objetivo**: Poder comparar automaticamente los resultados del modelo contra datos de referencia (simulaciones validadas, ejercicios, wargames doctrinales).

**Diseno**:

Nuevo directorio `calibration/` con:

```
calibration/
  reference_data.json       # Datos de referencia (a rellenar por el usuario)
  calibrate.py              # Script de calibracion
  sensitivity_report.py     # Analisis de sensibilidad (post-proceso)
```

**`reference_data.json`** — formato:

```json
{
  "reference_cases": [
    {
      "id": "REF-001",
      "source": "Ejercicio TRIDENT JUNCTURE 2018 / simulacion JANUS",
      "scenario": "calibration/scenarios/ref_001.json",
      "expected": {
        "outcome": "BLUE_WINS",
        "blue_survivors": { "min": 8, "max": 12 },
        "red_survivors": { "min": 0, "max": 2 },
        "duration_minutes": { "min": 10, "max": 25 }
      },
      "confidence": "medium",
      "notes": "Datos de Cia MBT vs Cia MBT en terreno abierto, ratio 1.5:1"
    }
  ]
}
```

**`calibrate.py`** — flujo:

1. Cargar `reference_data.json`.
2. Para cada caso de referencia:
   a. Ejecutar `./lanchester <scenario>` con los parametros actuales.
   b. Comparar resultado contra `expected` (min/max).
   c. Calcular error: `sum of (result - midpoint)^2 / range^2` normalizado.
3. Calcular error total (suma ponderada por confianza).
4. Opcionalmente: optimizacion (scipy.optimize.minimize) sobre los parametros del modelo para minimizar el error total.

**Parametros optimizables** (los que se barren):
- `kill_probability_slope` — rango [50, 500]
- 6 coeficientes de degradacion por distancia — rango individual
- 3 multiplicadores de terreno — rango [0.3, 1.0]
- Bases de multiplicadores tacticos defensivos — rango [1.5, 10.0]

**Restricciones**:
- `terrain_FACIL >= terrain_MEDIO >= terrain_DIFICIL`
- Todos los multiplicadores >= 0
- `kill_probability_slope > 0`

**Salida**: `calibration_report.json` con parametros optimizados y error por caso.

---

### A.3 — Documentacion de origen de parametros

**Objetivo**: Cada parametro debe tener trazabilidad.

Extender `model_params.json` con metadatos:

```json
{
  "kill_probability_slope": {
    "value": 175.0,
    "origin": "expert_judgment",
    "author": "",
    "date": "",
    "reference": "",
    "calibration_status": "uncalibrated",
    "sensitivity": "high",
    "valid_range": [50, 500]
  }
}
```

**Nota**: Esto es un cambio de formato. El loader (`load_model_params`) debe aceptar tanto el formato plano actual como el formato extendido, para no romper retrocompatibilidad:

```cpp
// Si es objeto con "value", extraer .value; si es escalar, usar directamente
double read_param(const json& j, const std::string& key, double default_val) {
    if (!j.contains(key)) return default_val;
    if (j[key].is_object()) return j[key].value("value", default_val);
    return j[key].get<double>();
}
```

---

## PARTE B — DT-003: Modo estocastico (Monte Carlo)

### B.1 — Modelo estocastico de Lanchester

**Fundamento teorico**:

Las ecuaciones deterministas de Lanchester:
```
dA/dt = -S_red * R
dR/dt = -S_blue * A
```

Se reinterpretan como tasas medias de un proceso estocastico. En cada paso temporal `dt`, el numero de bajas sigue una distribucion de Poisson:

```
bajas_blue(t) ~ Poisson(S_red * R * dt)
bajas_red(t)  ~ Poisson(S_blue * A * dt)
```

Donde `S * N_oponente * dt` es la tasa esperada de bajas en el intervalo `dt`. El proceso de Poisson es la eleccion natural porque:
- Las bajas son eventos discretos (0, 1, 2... vehiculos destruidos por paso).
- A escalas grandes, Poisson → Normal → continuo (recupera el determinista).
- A escalas pequenas (6-20 vehiculos), la varianza es significativa.

**Alternativa considerada**: Binomial. Cada vehiculo oponente tiene probabilidad `p = S * dt` de destruir un vehiculo en `dt`. Con `n = R` intentos, bajas ~ Binomial(R, p). Para `p` pequeno y `R` moderado, Binomial ≈ Poisson. Usar Poisson es mas simple y equivalente en el regimen relevante.

---

### B.2 — Diseno de implementacion

**Nuevo modo CLI**:

```
./lanchester <escenario.json> --montecarlo <N> [--seed <S>] [--output <file>]
```

- `N`: numero de replicas (default sugerido: 1000).
- `S`: semilla para reproducibilidad.

**Nueva funcion en `lanchester_model.h`**:

```cpp
struct MonteCarloResult {
    int n_replicas;
    // Estadisticas de blue_survivors
    double blue_surv_mean;
    double blue_surv_std;
    double blue_surv_p05;    // percentil 5
    double blue_surv_p25;
    double blue_surv_median;
    double blue_surv_p75;
    double blue_surv_p95;
    // Idem para red
    double red_surv_mean, red_surv_std;
    double red_surv_p05, red_surv_p25, red_surv_median, red_surv_p75, red_surv_p95;
    // Distribucion de outcomes
    int count_blue_wins;
    int count_red_wins;
    int count_draw;
    int count_indeterminate;
    // Duracion
    double duration_mean, duration_std;
    // Resultado determinista para comparacion
    CombatResult deterministic;
};
```

**Funcion `simulate_combat_stochastic()`**:

```cpp
CombatResult simulate_combat_stochastic(const CombatInput& input, std::mt19937& rng) {
    // Misma estructura que simulate_combat() pero:
    // En cada paso RK4, las tasas se usan como lambda de Poisson
    // bajas = poisson_distribution(rate * N_oponente * h)
    // A y R son enteros (vehiculos discretos)

    int A = static_cast<int>(std::round(A0));
    int R = static_cast<int>(std::round(R0));

    for (double t = 0; t < t_max && A > 0 && R > 0; t += h) {
        double lambda_blue = total_rate(red_rates, R, R0, ...) * R * h;
        double lambda_red  = total_rate(blue_rates, A, A0, ...) * A * h;

        std::poisson_distribution<int> dist_blue(std::max(0.0, lambda_blue));
        std::poisson_distribution<int> dist_red(std::max(0.0, lambda_red));

        int blue_casualties = std::min(dist_blue(rng), A);
        int red_casualties  = std::min(dist_red(rng), R);

        A -= blue_casualties;
        R -= red_casualties;
    }
    // ... construir CombatResult con enteros
}
```

**Nota sobre el paso temporal `h`**: Con Poisson, `h` debe ser lo suficientemente pequeno para que `lambda = rate * N * h < 1` en la mayoria de pasos (evitar multiples bajas por paso que distorsionen la distribucion). Validar: si `lambda > 2`, subdividir el paso automaticamente.

**Funcion `run_montecarlo()`**:

```cpp
MonteCarloResult run_montecarlo(const CombatInput& input, int n_replicas, uint64_t seed) {
    std::mt19937 rng(seed);
    std::vector<CombatResult> results(n_replicas);

    for (int i = 0; i < n_replicas; ++i) {
        results[i] = simulate_combat_stochastic(input, rng);
    }

    // Calcular estadisticas: mean, std, percentiles, outcome counts
    return aggregate_mc_results(results);
}
```

---

### B.3 — Salida JSON del modo Monte Carlo

```json
{
  "scenario_id": "TEST-MC-01",
  "mode": "montecarlo",
  "n_replicas": 1000,
  "seed": 42,
  "combats": [
    {
      "combat_id": 1,
      "deterministic": {
        "outcome": "BLUE_WINS",
        "blue_survivors": 7.234,
        "red_survivors": 0.0
      },
      "montecarlo": {
        "outcome_distribution": {
          "BLUE_WINS": 0.823,
          "RED_WINS": 0.052,
          "DRAW": 0.125
        },
        "blue_survivors": {
          "mean": 6.8,
          "std": 2.1,
          "percentiles": { "p05": 3, "p25": 5, "p50": 7, "p75": 8, "p95": 11 }
        },
        "red_survivors": {
          "mean": 0.4,
          "std": 1.2,
          "percentiles": { "p05": 0, "p25": 0, "p50": 0, "p75": 0, "p95": 3 }
        },
        "duration_minutes": {
          "mean": 12.3,
          "std": 4.5
        }
      }
    }
  ]
}
```

Esto permite al usuario ver: "el determinista dice 7.2 supervivientes azules, pero en realidad hay un 5% de probabilidad de que queden solo 3 o menos, y un 5% de que rojo gane".

---

### B.4 — Encadenamiento estocastico

En el modo determinista, el encadenamiento pasa `blue_survivors` exactos al siguiente combate. En Monte Carlo:

**Opcion A — Replicas independientes por combate**: Cada combate se simula N veces por separado. Se pierde la correlacion entre combates. Simple pero incorrecto.

**Opcion B — Cadenas completas (recomendada)**: Cada replica ejecuta la cadena completa de combates. Los supervivientes (enteros) del combate k alimentan el combate k+1. Se preserva la correlacion. Las estadisticas se calculan sobre las N cadenas completas.

```cpp
for (int replica = 0; replica < N; ++replica) {
    vector<CombatResult> chain;
    int blue_carry = initial_blue;
    int red_carry = initial_red;

    for (auto& combat_def : combat_sequence) {
        CombatInput input = build_input(combat_def, blue_carry, red_carry);
        CombatResult r = simulate_combat_stochastic(input, rng);
        chain.push_back(r);
        blue_carry = static_cast<int>(r.blue_survivors);
        red_carry  = static_cast<int>(r.red_survivors);
    }
    all_chains.push_back(chain);
}
```

---

### B.5 — Validacion del modo Monte Carlo

1. **Test de convergencia**: Con N → infinito, la media de Monte Carlo debe converger al resultado determinista. Verificar que `|mean_MC(N=10000) - deterministic| < epsilon` para el escenario analitico (test_09).

2. **Test de varianza**: Para el caso simetrico (test_01, 10 vs 10), la varianza de supervivientes debe ser > 0 (no colapsar al determinista).

3. **Test de reproducibilidad**: Misma semilla → mismos resultados exactos.

4. **Test de limites**: Con fuerzas muy grandes (1000 vs 1000), la varianza relativa debe ser pequena (ley de los grandes numeros).

---

## PARTE C — Orden de implementacion

| Fase | Tarea | Dependencias | Esfuerzo |
|---|---|---|---|
| C.1 | Analisis de sensibilidad (A.1) | Ninguna | Medio |
| C.2 | Formato extendido de model_params.json (A.3) | Ninguna | Bajo |
| C.3 | Simulador estocastico (B.2) | Ninguna | Alto |
| C.4 | Salida MC + CLI (B.3) | C.3 | Medio |
| C.5 | Encadenamiento estocastico (B.4) | C.3 | Medio |
| C.6 | Tests de validacion MC (B.5) | C.3, C.4 | Medio |
| C.7 | Framework de calibracion (A.2) | C.1 | Alto |

**C.1 y C.2 se pueden hacer en paralelo con C.3.**

C.7 (calibracion real) depende de que alguien proporcione datos de referencia (`reference_data.json`). El framework se puede construir, pero sin datos queda vacio.

---

## Riesgos y decisiones abiertas

1. **Poisson vs Binomial**: El plan usa Poisson. Si el usuario prefiere un modelo mas riguroso (cada vehiculo decide independientemente si acierta), Binomial es mas correcto pero computacionalmente equivalente. Decision: Poisson por simplicidad, documentar la equivalencia.

2. **Rendimiento MC**: 1000 replicas x 18000 pasos (30 min / h=0.001) = 18M evaluaciones por combate. Estimacion: ~1-2 segundos en C++ single-thread. Aceptable. Si se necesita mas: paralelizar con OpenMP (trivial, replicas independientes).

3. **Paso adaptativo para Poisson**: Si `lambda > 2` en algun paso, hay riesgo de multiples bajas simultaneas que distorsionen. Subdividir automaticamente o advertir. Decision: implementar subdivision automatica, es sencillo.

4. **Datos de calibracion**: El framework de calibracion es util solo si hay datos. El plan lo construye como infraestructura; rellenarlo requiere input externo (ejercicios, JANUS, manuales).
