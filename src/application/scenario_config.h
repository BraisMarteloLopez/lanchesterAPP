// scenario_config.h — Configuracion tipada de un escenario de simulacion
#pragma once

#include "../domain/lanchester_types.h"
#include "../domain/vehicle_catalog.h"
#include <string>
#include <vector>
#include <stdexcept>

// Configuracion de un bando
struct SideConfig {
    std::vector<CompositionEntry> composition;
    std::string tactical_state = lanchester::ATTACKING_STATE;
    Mobility mobility = Mobility::ALTA;
    double aft_pct = 0.0;
    double engagement_fraction = 2.0 / 3.0;
    double rate_factor = 1.0;
    double count_factor = 1.0;
};

// Configuracion completa de un escenario (un solo combate)
struct ScenarioConfig {
    std::string scenario_id = "SCENARIO";
    Terrain terrain = Terrain::MEDIO;
    double distance_m = 2000.0;
    double t_max = 30.0;
    double h = lanchester::DEFAULT_TIMESTEP;
    AggregationMode aggregation = AggregationMode::PRE;
    SideConfig blue;
    SideConfig red;

    // Validacion
    void validate() const;
};
