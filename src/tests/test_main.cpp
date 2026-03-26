// test_main.cpp — Lanchester-CIO test suite entry point
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "lanchester_types.h"
#include "model_params.h"
#include "vehicle_catalog.h"
#include "square_law_model.h"

#include <string>

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
// ModelParams loading (OOP)
// ---------------------------------------------------------------------------

TEST_CASE("Load model params from JSON", "[params]") {
    auto mp = ModelParamsClass::load(test_data("model_params.json"));

    REQUIRE(mp.killProbabilitySlope() == 175.0);
    REQUIRE(mp.terrainFireMult(Terrain::FACIL) == 1.0);
    REQUIRE(mp.terrainFireMult(Terrain::MEDIO) == 0.85);
    REQUIRE(mp.terrainFireMult(Terrain::DIFICIL) == 0.65);
    auto tm = mp.tacticalMult("Ataque a posicion defensiva");
    REQUIRE(tm.self_mult == 1.0);
}

// ---------------------------------------------------------------------------
// Vehicle catalog loading (OOP)
// ---------------------------------------------------------------------------

TEST_CASE("Load vehicle catalogs", "[catalog]") {
    auto blue = VehicleCatalogClass::load(test_data("vehicle_db.json"));
    auto red  = VehicleCatalogClass::load(test_data("vehicle_db_en.json"));

    REQUIRE(blue.size() == 4);
    REQUIRE(red.size() == 4);
    REQUIRE(blue.contains("LEOPARDO_2E"));
    REQUIRE(red.contains("T-80U"));
}

// ---------------------------------------------------------------------------
// Symmetric combat: same forces should result in DRAW
// ---------------------------------------------------------------------------

TEST_CASE("Symmetric combat produces DRAW", "[simulation]") {
    auto params = ModelParamsClass::load(test_data("model_params.json"));
    auto red_cat = VehicleCatalogClass::load(test_data("vehicle_db_en.json"));
    SquareLawModel model(params);

    CombatInput ci;
    ci.combat_id = 1;
    ci.distance_m = 2000;
    ci.terrain = Terrain::MEDIO;
    ci.h = 1.0 / 600.0;
    ci.t_max = 60.0;
    ci.blue_state = "Ataque a posicion defensiva";
    ci.red_state  = "Ataque a posicion defensiva";

    CompositionEntry entry;
    entry.vehicle = red_cat.find("T-80U");
    entry.count = 10;
    ci.blue_composition = {entry};
    ci.red_composition  = {entry};

    auto result = model.simulate(ci);

    REQUIRE(result.blue_survivors < 1.0);
    REQUIRE(result.red_survivors < 1.0);
}

// ---------------------------------------------------------------------------
// Overwhelming force: 30 vs 5 should produce clear winner
// ---------------------------------------------------------------------------

TEST_CASE("Overwhelming force produces clear winner", "[simulation]") {
    auto params = ModelParamsClass::load(test_data("model_params.json"));
    auto blue_cat = VehicleCatalogClass::load(test_data("vehicle_db.json"));
    auto red_cat  = VehicleCatalogClass::load(test_data("vehicle_db_en.json"));
    SquareLawModel model(params);

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
    REQUIRE(result.blue_survivors > 5.0);
}

// ---------------------------------------------------------------------------
// Monte Carlo: basic sanity check
// ---------------------------------------------------------------------------

TEST_CASE("Monte Carlo produces consistent statistics", "[montecarlo]") {
    auto params = ModelParamsClass::load(test_data("model_params.json"));
    auto blue_cat = VehicleCatalogClass::load(test_data("vehicle_db.json"));
    auto red_cat  = VehicleCatalogClass::load(test_data("vehicle_db_en.json"));
    SquareLawModel model(params);

    CombatInput ci;
    ci.combat_id = 1;
    ci.distance_m = 2000;
    ci.terrain = Terrain::MEDIO;
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

    std::mt19937 rng(42);
    auto mc = model.runMonteCarlo(ci, 100, rng);

    REQUIRE(mc.count_blue_wins > 70);
    REQUIRE(mc.blue_survivors.mean > 0.0);
}
