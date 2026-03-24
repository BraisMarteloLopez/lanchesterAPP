// lanchester_model_iface.h — Interfaz abstracta del modelo de combate
#pragma once

#include "lanchester_types.h"
#include <random>
#include <string>

class ILanchesterModel {
public:
    virtual ~ILanchesterModel() = default;

    // Simulacion determinista de un combate individual
    virtual CombatResult simulate(const CombatInput& input) const = 0;

    // Simulacion estocastica (una replica)
    virtual CombatResult simulateStochastic(const CombatInput& input,
                                            std::mt19937& rng) const = 0;

    // Monte Carlo completo (N replicas + stats)
    virtual MonteCarloResult runMonteCarlo(const CombatInput& input,
                                           int n_replicas,
                                           std::mt19937& rng) const = 0;

    // Nombre del modelo
    virtual std::string name() const = 0;
};
