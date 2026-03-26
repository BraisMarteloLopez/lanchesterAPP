// test_model_params.cpp — Tests unitarios para ModelParamsClass
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "model_params.h"

using Catch::Matchers::WithinAbs;

static std::string test_data(const std::string& filename) {
    return std::string(TEST_DATA_DIR) + "/" + filename;
}

TEST_CASE("ModelParamsClass: load from valid JSON", "[model_params]") {
    auto mp = ModelParamsClass::load(test_data("model_params.json"));

    REQUIRE(mp.killProbabilitySlope() == 175.0);
    REQUIRE_THAT(mp.distCoeffs().c_dk, WithinAbs(-0.188, 1e-6));
    REQUIRE_THAT(mp.distCoeffs().c_f, WithinAbs(-0.865, 1e-6));
    REQUIRE_THAT(mp.distCoeffs().c_const, WithinAbs(1.295, 1e-6));
}

TEST_CASE("ModelParamsClass: terrain fire multipliers", "[model_params]") {
    auto mp = ModelParamsClass::load(test_data("model_params.json"));

    REQUIRE(mp.terrainFireMult(Terrain::FACIL) == 1.0);
    REQUIRE(mp.terrainFireMult(Terrain::MEDIO) == 0.85);
    REQUIRE(mp.terrainFireMult(Terrain::DIFICIL) == 0.65);
}

TEST_CASE("ModelParamsClass: tactical multipliers loaded", "[model_params]") {
    auto mp = ModelParamsClass::load(test_data("model_params.json"));

    auto tm = mp.tacticalMult("Ataque a posicion defensiva");
    REQUIRE(tm.self_mult == 1.0);
    REQUIRE(tm.opponent_mult == 1.0);

    auto tm_def = mp.tacticalMult("Defensiva organizacion media");
    REQUIRE(tm_def.self_mult == 1.0);
    REQUIRE_THAT(tm_def.opponent_mult, WithinAbs(1.0 / (4.25 * 4.25), 1e-6));
}

TEST_CASE("ModelParamsClass: nonexistent file returns defaults", "[model_params]") {
    auto mp = ModelParamsClass::load("/nonexistent/path.json");

    REQUIRE(mp.killProbabilitySlope() == 175.0);
    REQUIRE(mp.terrainFireMult(Terrain::FACIL) == 1.0);
}

