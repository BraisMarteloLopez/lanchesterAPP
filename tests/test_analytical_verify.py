#!/usr/bin/env python3
"""
DT-013: Verificacion del integrador numerico contra solucion analitica.

Modelo de Lanchester (ley cuadrada, sin C/C):
  dA/dt = -beta * R
  dR/dt = -alpha * A

Invariante: alpha * A^2 - beta * R^2 = constante
Si alpha*A0^2 > beta*R0^2, azul gana con:
  A_final = sqrt(A0^2 - (beta/alpha) * R0^2)

Este script:
1. Ejecuta el escenario test_09_analytical.json
2. Extrae las tasas (static_advantage = alpha*A0^2 / (beta*R0^2))
3. Calcula la solucion analitica
4. Compara contra el resultado numerico
"""

import json
import math
import subprocess
import sys

# Ejecutar simulacion
result = subprocess.run(
    ["./lanchester", "tests/test_09_analytical.json"],
    capture_output=True, text=True
)
if result.returncode != 0:
    print(f"FAIL: lanchester devolvio codigo {result.returncode}")
    print(result.stderr)
    sys.exit(1)

data = json.loads(result.stdout)
combat = data["combats"][0]

A0 = combat["blue_initial"]
R0 = combat["red_initial"]
blue_surv = combat["blue_survivors"]
red_surv = combat["red_survivors"]
static_adv = combat["static_advantage"]
outcome = combat["outcome"]

print(f"Resultado numerico:")
print(f"  A0={A0}, R0={R0}")
print(f"  blue_survivors={blue_surv:.6f}, red_survivors={red_surv:.6f}")
print(f"  static_advantage={static_adv:.6f}")
print(f"  outcome={outcome}")
print()

# De static_advantage = (alpha * A0^2) / (beta * R0^2)
# => beta/alpha = A0^2 / (static_adv * R0^2)
if static_adv <= 0:
    print("FAIL: static_advantage <= 0, no se puede verificar")
    sys.exit(1)

beta_over_alpha = (A0**2) / (static_adv * R0**2)

# Solucion analitica (ley cuadrada de Lanchester):
# Si alpha*A0^2 > beta*R0^2 (static_adv > 1): azul gana
# A_final = sqrt(A0^2 - (beta/alpha)*R0^2)
lanchester_invariant = A0**2 - beta_over_alpha * R0**2

if lanchester_invariant > 0:
    analytical_blue = math.sqrt(lanchester_invariant)
    analytical_red = 0.0
    expected_outcome = "BLUE_WINS"
elif lanchester_invariant < 0:
    analytical_blue = 0.0
    analytical_red = math.sqrt(-lanchester_invariant / beta_over_alpha)
    # Realmente: R_final = sqrt(R0^2 - (alpha/beta)*A0^2)
    analytical_red = math.sqrt(R0**2 - (1.0/beta_over_alpha) * A0**2)
    expected_outcome = "RED_WINS"
else:
    analytical_blue = 0.0
    analytical_red = 0.0
    expected_outcome = "DRAW"

print(f"Solucion analitica (ley cuadrada de Lanchester):")
print(f"  beta/alpha = {beta_over_alpha:.6f}")
print(f"  invariante = {lanchester_invariant:.6f}")
print(f"  A_final_analitico = {analytical_blue:.6f}")
print(f"  R_final_analitico = {analytical_red:.6f}")
print(f"  expected_outcome = {expected_outcome}")
print()

# Comparar
TOLERANCE = 0.5  # Tolerancia de 0.5 vehiculos (el modelo tiene distancia variable, terreno, etc.)
errors = []

if outcome != expected_outcome:
    errors.append(f"Outcome: esperado {expected_outcome}, obtenido {outcome}")

blue_error = abs(blue_surv - analytical_blue)
red_error = abs(red_surv - analytical_red)

print(f"Errores numericos:")
print(f"  |blue_surv - analitico| = {blue_error:.6f}")
print(f"  |red_surv - analitico|  = {red_error:.6f}")

# Nota: la tolerancia es amplia porque el modelo tiene distancia variable
# (DT-005) y multiplicador de terreno (DT-006) que la solucion analitica no
# contempla. La verificacion es de consistencia, no de precision exacta.
if blue_error > TOLERANCE:
    errors.append(f"blue_survivors: error {blue_error:.4f} > tolerancia {TOLERANCE}")
if red_error > TOLERANCE:
    errors.append(f"red_survivors: error {red_error:.4f} > tolerancia {TOLERANCE}")

print()
if errors:
    for e in errors:
        print(f"  FAIL: {e}")
    sys.exit(1)
else:
    print("  OK: Resultado numerico consistente con solucion analitica")
    sys.exit(0)
