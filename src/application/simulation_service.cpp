// simulation_service.cpp — Implementacion de SimulationService
#include "simulation_service.h"

#include <future>
#include <random>

// ---------------------------------------------------------------------------
// Helper: construir CombatInput desde ScenarioConfig (sin JSON intermedio)
// ---------------------------------------------------------------------------

static CombatInput buildCombatInput(const ScenarioConfig& config) {
    CombatInput input;
    input.combat_id = 1;
    input.blue_composition = config.blue.composition;
    input.red_composition  = config.red.composition;
    input.blue_state = config.blue.tactical_state;
    input.red_state  = config.red.tactical_state;
    input.blue_aft_pct = config.blue.aft_pct;
    input.red_aft_pct  = config.red.aft_pct;
    input.blue_engagement_fraction = config.blue.engagement_fraction;
    input.red_engagement_fraction  = config.red.engagement_fraction;
    input.blue_rate_factor  = config.blue.rate_factor;
    input.red_rate_factor   = config.red.rate_factor;
    input.blue_count_factor = config.blue.count_factor;
    input.red_count_factor  = config.red.count_factor;
    input.distance_m = config.distance_m;
    input.h     = config.h;
    input.t_max = config.t_max;
    input.aggregation_mode = config.aggregation;
    input.terrain = config.terrain;

    // Velocidad de aproximacion: si un bando ataca, se acerca
    bool blue_attacks = (config.blue.tactical_state == "Ataque a posicion defensiva");
    bool red_attacks  = (config.red.tactical_state  == "Ataque a posicion defensiva");
    double v = 0;
    if (blue_attacks) v += tactical_speed(config.blue.mobility, config.terrain);
    if (red_attacks)  v += tactical_speed(config.red.mobility, config.terrain);
    input.approach_speed_kmh = v;

    return input;
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

SimulationService::SimulationService(
    std::shared_ptr<ILanchesterModel> model,
    ModelParamsClass params,
    VehicleCatalogClass blueCat,
    VehicleCatalogClass redCat)
    : model_(std::move(model))
    , params_(std::move(params))
    , blue_cat_(std::move(blueCat))
    , red_cat_(std::move(redCat))
{}

// ---------------------------------------------------------------------------
// Ejecucion sincrona
// ---------------------------------------------------------------------------

ScenarioOutput SimulationService::runScenario(const ScenarioConfig& config) const {
    config.validate();
    CombatInput input = buildCombatInput(config);
    CombatResult result = model_->simulate(input);

    ScenarioOutput out;
    out.scenario_id = config.scenario_id;
    out.combats.push_back(result);
    return out;
}

MonteCarloScenarioOutput SimulationService::runMonteCarlo(
    const ScenarioConfig& config, int replicas, uint64_t seed) const
{
    config.validate();
    CombatInput input = buildCombatInput(config);
    std::mt19937 rng(seed);
    MonteCarloResult mc = model_->runMonteCarlo(input, replicas, rng);

    MonteCarloScenarioOutput out;
    out.scenario_id = config.scenario_id;
    out.n_replicas = replicas;
    out.seed = seed;
    out.combats.push_back(mc);
    return out;
}

// ---------------------------------------------------------------------------
// Ejecucion asincrona (captura por valor — sin race conditions)
// ---------------------------------------------------------------------------

std::future<ScenarioOutput> SimulationService::runScenarioAsync(
    ScenarioConfig config) const
{
    auto model = model_;
    return std::async(std::launch::async,
        [config = std::move(config), model]() {
            config.validate();
            CombatInput input = buildCombatInput(config);
            CombatResult result = model->simulate(input);
            ScenarioOutput out;
            out.scenario_id = config.scenario_id;
            out.combats.push_back(result);
            return out;
        });
}

std::future<MonteCarloScenarioOutput> SimulationService::runMonteCarloAsync(
    ScenarioConfig config, int replicas, uint64_t seed) const
{
    auto model = model_;
    return std::async(std::launch::async,
        [config = std::move(config), model, replicas, seed]() {
            config.validate();
            CombatInput input = buildCombatInput(config);
            std::mt19937 rng(seed);
            MonteCarloResult mc = model->runMonteCarlo(input, replicas, rng);
            MonteCarloScenarioOutput out;
            out.scenario_id = config.scenario_id;
            out.n_replicas = replicas;
            out.seed = seed;
            out.combats.push_back(mc);
            return out;
        });
}
