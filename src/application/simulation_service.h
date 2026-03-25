// simulation_service.h — Servicio de simulacion (orquestacion)
#pragma once

#include "../domain/lanchester_model_iface.h"
#include "../domain/model_params.h"
#include "../domain/vehicle_catalog.h"
#include "scenario_config.h"

#include <future>
#include <memory>

class SimulationService {
public:
    SimulationService(std::shared_ptr<ILanchesterModel> model,
                      ModelParamsClass params,
                      VehicleCatalogClass blueCat,
                      VehicleCatalogClass redCat);

    // --- Ejecucion sincrona ---
    ScenarioOutput runScenario(const ScenarioConfig& config) const;
    MonteCarloScenarioOutput runMonteCarlo(const ScenarioConfig& config,
                                           int replicas, uint64_t seed) const;

    // --- Ejecucion asincrona (copia por valor, sin race conditions) ---
    std::future<ScenarioOutput> runScenarioAsync(ScenarioConfig config) const;
    std::future<MonteCarloScenarioOutput> runMonteCarloAsync(
        ScenarioConfig config, int replicas, uint64_t seed) const;

    // --- Acceso a datos (read-only) ---
    const VehicleCatalogClass& blueCatalog() const { return blue_cat_; }
    const VehicleCatalogClass& redCatalog() const { return red_cat_; }
    const ModelParamsClass& modelParams() const { return params_; }
    const ILanchesterModel& model() const { return *model_; }

private:
    std::shared_ptr<ILanchesterModel> model_;
    ModelParamsClass params_;
    VehicleCatalogClass blue_cat_;
    VehicleCatalogClass red_cat_;
};
