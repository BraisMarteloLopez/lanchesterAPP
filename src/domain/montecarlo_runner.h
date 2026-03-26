// montecarlo_runner.h — Orquestador de simulaciones Monte Carlo
#pragma once

#include "lanchester_model_iface.h"
#include "lanchester_types.h"
#include <random>

class MonteCarloRunner {
public:
    // Ejecuta N replicas estocasticas + una determinista de referencia.
    // Funciona con cualquier IStochasticModel.
    static MonteCarloResult run(const IStochasticModel& model,
                                const CombatInput& input,
                                int n_replicas,
                                std::mt19937& rng);
};
