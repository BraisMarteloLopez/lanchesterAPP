#!/bin/bash
# Analisis de sensibilidad: barrido de numero de vehiculos azules
# Cuantos TOA_SPIKE_I se necesitan para ganar contra 10 T-80U?
./lanchester --scenario ejemplos/toa_vs_t80u.json \
    --sweep "combat_sequence[0].blue.composition[0].count" 1 30 1 \
    --output resultados_sweep_count.csv
echo "Resultado en resultados_sweep_count.csv"
