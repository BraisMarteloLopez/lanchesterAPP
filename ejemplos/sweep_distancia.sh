#!/bin/bash
# Analisis de sensibilidad: barrido de distancia de enfrentamiento
# Genera CSV con resultados para distancias de 500 a 5000m (paso 100m)
./lanchester --scenario ejemplos/toa_vs_t80u.json \
    --sweep engagement_distance_m 500 5000 100 \
    --output resultados_sweep_distancia.csv
echo "Resultado en resultados_sweep_distancia.csv"
