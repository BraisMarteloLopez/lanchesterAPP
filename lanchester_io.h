// lanchester_io.h — Catalogo de vehiculos, I/O JSON/CSV, escenarios, batch, sweep
#pragma once

#include "lanchester_model.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Carga de parametros del modelo
// ---------------------------------------------------------------------------

inline ModelParams load_model_params(const std::string& path) {
    ModelParams mp;
    std::ifstream ifs(path);
    if (!ifs.is_open()) return mp;
    try {
        json j = json::parse(ifs);
        if (j.contains("kill_probability_slope"))
            mp.kill_probability_slope = j["kill_probability_slope"].get<double>();
        if (j.contains("distance_degradation_coefficients")) {
            const auto& d = j["distance_degradation_coefficients"];
            if (d.contains("c_dk"))    mp.dist_coeff.c_dk    = d["c_dk"].get<double>();
            if (d.contains("c_f"))     mp.dist_coeff.c_f     = d["c_f"].get<double>();
            if (d.contains("c_dk2"))   mp.dist_coeff.c_dk2   = d["c_dk2"].get<double>();
            if (d.contains("c_dk_f"))  mp.dist_coeff.c_dk_f  = d["c_dk_f"].get<double>();
            if (d.contains("c_f2"))    mp.dist_coeff.c_f2    = d["c_f2"].get<double>();
            if (d.contains("c_const")) mp.dist_coeff.c_const = d["c_const"].get<double>();
        }
        if (j.contains("terrain_fire_effectiveness")) {
            const auto& t = j["terrain_fire_effectiveness"];
            if (t.contains("FACIL"))   mp.terrain_fire_mult_facil   = t["FACIL"].get<double>();
            if (t.contains("MEDIO"))   mp.terrain_fire_mult_medio   = t["MEDIO"].get<double>();
            if (t.contains("DIFICIL")) mp.terrain_fire_mult_dificil = t["DIFICIL"].get<double>();
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Aviso: error leyendo model_params.json: %s. Usando defaults.\n", e.what());
    }
    return mp;
}

// ---------------------------------------------------------------------------
// Catalogo de vehiculos
// ---------------------------------------------------------------------------

inline VehicleParams vehicle_from_json(const json& j) {
    VehicleParams v;
    v.name  = j.at("name").get<std::string>();
    v.D     = j.at("D").get<double>();
    v.P     = j.at("P").get<double>();
    v.U     = j.at("U").get<double>();
    v.c     = j.at("c").get<double>();
    v.A_max = j.at("A_max").get<double>();
    v.f     = j.at("f").get<double>();
    v.CC    = j.at("CC").get<int>();
    v.P_cc  = j.at("P_cc").get<double>();
    v.D_cc  = j.at("D_cc").get<double>();
    v.c_cc  = j.at("c_cc").get<double>();
    v.A_cc  = j.at("A_cc").get<double>();
    v.M     = j.at("M").get<double>();
    v.f_cc  = j.at("f_cc").get<double>();
    return v;
}

inline VehicleCatalog load_catalog(const std::string& path) {
    VehicleCatalog cat;
    std::ifstream ifs(path);
    if (!ifs.is_open()) return cat;
    json j = json::parse(ifs);
    for (const auto& vj : j.at("vehicles"))
        cat[vj.at("name").get<std::string>()] = vehicle_from_json(vj);
    return cat;
}

inline VehicleParams find_vehicle(const std::string& name,
                                  const VehicleCatalog& primary,
                                  const VehicleCatalog& secondary) {
    auto it = primary.find(name);
    if (it != primary.end()) return it->second;
    it = secondary.find(name);
    if (it != secondary.end()) return it->second;
    throw std::runtime_error(
        "Vehiculo '" + name + "' no encontrado en catalogos.");
}

// ---------------------------------------------------------------------------
// Parseo y validacion de escenarios JSON
// ---------------------------------------------------------------------------

inline std::vector<CompositionEntry> parse_composition(
    const json& arr, const VehicleCatalog& primary,
    const VehicleCatalog& secondary)
{
    std::vector<CompositionEntry> comp;
    for (const auto& item : arr) {
        CompositionEntry ce;
        ce.vehicle = find_vehicle(item.at("vehicle").get<std::string>(),
                                  primary, secondary);
        ce.count = item.at("count").get<int>();
        if (ce.count < 1)
            throw std::runtime_error(
                "count debe ser >= 1 para vehiculo '" + ce.vehicle.name + "'.");
        comp.push_back(ce);
    }
    return comp;
}

inline void validate_side_params(const json& side, const std::string& label) {
    auto check_range = [&](const std::string& field, double lo, double hi) {
        if (!side.contains(field)) return;
        double v = side[field].get<double>();
        if (v < lo || v > hi)
            throw std::runtime_error(
                label + "." + field + " = " + std::to_string(v) +
                " fuera de rango [" + std::to_string(lo) + ", " +
                std::to_string(hi) + "].");
    };
    check_range("aft_casualties_pct", 0.0, 1.0);
    check_range("engagement_fraction", 0.0, 1.0);
    check_range("rate_factor", 0.0, 10.0);
    check_range("count_factor", 0.0, 10.0);
}

inline void validate_solver_params(double h, double t_max) {
    if (h <= 0.0)
        throw std::runtime_error(
            "solver.h debe ser > 0 (actual: " + std::to_string(h) + ").");
    if (t_max <= 0.0)
        throw std::runtime_error(
            "solver.t_max_minutes debe ser > 0 (actual: " +
            std::to_string(t_max) + ").");
}

// ---------------------------------------------------------------------------
// Ejecucion de escenario completo (con encadenamiento)
// ---------------------------------------------------------------------------

inline ScenarioOutput run_scenario(const json& scenario,
                                   const VehicleCatalog& blue_cat,
                                   const VehicleCatalog& red_cat,
                                   AggregationMode agg_mode = AggregationMode::PRE) {
    ScenarioOutput out;
    out.scenario_id = scenario.at("scenario_id").get<std::string>();

    std::string terrain_str = scenario.at("terrain").get<std::string>();
    double distance_m = scenario.at("engagement_distance_m").get<double>();
    Terrain terrain = parse_terrain(terrain_str);

    double h     = 1.0 / 600.0;
    double t_max = 30.0;
    if (scenario.contains("solver")) {
        const auto& sv = scenario["solver"];
        if (sv.contains("h"))             h     = sv["h"].get<double>();
        if (sv.contains("t_max_minutes")) t_max = sv["t_max_minutes"].get<double>();
    }
    validate_solver_params(h, t_max);

    double blue_survivors_prev = -1;
    double red_survivors_prev  = -1;
    double accumulated_time = 0;
    std::vector<CompositionEntry> blue_comp_carry, red_comp_carry;

    const auto& seq = scenario.at("combat_sequence");
    for (size_t ci = 0; ci < seq.size(); ++ci) {
        const auto& combat = seq[ci];
        int combat_id = combat.at("combat_id").get<int>();

        const auto& blue_j = combat.at("blue");
        const auto& red_j  = combat.at("red");

        std::string ctx = "combat_sequence[" + std::to_string(ci) + "]";
        validate_side_params(blue_j, ctx + ".blue");
        validate_side_params(red_j,  ctx + ".red");

        auto blue_comp_this = parse_composition(
            blue_j.at("composition"), blue_cat, red_cat);
        auto red_comp_this = parse_composition(
            red_j.at("composition"), red_cat, blue_cat);

        std::vector<CompositionEntry> reinf_blue, reinf_red;
        if (combat.contains("reinforcements_blue"))
            reinf_blue = parse_composition(combat["reinforcements_blue"],
                                           blue_cat, red_cat);
        if (combat.contains("reinforcements_red"))
            reinf_red = parse_composition(combat["reinforcements_red"],
                                          red_cat, blue_cat);

        std::vector<CompositionEntry> blue_comp, red_comp;
        if (ci == 0) {
            blue_comp = blue_comp_this;
            red_comp  = red_comp_this;
        } else {
            blue_comp = blue_comp_carry;
            red_comp  = red_comp_carry;
            for (auto& e : blue_comp_this) blue_comp.push_back(e);
            for (auto& e : red_comp_this)  red_comp.push_back(e);
            for (auto& e : reinf_blue) blue_comp.push_back(e);
            for (auto& e : reinf_red)  red_comp.push_back(e);
        }

        double disp_dist = 0;
        if (combat.contains("displacement_distance_m"))
            disp_dist = combat["displacement_distance_m"].get<double>();

        Mobility mob_blue = parse_mobility(blue_j.at("mobility").get<std::string>());
        Mobility mob_red  = parse_mobility(red_j.at("mobility").get<std::string>());
        double t_disp = displacement_time_minutes(disp_dist, mob_blue, mob_red, terrain);

        CombatInput cinput;
        cinput.combat_id = combat_id;
        cinput.blue_composition = blue_comp;
        cinput.red_composition  = red_comp;
        cinput.blue_state = blue_j.at("tactical_state").get<std::string>();
        cinput.red_state  = red_j.at("tactical_state").get<std::string>();
        cinput.blue_mobility_str = blue_j.at("mobility").get<std::string>();
        cinput.red_mobility_str  = red_j.at("mobility").get<std::string>();
        cinput.blue_aft_pct = blue_j.value("aft_casualties_pct", 0.0);
        cinput.red_aft_pct  = red_j.value("aft_casualties_pct", 0.0);
        cinput.blue_engagement_fraction = blue_j.value("engagement_fraction", 1.0);
        cinput.red_engagement_fraction  = red_j.value("engagement_fraction", 1.0);
        cinput.blue_rate_factor  = blue_j.value("rate_factor", 1.0);
        cinput.red_rate_factor   = red_j.value("rate_factor", 1.0);
        cinput.blue_count_factor = blue_j.value("count_factor", 1.0);
        cinput.red_count_factor  = red_j.value("count_factor", 1.0);
        cinput.distance_m = distance_m;
        cinput.h     = h;
        cinput.t_max = t_max;
        cinput.aggregation_mode = agg_mode;
        cinput.terrain = terrain;

        bool red_attacks  = (cinput.red_state == "Ataque a posicion defensiva");
        bool blue_attacks = (cinput.blue_state == "Ataque a posicion defensiva");
        if (red_attacks || blue_attacks) {
            double v_approach = 0;
            if (red_attacks)
                v_approach += tactical_speed(mob_red, terrain);
            if (blue_attacks)
                v_approach += tactical_speed(mob_blue, terrain);
            cinput.approach_speed_kmh = v_approach;
        }

        if (ci > 0 && blue_survivors_prev >= 0) {
            double extra_blue = static_cast<double>(
                total_count(blue_comp_this) + total_count(reinf_blue));
            double extra_red = static_cast<double>(
                total_count(red_comp_this) + total_count(reinf_red));
            cinput.blue_override_initial = blue_survivors_prev + extra_blue;
            cinput.red_override_initial  = red_survivors_prev + extra_red;
            cinput.blue_override_initial *= (1.0 - cinput.blue_aft_pct);
            cinput.red_override_initial  *= (1.0 - cinput.red_aft_pct);
            cinput.blue_override_initial *= cinput.blue_engagement_fraction
                                          * cinput.blue_count_factor;
            cinput.red_override_initial  *= cinput.red_engagement_fraction
                                          * cinput.red_count_factor;
        }

        CombatResult result = simulate_combat(cinput);

        accumulated_time += t_disp + result.duration_contact_minutes;
        result.duration_total_minutes = accumulated_time;

        blue_survivors_prev = result.blue_survivors;
        red_survivors_prev  = result.red_survivors;

        blue_comp_carry = blue_comp;
        red_comp_carry  = red_comp;
        distribute_casualties_by_vulnerability(
            blue_comp_carry, result.blue_casualties);
        distribute_casualties_by_vulnerability(
            red_comp_carry, result.red_casualties);

        out.combats.push_back(result);
    }

    return out;
}

// ---------------------------------------------------------------------------
// Salida JSON / CSV
// ---------------------------------------------------------------------------

inline double round_to(double v, int decimals) {
    double f = std::pow(10.0, decimals);
    return std::round(v * f) / f;
}

inline json result_to_json(const CombatResult& r, int precision = 6) {
    json j;
    j["combat_id"]                = r.combat_id;
    j["outcome"]                  = outcome_str(r.outcome);
    j["duration_contact_minutes"] = round_to(r.duration_contact_minutes, precision);
    j["duration_total_minutes"]   = round_to(r.duration_total_minutes, precision);
    j["blue_initial"]             = round_to(r.blue_initial, precision);
    j["red_initial"]              = round_to(r.red_initial, precision);
    j["blue_survivors"]           = round_to(r.blue_survivors, precision);
    j["red_survivors"]            = round_to(r.red_survivors, precision);
    j["blue_casualties"]          = round_to(r.blue_casualties, precision);
    j["red_casualties"]           = round_to(r.red_casualties, precision);
    j["blue_ammo_consumed"]       = round_to(r.blue_ammo_consumed, precision);
    j["red_ammo_consumed"]        = round_to(r.red_ammo_consumed, precision);
    j["blue_cc_ammo_consumed"]    = round_to(r.blue_cc_ammo_consumed, precision);
    j["red_cc_ammo_consumed"]     = round_to(r.red_cc_ammo_consumed, precision);
    j["static_advantage"]         = round_to(r.static_advantage, precision);
    return j;
}

inline json scenario_to_json(const ScenarioOutput& out) {
    json j;
    j["scenario_id"] = out.scenario_id;
    j["combats"] = json::array();
    for (const auto& r : out.combats)
        j["combats"].push_back(result_to_json(r));
    return j;
}

const char CSV_SEP = ';';

inline void write_csv_header(std::ostream& os, const std::string& extra_col = "") {
    if (!extra_col.empty()) os << extra_col << CSV_SEP;
    os << "scenario_id" << CSV_SEP << "combat_id" << CSV_SEP
       << "outcome" << CSV_SEP << "duration_contact_minutes" << CSV_SEP
       << "duration_total_minutes" << CSV_SEP << "blue_initial" << CSV_SEP
       << "red_initial" << CSV_SEP << "blue_survivors" << CSV_SEP
       << "red_survivors" << CSV_SEP << "blue_casualties" << CSV_SEP
       << "red_casualties" << CSV_SEP << "blue_ammo_consumed" << CSV_SEP
       << "red_ammo_consumed" << CSV_SEP << "blue_cc_ammo_consumed" << CSV_SEP
       << "red_cc_ammo_consumed" << CSV_SEP << "static_advantage" << "\n";
}

inline void write_csv_row(std::ostream& os, const std::string& scenario_id,
                          const CombatResult& r, const std::string& extra_val = "",
                          int precision = 6) {
    if (!extra_val.empty()) os << extra_val << CSV_SEP;
    os << scenario_id << CSV_SEP << r.combat_id << CSV_SEP
       << outcome_str(r.outcome) << CSV_SEP
       << round_to(r.duration_contact_minutes, precision) << CSV_SEP
       << round_to(r.duration_total_minutes, precision) << CSV_SEP
       << round_to(r.blue_initial, precision) << CSV_SEP
       << round_to(r.red_initial, precision) << CSV_SEP
       << round_to(r.blue_survivors, precision) << CSV_SEP
       << round_to(r.red_survivors, precision) << CSV_SEP
       << round_to(r.blue_casualties, precision) << CSV_SEP
       << round_to(r.red_casualties, precision) << CSV_SEP
       << round_to(r.blue_ammo_consumed, precision) << CSV_SEP
       << round_to(r.red_ammo_consumed, precision) << CSV_SEP
       << round_to(r.blue_cc_ammo_consumed, precision) << CSV_SEP
       << round_to(r.red_cc_ammo_consumed, precision) << CSV_SEP
       << round_to(r.static_advantage, precision) << "\n";
}

// ---------------------------------------------------------------------------
// Parser de paths JSON (para --sweep)
// ---------------------------------------------------------------------------

struct PathSegment {
    std::string key;
    int index = -1;
};

inline std::vector<PathSegment> parse_json_path(const std::string& path) {
    std::vector<PathSegment> segments;
    std::string current;
    for (size_t i = 0; i < path.size(); ++i) {
        char ch = path[i];
        if (ch == '.') {
            if (!current.empty()) {
                segments.push_back({current, -1});
                current.clear();
            }
        } else if (ch == '[') {
            if (!current.empty()) {
                segments.push_back({current, -1});
                current.clear();
            }
            size_t end = path.find(']', i + 1);
            if (end == std::string::npos) {
                std::fprintf(stderr, "Error: corchete sin cerrar en path '%s'\n",
                             path.c_str());
                return {};
            }
            int idx = std::stoi(path.substr(i + 1, end - i - 1));
            segments.push_back({"", idx});
            i = end;
        } else {
            current += ch;
        }
    }
    if (!current.empty())
        segments.push_back({current, -1});
    return segments;
}

inline json* resolve_json_path(json& root, const std::vector<PathSegment>& segments) {
    json* current = &root;
    for (const auto& seg : segments) {
        if (seg.index >= 0) {
            if (!current->is_array() ||
                seg.index >= static_cast<int>(current->size()))
                return nullptr;
            current = &(*current)[seg.index];
        } else {
            if (!current->is_object() || !current->contains(seg.key))
                return nullptr;
            current = &(*current)[seg.key];
        }
    }
    return current;
}

// ---------------------------------------------------------------------------
// Modo batch
// ---------------------------------------------------------------------------

inline void run_batch(const std::string& dir_path, const std::string& output_path,
                      const VehicleCatalog& blue_cat, const VehicleCatalog& red_cat,
                      AggregationMode agg_mode = AggregationMode::PRE) {
    std::vector<std::string> files;
    for (const auto& entry : fs::directory_iterator(dir_path)) {
        if (entry.path().extension() == ".json")
            files.push_back(entry.path().string());
    }
    std::sort(files.begin(), files.end());

    if (files.empty()) {
        std::fprintf(stderr, "No se encontraron ficheros JSON en '%s'.\n",
                     dir_path.c_str());
        return;
    }

    std::ofstream ofs;
    std::ostream* out = &std::cout;
    if (!output_path.empty()) {
        ofs.open(output_path);
        if (!ofs.is_open()) {
            std::fprintf(stderr, "Error: no se pudo escribir '%s'.\n",
                         output_path.c_str());
            return;
        }
        out = &ofs;
    }

    write_csv_header(*out);

    for (const auto& file : files) {
        std::ifstream ifs(file);
        if (!ifs.is_open()) {
            std::fprintf(stderr, "Aviso: no se pudo abrir '%s', saltando.\n",
                         file.c_str());
            continue;
        }
        try {
            json scenario = json::parse(ifs);
            ScenarioOutput result = run_scenario(scenario, blue_cat, red_cat, agg_mode);
            for (const auto& r : result.combats)
                write_csv_row(*out, result.scenario_id, r);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "Error procesando '%s': %s\n",
                         file.c_str(), e.what());
        }
    }

    if (!output_path.empty())
        std::fprintf(stderr, "Batch: %zu escenarios procesados -> '%s'\n",
                     files.size(), output_path.c_str());
}

// ---------------------------------------------------------------------------
// Modo sweep
// ---------------------------------------------------------------------------

inline void run_sweep(const std::string& scenario_path, const std::string& param_path,
                      double start, double end, double step,
                      const std::string& output_path,
                      const VehicleCatalog& blue_cat, const VehicleCatalog& red_cat,
                      AggregationMode agg_mode = AggregationMode::PRE) {
    std::ifstream ifs(scenario_path);
    if (!ifs.is_open()) {
        std::fprintf(stderr, "Error: no se pudo abrir '%s'.\n",
                     scenario_path.c_str());
        return;
    }
    json base_scenario;
    try {
        base_scenario = json::parse(ifs);
    } catch (const json::parse_error& e) {
        std::fprintf(stderr, "Error JSON en '%s': %s\n",
                     scenario_path.c_str(), e.what());
        return;
    }

    auto segments = parse_json_path(param_path);
    if (segments.empty()) return;

    json* target = resolve_json_path(base_scenario, segments);
    if (!target) {
        std::fprintf(stderr, "Error: path '%s' no encontrado en el escenario.\n",
                     param_path.c_str());
        return;
    }

    std::ofstream ofs;
    std::ostream* out = &std::cout;
    if (!output_path.empty()) {
        ofs.open(output_path);
        if (!ofs.is_open()) {
            std::fprintf(stderr, "Error: no se pudo escribir '%s'.\n",
                         output_path.c_str());
            return;
        }
        out = &ofs;
    }

    write_csv_header(*out, param_path);

    int n_steps = static_cast<int>(std::round((end - start) / step));
    for (int i = 0; i <= n_steps; ++i) {
        double val = start + i * step;
        json scenario = base_scenario;
        json* t = resolve_json_path(scenario, segments);
        if (!t) continue;

        if (target->is_number_integer())
            *t = static_cast<int>(std::round(val));
        else
            *t = val;

        ScenarioOutput result = run_scenario(scenario, blue_cat, red_cat, agg_mode);
        if (!result.combats.empty()) {
            std::string val_str;
            if (target->is_number_integer())
                val_str = std::to_string(static_cast<int>(std::round(val)));
            else {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.4f", val);
                val_str = buf;
            }
            write_csv_row(*out, result.scenario_id,
                          result.combats.back(), val_str);
        }
    }

    if (!output_path.empty())
        std::fprintf(stderr, "Sweep: %d valores de '%s' [%.4f..%.4f] -> '%s'\n",
                     n_steps + 1, param_path.c_str(), start, end, output_path.c_str());
}

// ---------------------------------------------------------------------------
// Utilidades
// ---------------------------------------------------------------------------

inline std::string exe_directory(const char* argv0) {
    try {
        auto real = fs::read_symlink("/proc/self/exe");
        return real.parent_path().string();
    } catch (...) {}
    std::string path(argv0);
    auto pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return ".";
    return path.substr(0, pos);
}
