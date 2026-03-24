// test_main.cpp — Lanchester-CIO test suite entry point
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "lanchester_types.h"
#include "lanchester_model.h"
#include "lanchester_io.h"

#include <fstream>
#include <string>

// Global model params — required by current code (will be removed in Fase 1)
ModelParams g_model_params;

// Helper: path to test data files
static std::string test_data(const std::string& filename) {
    return std::string(TEST_DATA_DIR) + "/" + filename;
}

// ---------------------------------------------------------------------------
// Smoke test: verify the build works and core types are usable
// ---------------------------------------------------------------------------

TEST_CASE("Smoke test: CombatInput has sane defaults", "[smoke]") {
    CombatInput ci;
    REQUIRE(ci.distance_m == 2000);
    REQUIRE(ci.t_max == 30.0);
    REQUIRE(ci.h > 0);
    REQUIRE(ci.blue_engagement_fraction == 1.0);
    REQUIRE(ci.red_engagement_fraction == 1.0);
}

// ---------------------------------------------------------------------------
// ModelParams loading
// ---------------------------------------------------------------------------

TEST_CASE("Load model params from JSON", "[params]") {
    g_model_params = load_model_params(test_data("model_params.json"));

    REQUIRE(g_model_params.kill_probability_slope == 175.0);
    REQUIRE(g_model_params.terrain_fire_mult_facil == 1.0);
    REQUIRE(g_model_params.terrain_fire_mult_medio == 0.85);
    REQUIRE(g_model_params.terrain_fire_mult_dificil == 0.65);
    REQUIRE_FALSE(g_model_params.tactical_multipliers.empty());
}

// ---------------------------------------------------------------------------
// Vehicle catalog loading
// ---------------------------------------------------------------------------

TEST_CASE("Load vehicle catalogs", "[catalog]") {
    VehicleCatalog blue = load_catalog(test_data("vehicle_db.json"));
    VehicleCatalog red  = load_catalog(test_data("vehicle_db_en.json"));

    REQUIRE(blue.size() == 4);
    REQUIRE(red.size() == 4);
    REQUIRE(blue.count("LEOPARDO_2E") == 1);
    REQUIRE(red.count("T-80U") == 1);
}

// ---------------------------------------------------------------------------
// Symmetric combat: same forces should result in DRAW
// ---------------------------------------------------------------------------

TEST_CASE("Symmetric combat produces DRAW", "[simulation]") {
    g_model_params = load_model_params(test_data("model_params.json"));
    VehicleCatalog blue = load_catalog(test_data("vehicle_db.json"));
    VehicleCatalog red  = load_catalog(test_data("vehicle_db_en.json"));

    std::ifstream ifs(test_data("test_01_symmetric.json"));
    REQUIRE(ifs.is_open());
    auto scenario = nlohmann::json::parse(ifs);

    ScenarioOutput result = run_scenario(scenario, blue, red, AggregationMode::PRE);
    REQUIRE(result.combats.size() == 1);

    const auto& r = result.combats[0];
    // Symmetric forces: both should be near 0 or exact draw
    REQUIRE(r.blue_survivors < 1.0);
    REQUIRE(r.red_survivors < 1.0);
}

// ---------------------------------------------------------------------------
// Overwhelming force: 30 vs 5 should produce clear winner
// ---------------------------------------------------------------------------

TEST_CASE("Overwhelming force produces clear winner", "[simulation]") {
    g_model_params = load_model_params(test_data("model_params.json"));
    VehicleCatalog blue = load_catalog(test_data("vehicle_db.json"));
    VehicleCatalog red  = load_catalog(test_data("vehicle_db_en.json"));

    std::ifstream ifs(test_data("test_02_overwhelming.json"));
    REQUIRE(ifs.is_open());
    auto scenario = nlohmann::json::parse(ifs);

    ScenarioOutput result = run_scenario(scenario, blue, red, AggregationMode::PRE);
    REQUIRE(result.combats.size() == 1);

    const auto& r = result.combats[0];
    // Overwhelming blue force should win
    REQUIRE(r.outcome == Outcome::BLUE_WINS);
    REQUIRE(r.blue_survivors > 5.0);
}

// ---------------------------------------------------------------------------
// Monte Carlo: basic sanity check
// ---------------------------------------------------------------------------

TEST_CASE("Monte Carlo produces consistent statistics", "[montecarlo]") {
    g_model_params = load_model_params(test_data("model_params.json"));
    VehicleCatalog blue = load_catalog(test_data("vehicle_db.json"));
    VehicleCatalog red  = load_catalog(test_data("vehicle_db_en.json"));

    std::ifstream ifs(test_data("test_02_overwhelming.json"));
    REQUIRE(ifs.is_open());
    auto scenario = nlohmann::json::parse(ifs);

    auto mc = run_scenario_montecarlo(scenario, blue, red,
                                       AggregationMode::PRE, 100, 42);
    REQUIRE(mc.combats.size() == 1);

    const auto& m = mc.combats[0];
    // Blue should win majority of MC replicas
    REQUIRE(m.count_blue_wins > 70);
    // Mean survivors should be positive for blue
    REQUIRE(m.blue_survivors.mean > 0.0);
}
