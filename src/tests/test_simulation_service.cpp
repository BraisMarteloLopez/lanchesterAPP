// test_simulation_service.cpp — Tests de integracion del SimulationService
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "simulation_service.h"
#include "square_law_model.h"
#include "model_params.h"
#include "vehicle_catalog.h"

#include <memory>

using Catch::Matchers::WithinAbs;

static std::string test_data(const std::string& filename) {
    return std::string(TEST_DATA_DIR) + "/" + filename;
}

static SimulationService make_service() {
    auto params = std::make_shared<ModelParamsClass>(
        ModelParamsClass::load(test_data("model_params.json")));
    auto blue_cat = std::make_shared<VehicleCatalogClass>(
        VehicleCatalogClass::load(test_data("vehicle_db.json")));
    auto red_cat = std::make_shared<VehicleCatalogClass>(
        VehicleCatalogClass::load(test_data("vehicle_db_en.json")));
    auto model = std::make_shared<SquareLawModel>(params);
    return SimulationService(model, params, blue_cat, red_cat);
}

static ScenarioConfig make_overwhelming_config(const SimulationService& svc) {
    ScenarioConfig cfg;
    cfg.scenario_id = "TEST-OVERWHELMING";
    cfg.terrain = Terrain::MEDIO;
    cfg.distance_m = 2000;
    cfg.h = 0.001666;
    cfg.t_max = 60.0;

    CompositionEntry blue_e;
    blue_e.vehicle = svc.blueCatalog().find("LEOPARDO_2E");
    blue_e.count = 30;
    cfg.blue.composition = {blue_e};
    cfg.blue.tactical_state = "Ataque a posicion defensiva";

    CompositionEntry red_e;
    red_e.vehicle = svc.redCatalog().find("T-80U");
    red_e.count = 5;
    cfg.red.composition = {red_e};
    cfg.red.tactical_state = "Ataque a posicion defensiva";

    return cfg;
}

TEST_CASE("SimulationService: runScenario produces valid result", "[service]") {
    auto svc = make_service();
    auto cfg = make_overwhelming_config(svc);

    auto result = svc.runScenario(cfg);
    REQUIRE(result.combats.size() == 1);
    REQUIRE(result.combats[0].outcome == Outcome::BLUE_WINS);
    REQUIRE(result.combats[0].blue_survivors > 10.0);
}

TEST_CASE("SimulationService: runMonteCarlo produces statistics", "[service]") {
    auto svc = make_service();
    auto cfg = make_overwhelming_config(svc);

    auto mc = svc.runMonteCarlo(cfg, 100, 42);
    REQUIRE(mc.combats.size() == 1);
    REQUIRE(mc.combats[0].count_blue_wins > 80);
    REQUIRE(mc.combats[0].blue_survivors.mean > 5.0);
}

TEST_CASE("SimulationService: runScenarioAsync completes correctly", "[service]") {
    auto svc = make_service();
    auto cfg = make_overwhelming_config(svc);

    auto future = svc.runScenarioAsync(std::move(cfg));
    auto result = future.get();

    REQUIRE(result.combats.size() == 1);
    REQUIRE(result.combats[0].outcome == Outcome::BLUE_WINS);
}

TEST_CASE("SimulationService: validation rejects invalid config", "[service]") {
    auto svc = make_service();
    ScenarioConfig cfg;
    // Empty composition should fail
    REQUIRE_THROWS_AS(svc.runScenario(cfg), std::runtime_error);
}

TEST_CASE("ScenarioConfig: validate catches bad params", "[config]") {
    ScenarioConfig cfg;
    cfg.blue.composition = {{VehicleParams{}, 1}};
    cfg.red.composition = {{VehicleParams{}, 1}};

    // Valid default
    REQUIRE_NOTHROW(cfg.validate());

    // Bad engagement fraction
    cfg.blue.engagement_fraction = 2.0;
    REQUIRE_THROWS_AS(cfg.validate(), std::runtime_error);
    cfg.blue.engagement_fraction = 1.0;

    // Bad h
    cfg.h = 0.0;
    REQUIRE_THROWS_AS(cfg.validate(), std::runtime_error);
    cfg.h = 0.001;

    // Empty composition
    cfg.blue.composition.clear();
    REQUIRE_THROWS_AS(cfg.validate(), std::runtime_error);
}

