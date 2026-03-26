// lanchester_model_iface.h — Interfaces abstractas del modelo de combate
#pragma once

#include "lanchester_types.h"
#include <random>
#include <string>

// Modelo de simulacion determinista
class ISimulationModel {
public:
    virtual ~ISimulationModel() = default;
    virtual CombatResult simulate(const CombatInput& input) const = 0;
    virtual std::string name() const = 0;
};

// Modelo de simulacion estocastica (extiende determinista)
class IStochasticModel : public ISimulationModel {
public:
    virtual CombatResult simulateStochastic(const CombatInput& input,
                                            std::mt19937& rng) const = 0;
};

// Alias de compatibilidad — ILanchesterModel = IStochasticModel
using ILanchesterModel = IStochasticModel;
