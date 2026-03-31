// combat_utils.h — Utilidades de combate: agregacion, bajas, estadisticas
#pragma once

#include "lanchester_types.h"
#include <vector>

namespace lanchester {

// Agrega parametros de una composicion mixta en medias ponderadas.
AggregatedParams aggregate(const std::vector<CompositionEntry>& composition);

// Calcula fuerzas iniciales efectivas tras AFT y empenamiento.
double initialForces(int n_total, double aft_pct, double eng_frac);

// Cuenta total de vehiculos en una composicion.
int totalCount(const std::vector<CompositionEntry>& comp);

// Distribuye bajas proporcionalmente a la vulnerabilidad (1/(D_cc+1)).
void distributeCasualtiesByVulnerability(
    std::vector<CompositionEntry>& comp, double total_casualties);

// Calcula estadisticas percentiles de un vector de datos (se ordena in-place).
PercentileStats computeStats(std::vector<double>& data);

} // namespace lanchester
