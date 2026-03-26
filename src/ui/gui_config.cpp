// gui_config.cpp — Implementacion de GuiConfig
#include "gui_config.h"
#include "nlohmann/json.hpp"

#include <fstream>

GuiConfig GuiConfig::load(const std::string& path) {
    GuiConfig cfg;
    std::ifstream ifs(path);
    if (!ifs.is_open()) return cfg;

    try {
        auto j = nlohmann::json::parse(ifs);
        if (j.contains("animation_speed_ms_per_step"))
            cfg.animation_speed_ms_per_step = j["animation_speed_ms_per_step"].get<int>();
    } catch (...) {}

    return cfg;
}
