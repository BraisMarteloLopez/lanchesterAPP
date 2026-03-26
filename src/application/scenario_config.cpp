// scenario_config.cpp — Implementacion de ScenarioConfig
#include "scenario_config.h"

void ScenarioConfig::validate() const {
    if (h <= 0.0)
        throw std::runtime_error("h debe ser > 0");
    if (t_max <= 0.0)
        throw std::runtime_error("t_max debe ser > 0");
    if (distance_m < 0.0)
        throw std::runtime_error("distance_m debe ser >= 0");

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
