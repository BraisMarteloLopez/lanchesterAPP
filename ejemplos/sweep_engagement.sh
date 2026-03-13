#!/bin/bash
# Analisis de sensibilidad: barrido de fraccion de empenamiento
# Efecto de comprometer mas o menos fuerza al combate
./lanchester --scenario ejemplos/toa_vs_t80u.json \
    --sweep "combat_sequence[0].blue.engagement_fraction" 0.1 1.0 0.1 \
    --output resultados_sweep_engagement.csv
echo "Resultado en resultados_sweep_engagement.csv"
