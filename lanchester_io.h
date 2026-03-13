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

// Helper: lee un parametro que puede ser escalar o {value: X, ...}
inline double read_param(const json& j, const std::string& key, double default_val) {
    if (!j.contains(key)) return default_val;
    const auto& v = j[key];
    if (v.is_object() && v.contains("value")) return v["value"].get<double>();
    if (v.is_number()) return v.get<double>();
    return default_val;
}

inline ModelParams load_model_params(const std::string& path) {
    ModelParams mp;
    std::ifstream ifs(path);
    if (!ifs.is_open()) return mp;
    try {
        json j = json::parse(ifs);
        mp.kill_probability_slope = read_param(j, "kill_probability_slope", 175.0);
        if (j.contains("distance_degradation_coefficients")) {
            const auto& d = j["distance_degradation_coefficients"];
            mp.dist_coeff.c_dk    = read_param(d, "c_dk",    mp.dist_coeff.c_dk);
            mp.dist_coeff.c_f     = read_param(d, "c_f",     mp.dist_coeff.c_f);
            mp.dist_coeff.c_dk2   = read_param(d, "c_dk2",   mp.dist_coeff.c_dk2);
            mp.dist_coeff.c_dk_f  = read_param(d, "c_dk_f",  mp.dist_coeff.c_dk_f);
            mp.dist_coeff.c_f2    = read_param(d, "c_f2",    mp.dist_coeff.c_f2);
            mp.dist_coeff.c_const = read_param(d, "c_const", mp.dist_coeff.c_const);
        }
        if (j.contains("terrain_fire_effectiveness")) {
            const auto& t = j["terrain_fire_effectiveness"];
            mp.terrain_fire_mult_facil   = read_param(t, "FACIL",   mp.terrain_fire_mult_facil);
            mp.terrain_fire_mult_medio   = read_param(t, "MEDIO",   mp.terrain_fire_mult_medio);
            mp.terrain_fire_mult_dificil = read_param(t, "DIFICIL", mp.terrain_fire_mult_dificil);
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
                mp.tactical_multipliers[it.key()] = tmd;
            }
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

// ---------------------------------------------------------------------------
// Salida JSON Monte Carlo
// ---------------------------------------------------------------------------

inline json stats_to_json(const PercentileStats& ps, int p = 6) {
    json j;
    j["mean"]   = round_to(ps.mean, p);
    j["std"]    = round_to(ps.std, p);
    j["p05"]    = round_to(ps.p05, p);
    j["p25"]    = round_to(ps.p25, p);
    j["median"] = round_to(ps.median, p);
    j["p75"]    = round_to(ps.p75, p);
    j["p95"]    = round_to(ps.p95, p);
    return j;
}

inline json mc_result_to_json(const MonteCarloResult& mc, int p = 6) {
    json j;
    j["combat_id"] = mc.combat_id;
    j["deterministic"] = result_to_json(mc.deterministic, p);
    json mcj;
    int n = mc.n_replicas;
    mcj["outcome_distribution"] = {
        {"BLUE_WINS",     n > 0 ? round_to(static_cast<double>(mc.count_blue_wins) / n, 4) : 0.0},
        {"RED_WINS",      n > 0 ? round_to(static_cast<double>(mc.count_red_wins) / n, 4) : 0.0},
        {"DRAW",          n > 0 ? round_to(static_cast<double>(mc.count_draw) / n, 4) : 0.0},
        {"INDETERMINATE", n > 0 ? round_to(static_cast<double>(mc.count_indeterminate) / n, 4) : 0.0}
    };
    mcj["blue_survivors"]    = stats_to_json(mc.blue_survivors, p);
    mcj["red_survivors"]     = stats_to_json(mc.red_survivors, p);
    mcj["duration_minutes"]  = stats_to_json(mc.duration, p);
    j["montecarlo"] = mcj;
    return j;
}

inline json mc_scenario_to_json(const MonteCarloScenarioOutput& out) {
    json j;
    j["scenario_id"] = out.scenario_id;
    j["mode"] = "montecarlo";
    j["n_replicas"] = out.n_replicas;
    j["seed"] = out.seed;
    j["combats"] = json::array();
    for (const auto& mc : out.combats)
        j["combats"].push_back(mc_result_to_json(mc));
    return j;
}

// ---------------------------------------------------------------------------
// Ejecucion Monte Carlo de escenario completo (cadenas completas)
// ---------------------------------------------------------------------------

inline MonteCarloScenarioOutput run_scenario_montecarlo(
    const json& scenario,
    const VehicleCatalog& blue_cat, const VehicleCatalog& red_cat,
    AggregationMode agg_mode, int n_replicas, uint64_t seed)
{
    MonteCarloScenarioOutput mc_out;
    mc_out.scenario_id = scenario.at("scenario_id").get<std::string>();
    mc_out.n_replicas  = n_replicas;
    mc_out.seed        = seed;

    // Parse scenario structure once
    std::string terrain_str = scenario.at("terrain").get<std::string>();
    double distance_m = scenario.at("engagement_distance_m").get<double>();
    Terrain terrain = parse_terrain(terrain_str);
    double h = 1.0 / 600.0, t_max = 30.0;
    if (scenario.contains("solver")) {
        const auto& sv = scenario["solver"];
        if (sv.contains("h"))             h     = sv["h"].get<double>();
        if (sv.contains("t_max_minutes")) t_max = sv["t_max_minutes"].get<double>();
    }
    validate_solver_params(h, t_max);

    const auto& seq = scenario.at("combat_sequence");
    int n_combats = static_cast<int>(seq.size());

    // Build CombatInput templates for each combat (without override_initial)
    struct CombatTemplate {
        int combat_id;
        std::vector<CompositionEntry> blue_comp, red_comp;
        std::vector<CompositionEntry> blue_comp_new, red_comp_new;
        std::vector<CompositionEntry> reinf_blue, reinf_red;
        std::string blue_state, red_state;
        Mobility mob_blue, mob_red;
        double blue_aft_pct, red_aft_pct;
        double blue_eng_frac, red_eng_frac;
        double blue_rate_fac, red_rate_fac;
        double blue_cnt_fac, red_cnt_fac;
        double approach_speed_kmh;
        double disp_dist;
    };

    std::vector<CombatTemplate> templates(n_combats);
    std::vector<CompositionEntry> base_blue_carry, base_red_carry;

    for (int ci = 0; ci < n_combats; ++ci) {
        const auto& combat = seq[ci];
        auto& tmpl = templates[ci];

        tmpl.combat_id = combat.at("combat_id").get<int>();
        const auto& blue_j = combat.at("blue");
        const auto& red_j  = combat.at("red");

        std::string ctx = "combat_sequence[" + std::to_string(ci) + "]";
        validate_side_params(blue_j, ctx + ".blue");
        validate_side_params(red_j,  ctx + ".red");

        tmpl.blue_comp_new = parse_composition(blue_j.at("composition"), blue_cat, red_cat);
        tmpl.red_comp_new  = parse_composition(red_j.at("composition"), red_cat, blue_cat);

        if (combat.contains("reinforcements_blue"))
            tmpl.reinf_blue = parse_composition(combat["reinforcements_blue"], blue_cat, red_cat);
        if (combat.contains("reinforcements_red"))
            tmpl.reinf_red = parse_composition(combat["reinforcements_red"], red_cat, blue_cat);

        tmpl.blue_state = blue_j.at("tactical_state").get<std::string>();
        tmpl.red_state  = red_j.at("tactical_state").get<std::string>();
        tmpl.mob_blue = parse_mobility(blue_j.at("mobility").get<std::string>());
        tmpl.mob_red  = parse_mobility(red_j.at("mobility").get<std::string>());
        tmpl.blue_aft_pct = blue_j.value("aft_casualties_pct", 0.0);
        tmpl.red_aft_pct  = red_j.value("aft_casualties_pct", 0.0);
        tmpl.blue_eng_frac = blue_j.value("engagement_fraction", 1.0);
        tmpl.red_eng_frac  = red_j.value("engagement_fraction", 1.0);
        tmpl.blue_rate_fac = blue_j.value("rate_factor", 1.0);
        tmpl.red_rate_fac  = red_j.value("rate_factor", 1.0);
        tmpl.blue_cnt_fac  = blue_j.value("count_factor", 1.0);
        tmpl.red_cnt_fac   = red_j.value("count_factor", 1.0);

        tmpl.disp_dist = 0;
        if (combat.contains("displacement_distance_m"))
            tmpl.disp_dist = combat["displacement_distance_m"].get<double>();

        tmpl.approach_speed_kmh = 0;
        bool red_attacks  = (tmpl.red_state == "Ataque a posicion defensiva");
        bool blue_attacks = (tmpl.blue_state == "Ataque a posicion defensiva");
        if (red_attacks || blue_attacks) {
            double v = 0;
            if (red_attacks)  v += tactical_speed(tmpl.mob_red, terrain);
            if (blue_attacks) v += tactical_speed(tmpl.mob_blue, terrain);
            tmpl.approach_speed_kmh = v;
        }

        // Store base compositions for first combat
        if (ci == 0) {
            tmpl.blue_comp = tmpl.blue_comp_new;
            tmpl.red_comp  = tmpl.red_comp_new;
        }
    }

    // First run deterministic once for each combat (for reference)
    ScenarioOutput det_out = run_scenario(scenario, blue_cat, red_cat, agg_mode);

    // Initialize per-combat collectors
    struct CombatCollector {
        std::vector<double> blue_surv, red_surv, duration;
        int count_bw = 0, count_rw = 0, count_d = 0, count_i = 0;
    };
    std::vector<CombatCollector> collectors(n_combats);
    for (auto& col : collectors) {
        col.blue_surv.resize(n_replicas);
        col.red_surv.resize(n_replicas);
        col.duration.resize(n_replicas);
    }

    // Run complete chains
    std::mt19937 rng(seed);

    for (int rep = 0; rep < n_replicas; ++rep) {
        double blue_surv_carry = -1;
        double red_surv_carry  = -1;
        std::vector<CompositionEntry> carry_blue, carry_red;

        for (int ci = 0; ci < n_combats; ++ci) {
            const auto& tmpl = templates[ci];

            std::vector<CompositionEntry> blue_comp, red_comp;
            if (ci == 0) {
                blue_comp = tmpl.blue_comp_new;
                red_comp  = tmpl.red_comp_new;
            } else {
                blue_comp = carry_blue;
                red_comp  = carry_red;
                for (const auto& e : tmpl.blue_comp_new) blue_comp.push_back(e);
                for (const auto& e : tmpl.red_comp_new)  red_comp.push_back(e);
                for (const auto& e : tmpl.reinf_blue) blue_comp.push_back(e);
                for (const auto& e : tmpl.reinf_red)  red_comp.push_back(e);
            }

            CombatInput cinput;
            cinput.combat_id = tmpl.combat_id;
            cinput.blue_composition = blue_comp;
            cinput.red_composition  = red_comp;
            cinput.blue_state = tmpl.blue_state;
            cinput.red_state  = tmpl.red_state;
            cinput.blue_aft_pct = tmpl.blue_aft_pct;
            cinput.red_aft_pct  = tmpl.red_aft_pct;
            cinput.blue_engagement_fraction = tmpl.blue_eng_frac;
            cinput.red_engagement_fraction  = tmpl.red_eng_frac;
            cinput.blue_rate_factor  = tmpl.blue_rate_fac;
            cinput.red_rate_factor   = tmpl.red_rate_fac;
            cinput.blue_count_factor = tmpl.blue_cnt_fac;
            cinput.red_count_factor  = tmpl.red_cnt_fac;
            cinput.distance_m = distance_m;
            cinput.h = h;
            cinput.t_max = t_max;
            cinput.aggregation_mode = agg_mode;
            cinput.terrain = terrain;
            cinput.approach_speed_kmh = tmpl.approach_speed_kmh;

            if (ci > 0 && blue_surv_carry >= 0) {
                double extra_blue = static_cast<double>(
                    total_count(tmpl.blue_comp_new) + total_count(tmpl.reinf_blue));
                double extra_red = static_cast<double>(
                    total_count(tmpl.red_comp_new) + total_count(tmpl.reinf_red));
                cinput.blue_override_initial = blue_surv_carry + extra_blue;
                cinput.red_override_initial  = red_surv_carry + extra_red;
                cinput.blue_override_initial *= (1.0 - cinput.blue_aft_pct);
                cinput.red_override_initial  *= (1.0 - cinput.red_aft_pct);
                cinput.blue_override_initial *= cinput.blue_engagement_fraction
                                              * cinput.blue_count_factor;
                cinput.red_override_initial  *= cinput.red_engagement_fraction
                                              * cinput.red_count_factor;
            }

            CombatResult result = simulate_combat_stochastic(cinput, rng);

            auto& col = collectors[ci];
            col.blue_surv[rep] = result.blue_survivors;
            col.red_surv[rep]  = result.red_survivors;
            col.duration[rep]  = result.duration_contact_minutes;
            switch (result.outcome) {
                case Outcome::BLUE_WINS:     ++col.count_bw; break;
                case Outcome::RED_WINS:      ++col.count_rw; break;
                case Outcome::DRAW:          ++col.count_d; break;
                case Outcome::INDETERMINATE: ++col.count_i; break;
            }

            blue_surv_carry = result.blue_survivors;
            red_surv_carry  = result.red_survivors;

            carry_blue = blue_comp;
            carry_red  = red_comp;
            distribute_casualties_by_vulnerability(carry_blue, result.blue_casualties);
            distribute_casualties_by_vulnerability(carry_red,  result.red_casualties);
        }
    }

    // Aggregate results
    for (int ci = 0; ci < n_combats; ++ci) {
        auto& col = collectors[ci];
        MonteCarloResult mc;
        mc.combat_id  = templates[ci].combat_id;
        mc.n_replicas = n_replicas;
        mc.blue_survivors = compute_stats(col.blue_surv);
        mc.red_survivors  = compute_stats(col.red_surv);
        mc.duration       = compute_stats(col.duration);
        mc.count_blue_wins     = col.count_bw;
        mc.count_red_wins      = col.count_rw;
        mc.count_draw          = col.count_d;
        mc.count_indeterminate = col.count_i;
        if (ci < static_cast<int>(det_out.combats.size()))
            mc.deterministic = det_out.combats[ci];
        mc_out.combats.push_back(mc);
    }

    return mc_out;
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
// Modo sensibilidad
// ---------------------------------------------------------------------------

inline void run_sensitivity(const json& scenario, const std::string& output_path,
                             const VehicleCatalog& blue_cat, const VehicleCatalog& red_cat,
                             AggregationMode agg_mode = AggregationMode::PRE)
{
    // Run baseline
    ModelParams original_params = g_model_params;
    ScenarioOutput baseline = run_scenario(scenario, blue_cat, red_cat, agg_mode);
    if (baseline.combats.empty()) {
        std::fprintf(stderr, "Error: escenario sin combates.\n");
        return;
    }
    const CombatResult& ref = baseline.combats.back();
    double ref_blue = ref.blue_survivors;
    double ref_red  = ref.red_survivors;

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

    *out << "parametro" << CSV_SEP << "valor_base" << CSV_SEP << "factor" << CSV_SEP
         << "valor_test" << CSV_SEP << "blue_surv_ref" << CSV_SEP << "blue_surv_test" << CSV_SEP
         << "red_surv_ref" << CSV_SEP << "red_surv_test" << CSV_SEP
         << "blue_delta" << CSV_SEP << "red_delta" << CSV_SEP
         << "blue_elasticity" << CSV_SEP << "red_elasticity" << CSV_SEP
         << "outcome_ref" << CSV_SEP << "outcome_test\n";

    double factors[] = {0.8, 0.9, 1.1, 1.2};

    auto run_test = [&](const std::string& param_name, double base_val,
                        double factor, auto apply_fn) {
        double test_val = base_val * factor;
        apply_fn(test_val);
        ScenarioOutput test_out = run_scenario(scenario, blue_cat, red_cat, agg_mode);
        g_model_params = original_params;  // restore

        if (test_out.combats.empty()) return;
        const CombatResult& tr = test_out.combats.back();
        double blue_delta = tr.blue_survivors - ref_blue;
        double red_delta  = tr.red_survivors - ref_red;

        double pct_change = factor - 1.0;
        double blue_elast = 0, red_elast = 0;
        if (std::abs(pct_change) > 1e-9 && std::abs(ref_blue) > 1e-9)
            blue_elast = (blue_delta / ref_blue) / pct_change;
        if (std::abs(pct_change) > 1e-9 && std::abs(ref_red) > 1e-9)
            red_elast = (red_delta / ref_red) / pct_change;

        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "%s;%.6f;%.2f;%.6f;%.6f;%.6f;%.6f;%.6f;%.6f;%.6f;%.4f;%.4f;%s;%s\n",
            param_name.c_str(), base_val, factor, test_val,
            ref_blue, tr.blue_survivors, ref_red, tr.red_survivors,
            blue_delta, red_delta, blue_elast, red_elast,
            outcome_str(ref.outcome), outcome_str(tr.outcome));
        *out << buf;
    };

    // Barrer kill_probability_slope
    for (double f : factors) {
        run_test("kill_probability_slope", original_params.kill_probability_slope, f,
            [](double v) { g_model_params.kill_probability_slope = v; });
    }

    // Barrer coeficientes de degradacion por distancia
    auto& dc = original_params.dist_coeff;
    struct CoeffEntry { const char* name; double val; void (*setter)(double); };
    CoeffEntry coeffs[] = {
        {"dist_coeff.c_dk",   dc.c_dk,    [](double v){ g_model_params.dist_coeff.c_dk = v; }},
        {"dist_coeff.c_f",    dc.c_f,     [](double v){ g_model_params.dist_coeff.c_f = v; }},
        {"dist_coeff.c_dk2",  dc.c_dk2,   [](double v){ g_model_params.dist_coeff.c_dk2 = v; }},
        {"dist_coeff.c_dk_f", dc.c_dk_f,  [](double v){ g_model_params.dist_coeff.c_dk_f = v; }},
        {"dist_coeff.c_f2",   dc.c_f2,    [](double v){ g_model_params.dist_coeff.c_f2 = v; }},
        {"dist_coeff.c_const",dc.c_const,  [](double v){ g_model_params.dist_coeff.c_const = v; }},
    };
    for (auto& ce : coeffs) {
        // Para coeficientes que pueden ser negativos o cercanos a 0,
        // usar perturbacion aditiva en vez de multiplicativa
        if (std::abs(ce.val) < 0.01) continue;
        for (double f : factors) {
            run_test(ce.name, ce.val, f, [&ce](double v) { ce.setter(v); });
        }
    }

    // Barrer multiplicadores de terreno
    for (double f : factors) {
        run_test("terrain_FACIL", original_params.terrain_fire_mult_facil, f,
            [](double v) { g_model_params.terrain_fire_mult_facil = v; });
    }
    for (double f : factors) {
        run_test("terrain_MEDIO", original_params.terrain_fire_mult_medio, f,
            [](double v) { g_model_params.terrain_fire_mult_medio = v; });
    }
    for (double f : factors) {
        run_test("terrain_DIFICIL", original_params.terrain_fire_mult_dificil, f,
            [](double v) { g_model_params.terrain_fire_mult_dificil = v; });
    }

    // Restaurar parametros originales
    g_model_params = original_params;

    if (!output_path.empty())
        std::fprintf(stderr, "Sensibilidad escrita en '%s'.\n", output_path.c_str());
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
