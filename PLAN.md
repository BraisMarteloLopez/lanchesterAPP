# Plan de Diseno — DT-002 y DT-003

## Contexto

Dos problemas fundamentales que afectaban la fiabilidad del modelo:

1. **DT-002**: Las constantes del modelo no estaban calibradas. Los resultados eran numeros sin significado verificable.
2. **DT-003**: El modelo era determinista aplicado a fuerzas de 6-20 vehiculos. No habia incertidumbre en los resultados.

Ambos han sido resueltos. Este documento describe el diseno implementado.

---

## PARTE A — DT-002: Calibracion de constantes (RESUELTO)

### A.1 — Analisis de sensibilidad automatizado

**Objetivo**: Cuantificar cuanto cambia el resultado al variar cada parametro. Identificar cuales dominan y cuales son irrelevantes.

**Implementacion**:

La funcionalidad de sensibilidad esta implementada en `lanchester_io.h` (`run_sensitivity()`). La GUI no expone esta funcion directamente, pero el motor interno la soporta para analisis programatico.

Funcionamiento:
1. Ejecutar el escenario base (resultado de referencia).
2. Para cada parametro en `model_params.json`, barrer ±20% en 5 pasos (0.8x, 0.9x, 1.0x, 1.1x, 1.2x).
3. Para cada vehiculo en los catalogos, barrer D, P, U, c individualmente ±20%.
4. Registrar: `delta_blue_survivors`, `delta_red_survivors`, `delta_outcome` respecto a la referencia.

**Metricas de sensibilidad**:
- **Elasticidad**: `(delta_output / output_ref) / (delta_param / param_ref)` — cuanto cambia el resultado (%) por 1% de cambio en el parametro.
- Ordenar parametros por elasticidad absoluta descendente.

---

### A.2 — Parametros externalizados con trazabilidad

**Objetivo**: Cada parametro debe tener trazabilidad.

Todas las constantes estan externalizadas a `model_params.json` (junto al .exe en `release/`):

- `kill_probability_slope`: 175.0
- `distance_degradation_coefficients`: 6 coeficientes del polinomio
- `tactical_multipliers`: todos los estados tacticos
- `terrain_fire_effectiveness`: multiplicadores por tipo de terreno

Cada parametro incluye metadatos (`_comment`) documentando su funcion y estado de calibracion. El loader (`load_model_params`) acepta tanto formato plano como extendido:

```cpp
// Si es objeto con "value", extraer .value; si es escalar, usar directamente
double read_param(const json& j, const std::string& key, double default_val) {
    if (!j.contains(key)) return default_val;
    if (j[key].is_object()) return j[key].value("value", default_val);
    return j[key].get<double>();
}
```

**Nota**: La calibracion contra datos reales (ejercicios, JANUS, wargames) sigue pendiente. Los valores estan externalizados pero no validados empiricamente. Para calibrar, se necesitarian datos de referencia proporcionados por el usuario.

---

## PARTE B — DT-003: Modo estocastico Monte Carlo (RESUELTO)

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

---

### B.2 — Implementacion

El modo Monte Carlo esta integrado en la aplicacion GUI (`lanchester_gui.exe`). El usuario selecciona el modo en la interfaz grafica, configura el numero de replicas y la semilla, y ejecuta la simulacion.

**Estructura de datos** (`lanchester_model.h`):

```cpp
struct MonteCarloResult {
    int n_replicas;
    // Estadisticas de blue_survivors
    double blue_surv_mean, blue_surv_std;
    double blue_surv_p05, blue_surv_p25, blue_surv_median, blue_surv_p75, blue_surv_p95;
    // Idem para red
    double red_surv_mean, red_surv_std;
    double red_surv_p05, red_surv_p25, red_surv_median, red_surv_p75, red_surv_p95;
    // Distribucion de outcomes
    int count_blue_wins, count_red_wins, count_draw, count_indeterminate;
    // Duracion
    double duration_mean, duration_std;
    // Resultado determinista para comparacion
    CombatResult deterministic;
};
```

**Simulacion estocastica** (`simulate_combat_stochastic()`):

```cpp
CombatResult simulate_combat_stochastic(const CombatInput& input, std::mt19937& rng) {
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
}
```

**Nota sobre el paso temporal `h`**: Si `lambda > 2`, el paso se subdivide automaticamente para evitar distorsion en la distribucion.

---

### B.3 — Visualizacion en la GUI

La aplicacion Windows muestra los resultados Monte Carlo con:

- **Probabilidades de victoria**: P(Blue wins), P(Red wins), P(Draw)
- **Tabla de percentiles**: p05, p25, mediana, p75, p95 para supervivientes de cada bando
- **Graficas de barras** (implot): distribucion de outcomes y supervivientes
- **Comparacion**: resultado determinista junto al estocastico

---

### B.4 — Encadenamiento estocastico

Cada replica ejecuta la cadena completa de combates. Los supervivientes (enteros) del combate k alimentan el combate k+1. Se preserva la correlacion entre combates.

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

1. **Convergencia**: Con N → infinito, la media MC converge al resultado determinista.
2. **Varianza**: Para caso simetrico (10 vs 10), la varianza de supervivientes > 0.
3. **Reproducibilidad**: Misma semilla → mismos resultados exactos.
4. **Limites**: Con fuerzas grandes (1000 vs 1000), varianza relativa pequena.

Estas validaciones se pueden ejecutar desde la propia GUI usando los tests del `PLAN_DE_PRUEBAS.md` (Tests 4.1-4.3).

---

## PARTE C — Estado de implementacion

| Fase | Tarea | Estado |
|---|---|---|
| C.1 | Analisis de sensibilidad (A.1) | IMPLEMENTADO |
| C.2 | Formato extendido de model_params.json (A.2) | IMPLEMENTADO |
| C.3 | Simulador estocastico (B.2) | IMPLEMENTADO |
| C.4 | Visualizacion MC en GUI (B.3) | IMPLEMENTADO |
| C.5 | Encadenamiento estocastico (B.4) | IMPLEMENTADO |
| C.6 | Validacion MC via GUI (B.5) | DISPONIBLE |
| C.7 | Calibracion contra datos reales | PENDIENTE (requiere datos de referencia) |

---

## Decisiones de diseno

1. **Poisson vs Binomial**: Se usa Poisson por simplicidad. Para `p` pequeno y `R` moderado, Binomial ≈ Poisson. Documentada la equivalencia.

2. **Rendimiento MC**: 1000 replicas x 18000 pasos = 18M evaluaciones por combate. ~1-2 segundos en C++ single-thread. La GUI ejecuta la simulacion en hilo separado para no bloquear la interfaz.

3. **Paso adaptativo para Poisson**: Subdivision automatica si `lambda > 2` para evitar multiples bajas simultaneas que distorsionen la distribucion.

4. **Datos de calibracion**: El motor soporta calibracion, pero requiere datos de referencia externos (ejercicios, JANUS, manuales doctrinales) que deben ser proporcionados por el usuario.

5. **Plataforma Windows**: La aplicacion se distribuye como ejecutable Windows (.exe) con interfaz grafica Dear ImGui + SDL2. Se compila mediante cross-compilacion MinGW-w64 desde Linux.
