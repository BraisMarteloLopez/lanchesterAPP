#!/bin/bash
# Script de validacion — Sesion 3
# Ejecuta todos los casos de prueba y verifica invariantes

set -e

BINARY="./lanchester"
TESTS_DIR="tests"
PASS=0
FAIL=0
TOTAL=0

red()   { printf "\033[31m%s\033[0m" "$1"; }
green() { printf "\033[32m%s\033[0m" "$1"; }

check() {
    local desc="$1" cond="$2"
    TOTAL=$((TOTAL + 1))
    if eval "$cond"; then
        PASS=$((PASS + 1))
        printf "  [%s] %s\n" "$(green "OK")" "$desc"
    else
        FAIL=$((FAIL + 1))
        printf "  [%s] %s\n" "$(red "FAIL")" "$desc"
    fi
}

# Helper: extraer campo JSON con jq-like usando python
jval() {
    python3 -c "import json,sys; d=json.load(sys.stdin); print(d$1)" <<< "$2"
}

echo "============================================="
echo " Validacion del Modelo Lanchester-CIO"
echo "============================================="
echo ""

# --- TEST 01: Combate simetrico ---
echo "TEST-01: Combate simetrico (T-80U vs T-80U, 10 vs 10)"
OUT=$($BINARY $TESTS_DIR/test_01_symmetric.json)
outcome=$(jval "['combats'][0]['outcome']" "$OUT")
blue_surv=$(jval "['combats'][0]['blue_survivors']" "$OUT")
red_surv=$(jval "['combats'][0]['red_survivors']" "$OUT")
blue_init=$(jval "['combats'][0]['blue_initial']" "$OUT")
red_init=$(jval "['combats'][0]['red_initial']" "$OUT")
static_adv=$(jval "['combats'][0]['static_advantage']" "$OUT")

check "Outcome es DRAW" '[ "$outcome" = "DRAW" ]'
check "Static advantage = 1.0 (simetrico)" 'python3 -c "assert abs($static_adv - 1.0) < 0.001"'
check "Fuerzas iniciales iguales (10 vs 10)" 'python3 -c "assert abs($blue_init - $red_init) < 0.01"'
check "Fuerzas iniciales = 10" 'python3 -c "assert abs($blue_init - 10.0) < 0.01"'
echo ""

# --- TEST 02: Fuerza abrumadora ---
echo "TEST-02: Fuerza abrumadora (20 LEO2E vs 2 T-80U)"
OUT=$($BINARY $TESTS_DIR/test_02_overwhelming.json)
outcome=$(jval "['combats'][0]['outcome']" "$OUT")
blue_surv=$(jval "['combats'][0]['blue_survivors']" "$OUT")
blue_init=$(jval "['combats'][0]['blue_initial']" "$OUT")

check "Outcome es BLUE_WINS" '[ "$outcome" = "BLUE_WINS" ]'
check "Supervivientes azul > 18 (bajas minimas)" 'python3 -c "assert $blue_surv > 18.0"'
check "Fuerzas iniciales azul = 20" 'python3 -c "assert abs($blue_init - 20.0) < 0.01"'
echo ""

# --- TEST 03: Sin C/C ---
echo "TEST-03: Sin capacidad C/C (VEC-25 vs BTR-82A)"
OUT=$($BINARY $TESTS_DIR/test_03_no_cc.json)
blue_cc=$(jval "['combats'][0]['blue_cc_ammo_consumed']" "$OUT")
red_cc=$(jval "['combats'][0]['red_cc_ammo_consumed']" "$OUT")

check "Municion C/C azul = 0" 'python3 -c "assert abs($blue_cc) < 0.001"'
check "Municion C/C roja = 0" 'python3 -c "assert abs($red_cc) < 0.001"'
echo ""

# --- TEST 04: Fuera de alcance ---
echo "TEST-04: Fuera de alcance (9000m, nadie dispara)"
OUT=$($BINARY $TESTS_DIR/test_04_out_of_range.json)
outcome=$(jval "['combats'][0]['outcome']" "$OUT")
blue_surv=$(jval "['combats'][0]['blue_survivors']" "$OUT")
red_surv=$(jval "['combats'][0]['red_survivors']" "$OUT")
blue_init=$(jval "['combats'][0]['blue_initial']" "$OUT")
red_init=$(jval "['combats'][0]['red_initial']" "$OUT")

check "Outcome es INDETERMINATE" '[ "$outcome" = "INDETERMINATE" ]'
check "Supervivientes azul = iniciales (sin bajas)" 'python3 -c "assert abs($blue_surv - $blue_init) < 0.01"'
check "Supervivientes rojo = iniciales (sin bajas)" 'python3 -c "assert abs($red_surv - $red_init) < 0.01"'
echo ""

# --- TEST 05: Bajas AFT ---
echo "TEST-05: Bajas AFT (50% bajas pre-contacto)"
OUT=$($BINARY $TESTS_DIR/test_05_aft_casualties.json)
blue_init=$(jval "['combats'][0]['blue_initial']" "$OUT")
red_init=$(jval "['combats'][0]['red_initial']" "$OUT")

check "Azul inicia con 5.0 (10 * 0.5)" 'python3 -c "assert abs($blue_init - 5.0) < 0.01"'
check "Rojo inicia con 10.0 (sin AFT)" 'python3 -c "assert abs($red_init - 10.0) < 0.01"'
echo ""

# --- TEST 06: Multiplicador defensivo ---
echo "TEST-06: Defensiva organizacion media (mult 1/4.25^2 = 0.0554)"
OUT=$($BINARY $TESTS_DIR/test_06_defense_mult.json)
outcome=$(jval "['combats'][0]['outcome']" "$OUT")
blue_surv=$(jval "['combats'][0]['blue_survivors']" "$OUT")

check "Outcome es BLUE_WINS (defensor gana pese a inferioridad numerica)" '[ "$outcome" = "BLUE_WINS" ]'
check "Defensor tiene supervivientes" 'python3 -c "assert $blue_surv > 0.5"'
echo ""

# --- TEST 07: Fuerzas mixtas ---
echo "TEST-07: Fuerzas mixtas (LEO2E+PIZARRO vs T-80U+BMP-3)"
OUT=$($BINARY $TESTS_DIR/test_07_mixed_forces.json)
outcome=$(jval "['combats'][0]['outcome']" "$OUT")
blue_init=$(jval "['combats'][0]['blue_initial']" "$OUT")
red_init=$(jval "['combats'][0]['red_initial']" "$OUT")
blue_cc=$(jval "['combats'][0]['blue_cc_ammo_consumed']" "$OUT")
red_cc=$(jval "['combats'][0]['red_cc_ammo_consumed']" "$OUT")

check "Fuerzas iniciales azul = 12" 'python3 -c "assert abs($blue_init - 12.0) < 0.01"'
check "Fuerzas iniciales rojo = 16" 'python3 -c "assert abs($red_init - 16.0) < 0.01"'
check "Azul consume municion C/C (PIZARRO tiene C/C)" 'python3 -c "assert $blue_cc > 0"'
check "Rojo consume municion C/C (BMP-3 tiene C/C)" 'python3 -c "assert $red_cc > 0"'
echo ""

# --- TEST 08: Engagement fraction + count_factor ---
echo "TEST-08: Engagement fraction y count_factor (10 * 0.5 * 2.0 = 10)"
OUT=$($BINARY $TESTS_DIR/test_08_engagement_fraction.json)
blue_init=$(jval "['combats'][0]['blue_initial']" "$OUT")
red_init=$(jval "['combats'][0]['red_initial']" "$OUT")

check "Azul inicia con 10.0 (10 * 0.5 * 2.0)" 'python3 -c "assert abs($blue_init - 10.0) < 0.01"'
check "Rojo inicia con 10.0" 'python3 -c "assert abs($red_init - 10.0) < 0.01"'
echo ""

# --- Invariantes generales ---
echo "INVARIANTES: Verificacion en todos los tests"
for test_file in $TESTS_DIR/test_*.json; do
    name=$(basename "$test_file" .json)
    OUT=$($BINARY "$test_file")
    n_combats=$(python3 -c "import json; print(len(json.loads('''$OUT''')['combats']))")
    for i in $(seq 0 $((n_combats - 1))); do
        blue_init=$(jval "['combats'][$i]['blue_initial']" "$OUT")
        blue_surv=$(jval "['combats'][$i]['blue_survivors']" "$OUT")
        blue_cas=$(jval "['combats'][$i]['blue_casualties']" "$OUT")
        red_init=$(jval "['combats'][$i]['red_initial']" "$OUT")
        red_surv=$(jval "['combats'][$i]['red_survivors']" "$OUT")
        red_cas=$(jval "['combats'][$i]['red_casualties']" "$OUT")

        check "$name[c$i]: bajas_azul = inicial - supervivientes" \
            "python3 -c \"assert abs(($blue_init - $blue_surv) - $blue_cas) < 0.02\""
        check "$name[c$i]: bajas_rojo = inicial - supervivientes" \
            "python3 -c \"assert abs(($red_init - $red_surv) - $red_cas) < 0.02\""
        check "$name[c$i]: supervivientes >= 0" \
            "python3 -c \"assert $blue_surv >= 0 and $red_surv >= 0\""
    done
done
echo ""

# --- Comparacion pre-tasa vs post-tasa ---
echo "============================================="
echo " Comparacion: Pre-tasa vs Post-tasa"
echo "============================================="
echo ""
printf "%-25s | %-10s | %-14s %-14s | %-14s %-14s\n" \
    "Test" "Campo" "Pre-tasa" "Post-tasa" "Diff" "Diff%"
printf "%-25s-+-%-10s-+-%-14s-%-14s-+-%-14s-%-14s\n" \
    "-------------------------" "----------" "--------------" "--------------" "--------------" "--------------"

for test_file in $TESTS_DIR/test_*.json; do
    name=$(basename "$test_file" .json)
    OUT_PRE=$($BINARY "$test_file" --aggregation pre)
    OUT_POST=$($BINARY "$test_file" --aggregation post)

    for field in blue_survivors red_survivors static_advantage; do
        val_pre=$(jval "['combats'][0]['$field']" "$OUT_PRE")
        val_post=$(jval "['combats'][0]['$field']" "$OUT_POST")
        diff=$(python3 -c "d=$val_post-$val_pre; print(f'{d:+.6f}')")
        pct=$(python3 -c "
v=max(abs($val_pre),abs($val_post),0.0001)
d=abs($val_post-$val_pre)
print(f'{100*d/v:.2f}%')
")
        printf "%-25s | %-10s | %14s %14s | %14s %14s\n" \
            "$name" "$field" "$val_pre" "$val_post" "$diff" "$pct"
    done
done
echo ""

# --- Resumen ---
echo "============================================="
printf " Resultado: %s pasados, %s fallidos de %s\n" \
    "$(green "$PASS")" "$(red "$FAIL")" "$TOTAL"
echo "============================================="

exit $FAIL
