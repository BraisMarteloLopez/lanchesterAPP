// scenario_config.cpp — Implementacion de ScenarioConfig
#include "scenario_config.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

static const char* terrain_to_str(Terrain t) {
    switch (t) {
        case Terrain::FACIL:   return "FACIL";
        case Terrain::MEDIO:   return "MEDIO";
        case Terrain::DIFICIL: return "DIFICIL";
    }
    return "MEDIO";
}

static const char* mobility_to_str(Mobility m) {
    switch (m) {
        case Mobility::MUY_ALTA: return "MUY_ALTA";
        case Mobility::ALTA:     return "ALTA";
        case Mobility::MEDIA:    return "MEDIA";
        case Mobility::BAJA:     return "BAJA";
    }
    return "ALTA";
}

void ScenarioConfig::validate() const {
    // Solver params
    if (h <= 0.0)
        throw std::runtime_error("h debe ser > 0");
    if (t_max <= 0.0)
        throw std::runtime_error("t_max debe ser > 0");
    if (distance_m < 0.0)
        throw std::runtime_error("distance_m debe ser >= 0");

    // Side validation
    auto validate_side = [](const SideConfig& side, const std::string& label) {
        if (side.composition.empty())
            throw std::runtime_error(label + ": composicion vacia");
        if (side.aft_pct < 0.0 || side.aft_pct > 1.0)
            throw std::runtime_error(label + ".aft_pct fuera de [0, 1]");
        if (side.engagement_fraction < 0.0 || side.engagement_fraction > 1.0)
            throw std::runtime_error(label + ".engagement_fraction fuera de [0, 1]");
        if (side.rate_factor < 0.0 || side.rate_factor > 10.0)
            throw std::runtime_error(label + ".rate_factor fuera de [0, 10]");
        if (side.count_factor < 0.0 || side.count_factor > 10.0)
            throw std::runtime_error(label + ".count_factor fuera de [0, 10]");
        for (const auto& ce : side.composition) {
            if (ce.count < 1)
                throw std::runtime_error(label + ": count < 1 para " + ce.vehicle.name);
        }
    };

    validate_side(blue, "blue");
    validate_side(red, "red");
}

json ScenarioConfig::toJson() const {
    json scenario;
    scenario["scenario_id"] = scenario_id;
    scenario["terrain"] = terrain_to_str(terrain);
    scenario["engagement_distance_m"] = distance_m;
    scenario["solver"] = {{"h", h}, {"t_max_minutes", t_max}};

    auto build_side = [](const SideConfig& side) -> json {
        json s;
        s["tactical_state"] = side.tactical_state;
        s["mobility"] = mobility_to_str(side.mobility);
        s["aft_received"] = (side.aft_pct > 0) ? 1 : 0;
        s["aft_casualties_pct"] = side.aft_pct;
        s["engagement_fraction"] = side.engagement_fraction;
        s["rate_factor"] = side.rate_factor;
        s["count_factor"] = side.count_factor;

        json comp = json::array();
        for (const auto& ce : side.composition) {
            comp.push_back({{"vehicle", ce.vehicle.name}, {"count", ce.count}});
        }
        s["composition"] = comp;
        return s;
    };

    json combat;
    combat["combat_id"] = 1;
    combat["blue"] = build_side(blue);
    combat["red"] = build_side(red);
    combat["reinforcements_blue"] = json::array();
    combat["reinforcements_red"] = json::array();

    scenario["combat_sequence"] = json::array({combat});
    return scenario;
}
