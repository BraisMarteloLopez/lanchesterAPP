// test_vehicle_catalog.cpp — Tests unitarios para VehicleCatalogClass
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "vehicle_catalog.h"

static std::string test_data(const std::string& filename) {
    return std::string(TEST_DATA_DIR) + "/" + filename;
}

TEST_CASE("VehicleCatalogClass: load blue catalog", "[catalog]") {
    auto cat = VehicleCatalogClass::load(test_data("vehicle_db.json"));

    REQUIRE(cat.size() == 4);
    REQUIRE(cat.contains("LEOPARDO_2E"));
    REQUIRE(cat.contains("PIZARRO"));
    REQUIRE(cat.contains("TOA_SPIKE_I"));
    REQUIRE(cat.contains("VEC_25"));
    REQUIRE_FALSE(cat.contains("T-80U"));
}

TEST_CASE("VehicleCatalogClass: load red catalog", "[catalog]") {
    auto cat = VehicleCatalogClass::load(test_data("vehicle_db_en.json"));

    REQUIRE(cat.size() == 4);
    REQUIRE(cat.contains("T-80U"));
    REQUIRE(cat.contains("BMP-3"));
    REQUIRE(cat.contains("T-72B3"));
    REQUIRE(cat.contains("BTR-82A"));
}

TEST_CASE("VehicleCatalogClass: find existing vehicle", "[catalog]") {
    auto cat = VehicleCatalogClass::load(test_data("vehicle_db.json"));
    const auto& leo = cat.find("LEOPARDO_2E");

    REQUIRE(leo.name == "LEOPARDO_2E");
    REQUIRE(leo.D == 900);
    REQUIRE(leo.P == 850);
    REQUIRE(leo.CC == 0);
}

TEST_CASE("VehicleCatalogClass: find throws on missing vehicle", "[catalog]") {
    auto cat = VehicleCatalogClass::load(test_data("vehicle_db.json"));

    REQUIRE_THROWS_AS(cat.find("NONEXISTENT"), std::runtime_error);
}

TEST_CASE("VehicleCatalogClass: findInEither searches both catalogs", "[catalog]") {
    auto blue = VehicleCatalogClass::load(test_data("vehicle_db.json"));
    auto red  = VehicleCatalogClass::load(test_data("vehicle_db_en.json"));

    // Blue vehicle found in primary
    auto leo = VehicleCatalogClass::findInEither("LEOPARDO_2E", blue, red);
    REQUIRE(leo.name == "LEOPARDO_2E");

    // Red vehicle found in secondary
    auto t80 = VehicleCatalogClass::findInEither("T-80U", blue, red);
    REQUIRE(t80.name == "T-80U");

    // Not in either
    REQUIRE_THROWS_AS(
        VehicleCatalogClass::findInEither("NONEXISTENT", blue, red),
        std::runtime_error);
}

TEST_CASE("VehicleCatalogClass: names() returns sorted list", "[catalog]") {
    auto cat = VehicleCatalogClass::load(test_data("vehicle_db.json"));
    auto n = cat.names();

    REQUIRE(n.size() == 4);
    // std::map keys are sorted
    REQUIRE(n[0] == "LEOPARDO_2E");
    REQUIRE(n[1] == "PIZARRO");
    REQUIRE(n[2] == "TOA_SPIKE_I");
    REQUIRE(n[3] == "VEC_25");
}

TEST_CASE("VehicleCatalogClass: nonexistent file returns empty catalog", "[catalog]") {
    auto cat = VehicleCatalogClass::load("/nonexistent/path.json");

    REQUIRE(cat.size() == 0);
    REQUIRE(cat.names().empty());
}
