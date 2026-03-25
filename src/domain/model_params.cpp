// model_params.cpp — Implementacion de ModelParamsClass
#include "model_params.h"
#include "nlohmann/json.hpp"

#include <cstdio>
#include <fstream>

using json = nlohmann::json;

// Helper: lee un parametro que puede ser escalar o {value: X, ...}
static double read_param(const json& j, const std::string& key, double default_val) {
    if (!j.contains(key)) return default_val;
    const auto& v = j[key];
    if (v.is_object() && v.contains("value")) return v["value"].get<double>();
    if (v.is_number()) return v.get<double>();
    return default_val;
}

ModelParamsClass ModelParamsClass::load(const std::string& path) {
    ModelParamsClass mp;

    std::ifstream ifs(path);
    if (!ifs.is_open()) return mp;

    try {
        json j = json::parse(ifs);

        mp.data_.kill_probability_slope = read_param(j, "kill_probability_slope", 175.0);

        if (j.contains("distance_degradation_coefficients")) {
            const auto& d = j["distance_degradation_coefficients"];
            mp.data_.dist_coeff.c_dk    = read_param(d, "c_dk",    mp.data_.dist_coeff.c_dk);
            mp.data_.dist_coeff.c_f     = read_param(d, "c_f",     mp.data_.dist_coeff.c_f);
            mp.data_.dist_coeff.c_dk2   = read_param(d, "c_dk2",   mp.data_.dist_coeff.c_dk2);
            mp.data_.dist_coeff.c_dk_f  = read_param(d, "c_dk_f",  mp.data_.dist_coeff.c_dk_f);
            mp.data_.dist_coeff.c_f2    = read_param(d, "c_f2",    mp.data_.dist_coeff.c_f2);
            mp.data_.dist_coeff.c_const = read_param(d, "c_const", mp.data_.dist_coeff.c_const);
        }

        if (j.contains("terrain_fire_effectiveness")) {
            const auto& t = j["terrain_fire_effectiveness"];
            mp.data_.terrain_fire_mult_facil   = read_param(t, "FACIL",   mp.data_.terrain_fire_mult_facil);
            mp.data_.terrain_fire_mult_medio   = read_param(t, "MEDIO",   mp.data_.terrain_fire_mult_medio);
            mp.data_.terrain_fire_mult_dificil = read_param(t, "DIFICIL", mp.data_.terrain_fire_mult_dificil);
        }

        if (j.contains("tactical_multipliers")) {
            const auto& tm = j["tactical_multipliers"];
            for (auto it = tm.begin(); it != tm.end(); ++it) {
                if (it.key().front() == '_' || it.key() == "origin" ||
                    it.key() == "calibration_status")
                    continue;
                if (!it.value().is_object()) continue;

                TacticalMultDef tmd;
                tmd.self_mult = it.value().value("self", 1.0);
                if (it.value().contains("opponent")) {
                    tmd.opponent_mult = it.value()["opponent"].get<double>();
                } else if (it.value().contains("opponent_base")) {
                    double base = it.value()["opponent_base"].get<double>();
                    tmd.opponent_mult = 1.0 / (base * base);
                }
                mp.data_.tactical_multipliers[it.key()] = tmd;
            }
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Aviso: error leyendo model_params.json: %s. Usando defaults.\n",
                     e.what());
    }

    return mp;
}

double ModelParamsClass::terrainFireMult(Terrain t) const {
    switch (t) {
        case Terrain::FACIL:   return data_.terrain_fire_mult_facil;
        case Terrain::MEDIO:   return data_.terrain_fire_mult_medio;
        case Terrain::DIFICIL: return data_.terrain_fire_mult_dificil;
    }
    return 1.0;
}

TacticalMult ModelParamsClass::tacticalMult(const std::string& state) const {
    const auto& tm = data_.tactical_multipliers;
    auto it = tm.find(state);
    if (it != tm.end())
        return {it->second.self_mult, it->second.opponent_mult};

    // Fallback hardcoded
    if (state == "Ataque a posicion defensiva")    return {1.0, 1.0};
    if (state == "Busqueda del contacto")          return {0.9, 1.0};
    if (state == "En posicion de tiro")            return {1.0, 0.9};
    if (state == "Defensiva condiciones minimas")  return {1.0, 1.0 / (2.25 * 2.25)};
    if (state == "Defensiva organizacion ligera")  return {1.0, 1.0 / (2.75 * 2.75)};
    if (state == "Defensiva organizacion media")   return {1.0, 1.0 / (4.25 * 4.25)};
    if (state == "Retardo")                        return {1.0, 1.0 / (6.0 * 6.0)};
    if (state == "Retrocede")                      return {0.9, 1.0};
    return {1.0, 1.0};
}

void ModelParamsClass::applyToGlobal() const {
    g_model_params = data_;
}
