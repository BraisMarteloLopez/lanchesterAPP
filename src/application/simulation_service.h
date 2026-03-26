// simulation_service.h — Servicio de simulacion (orquestacion)
#pragma once

#include "../domain/lanchester_model_iface.h"
#include "../domain/model_params_iface.h"
#include "../domain/vehicle_catalog_iface.h"
#include "scenario_config.h"

#include <future>
#include <memory>

class SimulationService {
public:
    SimulationService(std::shared_ptr<ILanchesterModel> model,
                      std::shared_ptr<const IModelParams> params,
                      std::shared_ptr<const IVehicleCatalog> blueCat,
                      std::shared_ptr<const IVehicleCatalog> redCat);

    // --- Ejecucion sincrona ---
    ScenarioOutput runScenario(const ScenarioConfig& config) const;
    MonteCarloScenarioOutput runMonteCarlo(const ScenarioConfig& config,
                                           int replicas, uint64_t seed) const;

    // --- Ejecucion asincrona (copia por valor, sin race conditions) ---
    std::future<ScenarioOutput> runScenarioAsync(ScenarioConfig config) const;
    std::future<MonteCarloScenarioOutput> runMonteCarloAsync(
        ScenarioConfig config, int replicas, uint64_t seed) const;

    // --- Acceso a datos (read-only, por interfaz) ---
    const IVehicleCatalog& blueCatalog() const { return *blue_cat_; }
    const IVehicleCatalog& redCatalog() const { return *red_cat_; }
    const IModelParams& modelParams() const { return *params_; }
    const ILanchesterModel& model() const { return *model_; }

private:
    CombatInput buildCombatInput(const ScenarioConfig& config) const;

    std::shared_ptr<ILanchesterModel> model_;
    std::shared_ptr<const IModelParams> params_;
    std::shared_ptr<const IVehicleCatalog> blue_cat_;
    std::shared_ptr<const IVehicleCatalog> red_cat_;
};
