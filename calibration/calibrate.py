#!/usr/bin/env python3
"""
Framework de calibracion del modelo Lanchester-CIO.

Compara resultados del modelo contra datos de referencia y opcionalmente
optimiza los parametros del modelo para minimizar el error total.

Uso:
  python3 calibration/calibrate.py [--optimize] [--output report.json]

Requiere:
  - ./lanchester compilado
  - calibration/reference_data.json con casos de referencia
  - scipy (solo para --optimize)
"""

import json
import subprocess
import sys
import os
import argparse
import copy
from pathlib import Path

BINARY = "./lanchester"
REFERENCE_DATA = "calibration/reference_data.json"
MODEL_PARAMS = "model_params.json"


def run_scenario(scenario_path):
    """Ejecuta un escenario y devuelve el resultado."""
    result = subprocess.run(
        [BINARY, scenario_path],
        capture_output=True, text=True
    )
    if result.returncode != 0:
        raise RuntimeError(f"Error ejecutando {scenario_path}: {result.stderr}")
    return json.loads(result.stdout)


def evaluate_case(case, result):
    """Evalua un caso de referencia contra el resultado del modelo.
    Devuelve un dict con errores por metrica."""
    expected = case["expected"]
    combat = result["combats"][-1]  # ultimo combate

    errors = {}
    total_error = 0.0

    # Outcome
    if "outcome" in expected:
        outcome_match = combat["outcome"] == expected["outcome"]
        errors["outcome"] = {"match": outcome_match, "expected": expected["outcome"],
                             "actual": combat["outcome"]}
        if not outcome_match:
            total_error += 10.0  # penalizacion fuerte por outcome incorrecto

    # Blue survivors
    if "blue_survivors" in expected:
        actual = combat["blue_survivors"]
        lo, hi = expected["blue_survivors"]["min"], expected["blue_survivors"]["max"]
        mid = (lo + hi) / 2
        rng = (hi - lo) / 2 if hi > lo else 1.0
        err = max(0, (actual - hi)) + max(0, (lo - actual))
        normalized = (err / rng) ** 2 if rng > 0 else 0
        errors["blue_survivors"] = {"actual": actual, "expected_range": [lo, hi],
                                     "error": err, "normalized": normalized}
        total_error += normalized

    # Red survivors
    if "red_survivors" in expected:
        actual = combat["red_survivors"]
        lo, hi = expected["red_survivors"]["min"], expected["red_survivors"]["max"]
        rng = (hi - lo) / 2 if hi > lo else 1.0
        err = max(0, (actual - hi)) + max(0, (lo - actual))
        normalized = (err / rng) ** 2 if rng > 0 else 0
        errors["red_survivors"] = {"actual": actual, "expected_range": [lo, hi],
                                    "error": err, "normalized": normalized}
        total_error += normalized

    # Duration
    if "duration_minutes" in expected:
        actual = combat["duration_contact_minutes"]
        lo, hi = expected["duration_minutes"]["min"], expected["duration_minutes"]["max"]
        rng = (hi - lo) / 2 if hi > lo else 1.0
        err = max(0, (actual - hi)) + max(0, (lo - actual))
        normalized = (err / rng) ** 2 if rng > 0 else 0
        errors["duration_minutes"] = {"actual": actual, "expected_range": [lo, hi],
                                       "error": err, "normalized": normalized}
        total_error += normalized

    weight = case.get("weight", 1.0)
    return {"case_id": case["id"], "errors": errors,
            "total_error": total_error, "weighted_error": total_error * weight}


def load_reference_data():
    """Carga los casos de referencia."""
    with open(REFERENCE_DATA) as f:
        data = json.load(f)
    return data["reference_cases"]


def load_model_params():
    """Carga los parametros del modelo."""
    with open(MODEL_PARAMS) as f:
        return json.load(f)


def save_model_params(params):
    """Guarda los parametros del modelo."""
    with open(MODEL_PARAMS, "w") as f:
        json.dump(params, f, indent=2)
        f.write("\n")


def read_param_value(params, key):
    """Lee un valor de parametro (soporta formato plano y extendido)."""
    val = params.get(key)
    if isinstance(val, dict) and "value" in val:
        return val["value"]
    return val


def set_param_value(params, key, value):
    """Establece un valor de parametro (soporta formato plano y extendido)."""
    if isinstance(params.get(key), dict) and "value" in params[key]:
        params[key]["value"] = value
    else:
        params[key] = value


def evaluate_all(cases):
    """Evalua todos los casos de referencia con los parametros actuales."""
    results = []
    for case in cases:
        scenario_path = case["scenario"]
        if not os.path.exists(scenario_path):
            print(f"  SKIP: {case['id']} — {scenario_path} no existe")
            continue
        try:
            output = run_scenario(scenario_path)
            evaluation = evaluate_case(case, output)
            results.append(evaluation)
        except Exception as e:
            print(f"  ERROR: {case['id']} — {e}")
            results.append({"case_id": case["id"], "error": str(e),
                           "total_error": 100.0, "weighted_error": 100.0})
    return results


def print_report(results):
    """Imprime un informe de evaluacion."""
    print("\n" + "=" * 70)
    print("INFORME DE CALIBRACION")
    print("=" * 70)

    total_weighted = 0
    for r in results:
        case_id = r["case_id"]
        we = r["weighted_error"]
        total_weighted += we

        if "error" in r and isinstance(r["error"], str):
            print(f"\n  {case_id}: ERROR — {r['error']}")
            continue

        status = "OK" if we < 0.01 else ("WARN" if we < 1.0 else "FAIL")
        print(f"\n  {case_id}: {status} (error ponderado = {we:.4f})")

        for metric, info in r.get("errors", {}).items():
            if metric == "outcome":
                sym = "v" if info["match"] else "X"
                print(f"    [{sym}] outcome: esperado={info['expected']}, "
                      f"actual={info['actual']}")
            else:
                actual = info["actual"]
                lo, hi = info["expected_range"]
                in_range = lo <= actual <= hi
                sym = "v" if in_range else "X"
                print(f"    [{sym}] {metric}: actual={actual:.2f}, "
                      f"rango=[{lo}, {hi}], error={info['error']:.2f}")

    print(f"\n  Error total ponderado: {total_weighted:.4f}")
    return total_weighted


def optimize_params(cases):
    """Optimiza parametros del modelo para minimizar error total."""
    try:
        from scipy.optimize import minimize
    except ImportError:
        print("ERROR: scipy no disponible. Instalar con: pip install scipy")
        sys.exit(1)

    original_params = load_model_params()

    # Parametros a optimizar con sus rangos
    param_defs = [
        ("kill_probability_slope", 50.0, 500.0),
    ]
    # Anadir coeficientes de distancia
    dd = original_params.get("distance_degradation_coefficients", {})
    for key in ["c_dk", "c_f", "c_dk2", "c_dk_f", "c_f2", "c_const"]:
        val = dd.get(key)
        if isinstance(val, dict) and "valid_range" in val:
            lo, hi = val["valid_range"]
        else:
            lo, hi = -2.0, 3.0
        param_defs.append((f"dd.{key}", lo, hi))

    # Terreno
    te = original_params.get("terrain_fire_effectiveness", {})
    for key in ["FACIL", "MEDIO", "DIFICIL"]:
        val = te.get(key)
        if isinstance(val, dict) and "valid_range" in val:
            lo, hi = val["valid_range"]
        else:
            lo, hi = 0.3, 1.0
        param_defs.append((f"te.{key}", lo, hi))

    # Vector inicial
    x0 = []
    for name, lo, hi in param_defs:
        if name == "kill_probability_slope":
            x0.append(read_param_value(original_params, "kill_probability_slope"))
        elif name.startswith("dd."):
            k = name[3:]
            x0.append(read_param_value(dd, k) if isinstance(dd.get(k), dict) else dd.get(k, 0))
        elif name.startswith("te."):
            k = name[3:]
            x0.append(read_param_value(te, k) if isinstance(te.get(k), dict) else te.get(k, 1))

    bounds = [(lo, hi) for _, lo, hi in param_defs]

    def objective(x):
        params = copy.deepcopy(original_params)
        for i, (name, _, _) in enumerate(param_defs):
            if name == "kill_probability_slope":
                set_param_value(params, "kill_probability_slope", x[i])
            elif name.startswith("dd."):
                k = name[3:]
                dd_section = params["distance_degradation_coefficients"]
                set_param_value(dd_section, k, x[i])
            elif name.startswith("te."):
                k = name[3:]
                te_section = params["terrain_fire_effectiveness"]
                set_param_value(te_section, k, x[i])

        save_model_params(params)
        results = evaluate_all(cases)
        total = sum(r["weighted_error"] for r in results)
        return total

    print("\nOptimizando parametros...")
    print(f"  {len(param_defs)} parametros, {len(cases)} casos de referencia")

    result = minimize(objective, x0, method="Nelder-Mead", bounds=bounds,
                      options={"maxiter": 200, "xatol": 0.01, "fatol": 0.001,
                               "disp": True})

    # Aplicar mejores parametros
    objective(result.x)
    print(f"\n  Optimizacion completada. Error final: {result.fun:.4f}")
    print(f"  Parametros optimizados guardados en {MODEL_PARAMS}")

    # Mostrar cambios
    print("\n  Cambios:")
    for i, (name, _, _) in enumerate(param_defs):
        if abs(x0[i] - result.x[i]) > 1e-6:
            print(f"    {name}: {x0[i]:.6f} -> {result.x[i]:.6f}")

    # Restaurar originales si no se quiere aplicar
    save_model_params(original_params)
    return result


def main():
    parser = argparse.ArgumentParser(description="Calibracion del modelo Lanchester-CIO")
    parser.add_argument("--optimize", action="store_true",
                        help="Optimizar parametros (requiere scipy)")
    parser.add_argument("--output", type=str, default="",
                        help="Guardar informe en fichero JSON")
    args = parser.parse_args()

    if not os.path.exists(BINARY):
        print(f"ERROR: {BINARY} no encontrado. Compilar primero.")
        sys.exit(1)

    cases = load_reference_data()
    if not cases:
        print("No hay casos de referencia en reference_data.json")
        sys.exit(1)

    print(f"Cargados {len(cases)} casos de referencia.")

    results = evaluate_all(cases)
    total_error = print_report(results)

    if args.optimize:
        optimize_params(cases)
        # Re-evaluar con parametros originales
        results = evaluate_all(cases)
        print_report(results)

    if args.output:
        report = {
            "total_error": total_error,
            "cases": results,
            "model_params_file": MODEL_PARAMS
        }
        with open(args.output, "w") as f:
            json.dump(report, f, indent=2)
        print(f"\nInforme guardado en {args.output}")


if __name__ == "__main__":
    main()
