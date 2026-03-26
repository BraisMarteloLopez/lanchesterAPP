// test_square_law.cpp — Tests del modelo SquareLawModel
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "square_law_model.h"
#include "model_factory.h"
#include "montecarlo_runner.h"
#include "combat_utils.h"
#include "model_params.h"
#include "vehicle_catalog.h"

#include <cmath>
#include <memory>

using Catch::Matchers::WithinAbs;

static std::string test_data(const std::string& filename) {
    return std::string(TEST_DATA_DIR) + "/" + filename;
}

// Helper: build a CombatInput for a simple scenario (same as test_01_symmetric)
static CombatInput make_symmetric_input(const VehicleCatalogClass& blue_cat,
                                         const VehicleCatalogClass& red_cat) {
    CombatInput ci;
    ci.combat_id = 1;
    ci.distance_m = 2000;
    ci.terrain = Terrain::MEDIO;
    ci.h = 0.001666;
    ci.t_max = 60.0;
    ci.blue_state = "Ataque a posicion defensiva";
    ci.red_state = "Ataque a posicion defensiva";

    CompositionEntry blue_entry;
    blue_entry.vehicle = red_cat.find("T-80U");  // symmetric: both use T-80U
    blue_entry.count = 10;
    ci.blue_composition = {blue_entry};

    CompositionEntry red_entry;
    red_entry.vehicle = red_cat.find("T-80U");
    red_entry.count = 10;
    ci.red_composition = {red_entry};

    return ci;
}

TEST_CASE("SquareLawModel: symmetric combat produces draw", "[square_law]") {
    auto params = std::make_shared<ModelParamsClass>(
        ModelParamsClass::load(test_data("model_params.json")));
    SquareLawModel model(params);
    auto red_cat = VehicleCatalogClass::load(test_data("vehicle_db_en.json"));
    auto blue_cat = VehicleCatalogClass::load(test_data("vehicle_db.json"));

    auto input = make_symmetric_input(blue_cat, red_cat);
    auto result = model.simulate(input);

    REQUIRE(result.blue_survivors < 1.0);
    REQUIRE(result.red_survivors < 1.0);
}

TEST_CASE("SquareLawModel: overwhelming force produces correct result", "[square_law]") {
    auto params = std::make_shared<ModelParamsClass>(
        ModelParamsClass::load(test_data("model_params.json")));
    SquareLawModel model(params);
    auto blue_cat = VehicleCatalogClass::load(test_data("vehicle_db.json"));
    auto red_cat = VehicleCatalogClass::load(test_data("vehicle_db_en.json"));

    CombatInput ci;
    ci.combat_id = 1;
    ci.distance_m = 2000;
    ci.terrain = Terrain::MEDIO;
    ci.h = 1.0 / 600.0;
    ci.t_max = 30.0;
    ci.blue_state = "Ataque a posicion defensiva";
    ci.red_state  = "Ataque a posicion defensiva";

    CompositionEntry blue_e;
    blue_e.vehicle = blue_cat.find("LEOPARDO_2E");
    blue_e.count = 30;
    ci.blue_composition = {blue_e};

    CompositionEntry red_e;
    red_e.vehicle = red_cat.find("T-80U");
    red_e.count = 5;
    ci.red_composition = {red_e};

    auto result = model.simulate(ci);

    REQUIRE(result.outcome == Outcome::BLUE_WINS);
    REQUIRE(result.blue_survivors > 10.0);
    REQUIRE(result.red_survivors == 0.0);
    REQUIRE(result.duration_contact_minutes > 0.0);
    REQUIRE(result.static_advantage > 1.0);

    // Time series must be populated
    REQUIRE(result.time_series.size() > 2);
    // First point: t=0, initial forces
    REQUIRE(result.time_series.front().t == 0.0);
    REQUIRE(result.time_series.front().blue_forces == result.blue_initial);
    REQUIRE(result.time_series.front().red_forces == result.red_initial);
    // Last point: final state
    REQUIRE(result.time_series.back().blue_forces == result.blue_survivors);
    REQUIRE(result.time_series.back().red_forces == result.red_survivors);
    // Monotonically decreasing (at least one side)
    bool red_decreased = result.time_series.back().red_forces <
                         result.time_series.front().red_forces;
    REQUIRE(red_decreased);
}

TEST_CASE("SquareLawModel: Monte Carlo basic sanity", "[square_law][montecarlo]") {
    auto params = std::make_shared<ModelParamsClass>(
        ModelParamsClass::load(test_data("model_params.json")));
    SquareLawModel model(params);
    auto blue_cat = VehicleCatalogClass::load(test_data("vehicle_db.json"));
    auto red_cat = VehicleCatalogClass::load(test_data("vehicle_db_en.json"));

    // 20 LEOPARDO vs 10 T-80U — blue should win most
    CombatInput ci;
    ci.combat_id = 1;
    ci.distance_m = 2000;
    ci.terrain = Terrain::MEDIO;
    ci.blue_state = "Ataque a posicion defensiva";
    ci.red_state = "Ataque a posicion defensiva";

    CompositionEntry blue_e;
    blue_e.vehicle = blue_cat.find("LEOPARDO_2E");
    blue_e.count = 20;
    ci.blue_composition = {blue_e};

    CompositionEntry red_e;
    red_e.vehicle = red_cat.find("T-80U");
    red_e.count = 10;
    ci.red_composition = {red_e};

    std::mt19937 rng(42);
    auto mc = MonteCarloRunner::run(model, ci, 200, rng);

    REQUIRE(mc.n_replicas == 200);
    REQUIRE(mc.count_blue_wins > 100);  // blue should dominate
    REQUIRE(mc.blue_survivors.mean > 0.0);
    REQUIRE(mc.deterministic.outcome == Outcome::BLUE_WINS);
}

TEST_CASE("SquareLawModel: aggregate produces correct counts", "[square_law]") {
    auto blue_cat = VehicleCatalogClass::load(test_data("vehicle_db.json"));

    std::vector<CompositionEntry> comp;
    CompositionEntry e1;
    e1.vehicle = blue_cat.find("LEOPARDO_2E");
    e1.count = 5;
    comp.push_back(e1);

    CompositionEntry e2;
    e2.vehicle = blue_cat.find("PIZARRO");
    e2.count = 3;
    comp.push_back(e2);

    auto agg = lanchester::aggregate(comp);
    REQUIRE(agg.n_total == 8);
    REQUIRE(agg.n_cc == 3);  // PIZARRO has CC
    REQUIRE(agg.has_cc);
}

TEST_CASE("ModelFactory: creates registered model", "[factory]") {
    auto& factory = ModelFactory::instance();
    auto models = factory.availableModels();
    REQUIRE_FALSE(models.empty());
    REQUIRE(models[0] == "Lanchester Square Law (RK4)");

    auto params = std::make_shared<ModelParamsClass>(
        ModelParamsClass::load(test_data("model_params.json")));
    auto model = factory.create(models[0], params);
    REQUIRE(model->name() == "Lanchester Square Law (RK4)");

    // Verify it actually simulates correctly
    auto blue_cat = VehicleCatalogClass::load(test_data("vehicle_db.json"));
    CombatInput ci;
    ci.distance_m = 2000;
    ci.terrain = Terrain::MEDIO;
    ci.blue_state = lanchester::ATTACKING_STATE;
    ci.red_state  = lanchester::ATTACKING_STATE;
    CompositionEntry e;
    e.vehicle = blue_cat.find("LEOPARDO_2E");
    e.count = 20;
    ci.blue_composition = {e};
    e.count = 5;
    ci.red_composition = {e};

    auto result = model->simulate(ci);
    REQUIRE(result.outcome == Outcome::BLUE_WINS);
}
