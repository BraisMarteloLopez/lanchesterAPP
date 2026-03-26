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

// Helper: string a enum Terrain
static Terrain parse_terrain(const std::string& s) {
    if (s == "FACIL")   return Terrain::FACIL;
    if (s == "DIFICIL") return Terrain::DIFICIL;
    return Terrain::MEDIO;
}

// Helper: string a enum Mobility
static Mobility parse_mobility(const std::string& s) {
    if (s == "MUY_ALTA") return Mobility::MUY_ALTA;
    if (s == "ALTA")     return Mobility::ALTA;
    if (s == "MEDIA")    return Mobility::MEDIA;
    return Mobility::BAJA;
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

        // Terrain fire multipliers — data-driven, itera sobre el JSON
        if (j.contains("terrain_fire_effectiveness")) {
            const auto& t = j["terrain_fire_effectiveness"];
            for (auto it = t.begin(); it != t.end(); ++it) {
                if (it.key().front() == '_' || it.key() == "origin" ||
                    it.key() == "calibration_status")
                    continue;
                Terrain ter = parse_terrain(it.key());
                double val = 1.0;
                if (it.value().is_object() && it.value().contains("value"))
                    val = it.value()["value"].get<double>();
                else if (it.value().is_number())
                    val = it.value().get<double>();
                mp.data_.terrain_fire_mults[ter] = val;
            }
        }

        // Tactical multipliers
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

        // Tactical speeds — data-driven
        if (j.contains("tactical_speeds")) {
            mp.data_.tactical_speeds.clear();
            const auto& ts = j["tactical_speeds"];
            for (auto it = ts.begin(); it != ts.end(); ++it) {
                if (!it.value().is_object()) continue;
                Mobility mob = parse_mobility(it.key());
                for (auto jt = it.value().begin(); jt != it.value().end(); ++jt) {
                    if (jt.value().is_number()) {
                        Terrain ter = parse_terrain(jt.key());
                        mp.data_.tactical_speeds[mob][ter] = jt.value().get<double>();
                    }
                }
            }
        }

    } catch (const std::exception& e) {
        std::fprintf(stderr, "Aviso: error leyendo model_params.json: %s. Usando defaults.\n",
                     e.what());
    }

    return mp;
}

double ModelParamsClass::terrainFireMult(Terrain t) const {
    auto it = data_.terrain_fire_mults.find(t);
    return (it != data_.terrain_fire_mults.end()) ? it->second : 1.0;
}

TacticalMult ModelParamsClass::tacticalMult(const std::string& state) const {
    auto it = data_.tactical_multipliers.find(state);
    if (it != data_.tactical_multipliers.end())
        return {it->second.self_mult, it->second.opponent_mult};
    return {1.0, 1.0};
}

double ModelParamsClass::tacticalSpeed(Mobility mob, Terrain ter) const {
    auto mit = data_.tactical_speeds.find(mob);
    if (mit == data_.tactical_speeds.end()) return 0.0;
    auto tit = mit->second.find(ter);
    return (tit != mit->second.end()) ? tit->second : 0.0;
}

std::vector<std::string> ModelParamsClass::tacticalStateNames() const {
    std::vector<std::string> names;
    for (const auto& [name, _] : data_.tactical_multipliers)
        names.push_back(name);
    return names;
}
