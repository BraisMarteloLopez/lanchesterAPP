#!/usr/bin/env python3
"""
Validacion del modo Monte Carlo:

1. Convergencia: con N grande, la media MC converge al resultado determinista.
2. Varianza: para fuerzas pequenas, la varianza es > 0.
3. Reproducibilidad: misma semilla = mismos resultados.
4. Limites: fuerzas grandes -> varianza relativa pequena.
5. Simetria: 10v10 simetrico -> ~50/50 outcomes.
"""

import json
import subprocess
import sys
import math

BINARY = "./lanchester"
errors = []
tests_run = 0
tests_passed = 0


def run_mc(scenario, n_replicas, seed=42):
    result = subprocess.run(
        [BINARY, scenario, "--montecarlo", str(n_replicas), "--seed", str(seed)],
        capture_output=True, text=True
    )
    if result.returncode != 0:
        raise RuntimeError(f"lanchester devolvio {result.returncode}: {result.stderr}")
    return json.loads(result.stdout)


def check(name, condition, msg=""):
    global tests_run, tests_passed
    tests_run += 1
    if condition:
        tests_passed += 1
        print(f"  OK: {name}")
    else:
        errors.append(f"{name}: {msg}")
        print(f"  FAIL: {name} — {msg}")


print("=== Test 1: Reproducibilidad (misma semilla) ===")
d1 = run_mc("tests/test_01_symmetric.json", 500, seed=12345)
d2 = run_mc("tests/test_01_symmetric.json", 500, seed=12345)
mc1 = d1["combats"][0]["montecarlo"]
mc2 = d2["combats"][0]["montecarlo"]
check("reproducibilidad_blue_mean",
      mc1["blue_survivors"]["mean"] == mc2["blue_survivors"]["mean"],
      f"{mc1['blue_survivors']['mean']} != {mc2['blue_survivors']['mean']}")
check("reproducibilidad_red_mean",
      mc1["red_survivors"]["mean"] == mc2["red_survivors"]["mean"],
      f"{mc1['red_survivors']['mean']} != {mc2['red_survivors']['mean']}")
check("reproducibilidad_outcomes",
      mc1["outcome_distribution"] == mc2["outcome_distribution"],
      f"outcomes differ")

print("\n=== Test 2: Simetria (10v10 simetrico -> ~50/50) ===")
d = run_mc("tests/test_01_symmetric.json", 2000, seed=42)
mc = d["combats"][0]["montecarlo"]
bw = mc["outcome_distribution"]["BLUE_WINS"]
rw = mc["outcome_distribution"]["RED_WINS"]
check("simetria_blue_wins_rango",
      0.35 < bw < 0.65,
      f"BLUE_WINS = {bw}, esperado ~0.5")
check("simetria_red_wins_rango",
      0.35 < rw < 0.65,
      f"RED_WINS = {rw}, esperado ~0.5")
check("simetria_diferencia",
      abs(bw - rw) < 0.15,
      f"|BW-RW| = {abs(bw-rw):.3f}, esperado < 0.15")

print("\n=== Test 3: Varianza > 0 para fuerzas pequenas ===")
check("varianza_blue_positiva",
      mc["blue_survivors"]["std"] > 0.5,
      f"std = {mc['blue_survivors']['std']}, esperado > 0.5")
check("varianza_red_positiva",
      mc["red_survivors"]["std"] > 0.5,
      f"std = {mc['red_survivors']['std']}, esperado > 0.5")

print("\n=== Test 4: Convergencia (media MC vs determinista) ===")
d = run_mc("tests/test_09_analytical.json", 5000, seed=42)
mc = d["combats"][0]["montecarlo"]
det = d["combats"][0]["deterministic"]
# Para un caso donde blue gana claramente (15v10), la media MC debe estar
# cerca del determinista (dentro de 2 unidades)
det_blue = det["blue_survivors"]
mc_blue_mean = mc["blue_survivors"]["mean"]
mc_red_mean = mc["red_survivors"]["mean"]
delta_blue = abs(mc_blue_mean - det_blue)
check("convergencia_blue_mean",
      delta_blue < 2.0,
      f"|MC_mean - det| = {delta_blue:.2f}, esperado < 2.0 (det={det_blue:.2f}, mc_mean={mc_blue_mean:.2f})")
# Red should be close to 0 in both
check("convergencia_red_near_zero",
      mc_red_mean < 2.0,
      f"MC red_mean = {mc_red_mean:.2f}, esperado < 2.0")

print("\n=== Test 5: Blue gana mayoria en caso asimetrico (15v10) ===")
bw = mc["outcome_distribution"]["BLUE_WINS"]
check("asimetrico_blue_wins_dominante",
      bw > 0.7,
      f"BLUE_WINS = {bw}, esperado > 0.7")

print("\n=== Test 6: Percentiles coherentes ===")
bs = mc["blue_survivors"]
check("percentiles_ordenados",
      bs["p05"] <= bs["p25"] <= bs["median"] <= bs["p75"] <= bs["p95"],
      f"p05={bs['p05']} p25={bs['p25']} median={bs['median']} p75={bs['p75']} p95={bs['p95']}")
check("p95_mayor_que_media",
      bs["p95"] >= bs["mean"] - 0.1,
      f"p95={bs['p95']} < mean={bs['mean']}")

print(f"\n{'='*50}")
print(f"Resultado: {tests_passed}/{tests_run} tests pasados")
if errors:
    print("Errores:")
    for e in errors:
        print(f"  - {e}")
    sys.exit(1)
else:
    print("Todos los tests de Monte Carlo pasados.")
    sys.exit(0)
