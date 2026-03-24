// simulation_service.cpp — Implementacion de SimulationService
#include "simulation_service.h"
#include "lanchester_io.h"  // legacy functions for scenario orchestration

#include <future>

SimulationService::SimulationService(
    std::shared_ptr<ILanchesterModel> model,
    ModelParamsClass params,
    VehicleCatalogClass blueCat,
    VehicleCatalogClass redCat)
    : model_(std::move(model))
    , params_(std::move(params))
    , blue_cat_(std::move(blueCat))
    , red_cat_(std::move(redCat))
{
    // Set the legacy global so that run_scenario/run_scenario_montecarlo work.
    // This bridge will be removed when we fully migrate orchestration to OOP.
    params_.applyToGlobal();
}

ScenarioOutput SimulationService::runScenario(const ScenarioConfig& config) const {
    config.validate();
    auto json_scenario = config.toJson();
    return run_scenario(json_scenario, blue_cat_.raw(), red_cat_.raw(),
                        config.aggregation);
}

MonteCarloScenarioOutput SimulationService::runMonteCarlo(
    const ScenarioConfig& config, int replicas, uint64_t seed) const
{
    config.validate();
    auto json_scenario = config.toJson();
    return run_scenario_montecarlo(json_scenario, blue_cat_.raw(), red_cat_.raw(),
                                    config.aggregation, replicas, seed);
}

std::future<ScenarioOutput> SimulationService::runScenarioAsync(
    ScenarioConfig config) const
{
    // Capture everything by VALUE — no references to mutable external state
    auto blue_raw = blue_cat_.raw();
    auto red_raw = red_cat_.raw();
    auto agg = config.aggregation;

    return std::async(std::launch::async,
        [config = std::move(config), blue_raw = std::move(blue_raw),
         red_raw = std::move(red_raw), agg]() mutable {
            config.validate();
            auto json_scenario = config.toJson();
            return run_scenario(json_scenario, blue_raw, red_raw, agg);
        });
}

std::future<MonteCarloScenarioOutput> SimulationService::runMonteCarloAsync(
    ScenarioConfig config, int replicas, uint64_t seed) const
{
    auto blue_raw = blue_cat_.raw();
    auto red_raw = red_cat_.raw();
    auto agg = config.aggregation;

    return std::async(std::launch::async,
        [config = std::move(config), blue_raw = std::move(blue_raw),
         red_raw = std::move(red_raw), agg, replicas, seed]() mutable {
            config.validate();
            auto json_scenario = config.toJson();
            return run_scenario_montecarlo(json_scenario, blue_raw, red_raw,
                                            agg, replicas, seed);
        });
}
