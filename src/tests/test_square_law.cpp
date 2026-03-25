// test_square_law.cpp — Tests del modelo SquareLawModel vs baseline legacy
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "square_law_model.h"
#include "model_params.h"
#include "vehicle_catalog.h"
#include "lanchester_io.h"

#include <fstream>
#include <cmath>

using Catch::Matchers::WithinAbs;

// g_model_params defined in test_main.cpp

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
    auto params = ModelParamsClass::load(test_data("model_params.json"));
    SquareLawModel model(params);
    auto red_cat = VehicleCatalogClass::load(test_data("vehicle_db_en.json"));
    auto blue_cat = VehicleCatalogClass::load(test_data("vehicle_db.json"));

    auto input = make_symmetric_input(blue_cat, red_cat);
    auto result = model.simulate(input);

    REQUIRE(result.blue_survivors < 1.0);
    REQUIRE(result.red_survivors < 1.0);
}

TEST_CASE("SquareLawModel: results match legacy code", "[square_law]") {
    auto params = ModelParamsClass::load(test_data("model_params.json"));
    params.applyToGlobal();  // set legacy global for comparison

    SquareLawModel model(params);
    auto blue_cat = VehicleCatalogClass::load(test_data("vehicle_db.json"));
    auto red_cat = VehicleCatalogClass::load(test_data("vehicle_db_en.json"));

    // Load test_02_overwhelming via legacy path
    std::ifstream ifs(test_data("test_02_overwhelming.json"));
    REQUIRE(ifs.is_open());
    auto scenario = nlohmann::json::parse(ifs);

    ScenarioOutput legacy_result = run_scenario(scenario, blue_cat.raw(), red_cat.raw(),
                                                 AggregationMode::PRE);
    REQUIRE(legacy_result.combats.size() == 1);
    const auto& lr = legacy_result.combats[0];

    // Now run the same via SquareLawModel
    // Build equivalent CombatInput from the scenario
    CombatInput ci;
    ci.combat_id = 1;
    ci.distance_m = scenario["engagement_distance_m"].get<double>();
    ci.terrain = parse_terrain(scenario["terrain"].get<std::string>());
    ci.h = scenario["solver"]["h"].get<double>();
    ci.t_max = scenario["solver"]["t_max_minutes"].get<double>();

    const auto& combat = scenario["combat_sequence"][0];
    ci.blue_state = combat["blue"]["tactical_state"].get<std::string>();
    ci.red_state  = combat["red"]["tactical_state"].get<std::string>();

    for (const auto& item : combat["blue"]["composition"]) {
        CompositionEntry ce;
        ce.vehicle = VehicleCatalogClass::findInEither(
            item["vehicle"].get<std::string>(), blue_cat, red_cat);
        ce.count = item["count"].get<int>();
        ci.blue_composition.push_back(ce);
    }
    for (const auto& item : combat["red"]["composition"]) {
        CompositionEntry ce;
        ce.vehicle = VehicleCatalogClass::findInEither(
            item["vehicle"].get<std::string>(), blue_cat, red_cat);
        ce.count = item["count"].get<int>();
        ci.red_composition.push_back(ce);
    }

    ci.blue_aft_pct = combat["blue"].value("aft_casualties_pct", 0.0);
    ci.red_aft_pct = combat["red"].value("aft_casualties_pct", 0.0);
    ci.blue_engagement_fraction = combat["blue"].value("engagement_fraction", 1.0);
    ci.red_engagement_fraction = combat["red"].value("engagement_fraction", 1.0);
    ci.blue_rate_factor = combat["blue"].value("rate_factor", 1.0);
    ci.red_rate_factor = combat["red"].value("rate_factor", 1.0);
    ci.blue_count_factor = combat["blue"].value("count_factor", 1.0);
    ci.red_count_factor = combat["red"].value("count_factor", 1.0);

    auto new_result = model.simulate(ci);

    // Results must match exactly (same algorithm, same params)
    REQUIRE_THAT(new_result.blue_survivors, WithinAbs(lr.blue_survivors, 0.01));
    REQUIRE_THAT(new_result.red_survivors, WithinAbs(lr.red_survivors, 0.01));
    REQUIRE_THAT(new_result.duration_contact_minutes,
                 WithinAbs(lr.duration_contact_minutes, 0.01));
    REQUIRE(new_result.outcome == lr.outcome);
}

TEST_CASE("SquareLawModel: Monte Carlo basic sanity", "[square_law][montecarlo]") {
    auto params = ModelParamsClass::load(test_data("model_params.json"));
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
    auto mc = model.runMonteCarlo(ci, 200, rng);

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

    auto agg = SquareLawModel::aggregate(comp);
    REQUIRE(agg.n_total == 8);
    REQUIRE(agg.n_cc == 3);  // PIZARRO has CC
    REQUIRE(agg.has_cc);
}
