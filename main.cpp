// Modelo Lanchester-CIO — Ejecutable completo
// CIO / ET — Herramienta de investigacion interna
// Sesion 2: I/O JSON, encadenamiento de combates, CLI

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "include/nlohmann/json.hpp"
using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Estructuras de datos
// ---------------------------------------------------------------------------

struct VehicleParams {
    std::string name;
    double D       = 0;
    double P       = 0;
    double U       = 0;
    double c       = 0;
    double A_max   = 0;
    double f       = 0;
    int    CC      = 0;
    double P_cc    = 0;
    double D_cc    = 0;
    double c_cc    = 0;
    double A_cc    = 0;
    double M       = 0;
    double f_cc    = 0;
};

struct CompositionEntry {
    VehicleParams vehicle;
    int count = 0;
};

struct AggregatedParams {
    double D = 0, P = 0, U = 0, c = 0, A_max = 0, f = 0;
    int    n_total = 0;
    double P_cc = 0, D_cc = 0, c_cc = 0, A_cc = 0, M = 0, f_cc = 0;
    int    n_cc    = 0;
    bool   has_cc  = false;
};

enum class Outcome { BLUE_WINS, RED_WINS, DRAW, INDETERMINATE };

struct CombatResult {
    int    combat_id                = 0;
    Outcome outcome                = Outcome::INDETERMINATE;
    double duration_contact_minutes = 0;
    double duration_total_minutes   = 0;
    double blue_initial             = 0;
    double red_initial              = 0;
    double blue_survivors           = 0;
    double red_survivors            = 0;
    double blue_casualties          = 0;
    double red_casualties           = 0;
    double blue_ammo_consumed       = 0;
    double red_ammo_consumed        = 0;
    double blue_cc_ammo_consumed    = 0;
    double red_cc_ammo_consumed     = 0;
    double static_advantage         = 0;
};

struct TacticalMult {
    double self_mult;
    double opponent_mult;
};

struct EffectiveRates {
    double S_conv       = 0;
    double S_cc_static  = 0;
    double rate_factor  = 1.0;
    double c_agg        = 0;
    double c_cc_agg     = 0;
    double M_agg        = 0;
    bool   has_cc       = false;
    int    n_cc         = 0;
    int    n_total      = 0;
};

enum class Mobility { MUY_ALTA, ALTA, MEDIA, BAJA };
enum class Terrain  { FACIL, MEDIO, DIFICIL };

// ---------------------------------------------------------------------------
// Funciones del modelo
// ---------------------------------------------------------------------------

double kill_probability(double D_target, double P_attacker) {
    return 1.0 / (1.0 + std::exp((D_target - P_attacker) / 175.0));
}

double distance_degradation(double d, double f_dist, double range_max) {
    if (d > range_max) return 0.0;
    double dk = d / 1000.0;
    double g = -0.188 * dk
               - 0.865 * f_dist
               + 0.018 * dk * dk
               - 0.162 * dk * f_dist
               + 0.755 * f_dist * f_dist
               + 1.295;
    return std::clamp(g, 0.0, 1.0);
}

double static_rate_conv(double T, double G, double U, double c) {
    return T * G * U * c;
}

double static_rate_cc(double T_cc, double G_cc, double c_cc) {
    return c_cc * T_cc * G_cc;
}

double dynamic_rate_cc(double S_cc_static, double A_current, double A0,
                       double M, double c_cc, double t) {
    if (M <= 0.0 || A0 <= 0.0) return 0.0;
    double ammo_factor = std::max(0.0, M - c_cc * t) / M;
    return (A_current / A0) * S_cc_static * ammo_factor;
}

// ---------------------------------------------------------------------------
// Multiplicadores tacticos
// ---------------------------------------------------------------------------

TacticalMult get_tactical_multipliers(const std::string& state) {
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

// ---------------------------------------------------------------------------
// Velocidades tacticas
// ---------------------------------------------------------------------------

Mobility parse_mobility(const std::string& s) {
    if (s == "MUY_ALTA") return Mobility::MUY_ALTA;
    if (s == "ALTA")     return Mobility::ALTA;
    if (s == "MEDIA")    return Mobility::MEDIA;
    return Mobility::BAJA;
}

Terrain parse_terrain(const std::string& s) {
    if (s == "FACIL")    return Terrain::FACIL;
    if (s == "DIFICIL")  return Terrain::DIFICIL;
    return Terrain::MEDIO;
}

double tactical_speed(Mobility mob, Terrain ter) {
    static const double table[4][3] = {
        {40, 25, 12},
        {30, 20, 10},
        {20, 12,  6},
        {10,  6,  3},
    };
    return table[static_cast<int>(mob)][static_cast<int>(ter)];
}

double displacement_time_minutes(double distance_m, Mobility mob_blue,
                                 Mobility mob_red, Terrain terrain) {
    double v_blue = tactical_speed(mob_blue, terrain);
    double v_red  = tactical_speed(mob_red, terrain);
    double v_min  = std::min(v_blue, v_red);
    if (v_min <= 0.0 || distance_m <= 0.0) return 0.0;
    return (distance_m / 1000.0) * 60.0 / v_min;
}

// ---------------------------------------------------------------------------
// Modo de agregacion (global, configurable via CLI)
// ---------------------------------------------------------------------------

enum class AggregationMode { PRE, POST };
AggregationMode g_aggregation_mode = AggregationMode::PRE;

// ---------------------------------------------------------------------------
// Agregacion de fuerzas mixtas (modo pre-tasa)
// ---------------------------------------------------------------------------

AggregatedParams aggregate(const std::vector<CompositionEntry>& composition) {
    AggregatedParams agg{};
    int n_total = 0, n_cc = 0;
    double sum_D = 0, sum_P = 0, sum_U = 0, sum_c = 0, sum_Amax = 0, sum_f = 0;
    double sum_Pcc = 0, sum_Dcc = 0, sum_ccc = 0, sum_Acc = 0, sum_M = 0, sum_fcc = 0;

    for (const auto& entry : composition) {
        int n = entry.count;
        const auto& v = entry.vehicle;
        n_total += n;
        sum_D    += n * v.D;
        sum_P    += n * v.P;
        sum_U    += n * v.U;
        sum_c    += n * v.c;
        sum_Amax += n * v.A_max;
        sum_f    += n * v.f;
        if (v.CC) {
            n_cc    += n;
            sum_Pcc += n * v.P_cc;
            sum_Dcc += n * v.D_cc;
            sum_ccc += n * v.c_cc;
            sum_Acc += n * v.A_cc;
            sum_M   += n * v.M;
            sum_fcc += n * v.f_cc;
        }
    }

    agg.n_total = n_total;
    agg.n_cc    = n_cc;
    if (n_total > 0) {
        agg.D     = sum_D    / n_total;
        agg.P     = sum_P    / n_total;
        agg.U     = sum_U    / n_total;
        agg.c     = sum_c    / n_total;
        agg.A_max = sum_Amax / n_total;
        agg.f     = sum_f    / n_total;
    }
    if (n_cc > 0) {
        agg.has_cc = true;
        agg.P_cc = sum_Pcc / n_cc;  agg.D_cc = sum_Dcc / n_cc;
        agg.c_cc = sum_ccc / n_cc;  agg.A_cc = sum_Acc / n_cc;
        agg.M    = sum_M   / n_cc;  agg.f_cc = sum_fcc / n_cc;
    }
    return agg;
}

// ---------------------------------------------------------------------------
// Tasas efectivas
// ---------------------------------------------------------------------------

EffectiveRates compute_effective_rates(
    const AggregatedParams& attacker, const AggregatedParams& defender,
    double distance_m, const std::string& att_state,
    const std::string& def_state, double rate_factor)
{
    EffectiveRates er{};
    er.rate_factor = rate_factor;
    er.c_agg    = attacker.c;
    er.c_cc_agg = attacker.c_cc;
    er.M_agg    = attacker.M;
    er.has_cc   = attacker.has_cc;
    er.n_cc     = attacker.n_cc;
    er.n_total  = attacker.n_total;

    double T = kill_probability(defender.D, attacker.P);
    double G = distance_degradation(distance_m, attacker.f, attacker.A_max);
    double S = static_rate_conv(T, G, attacker.U, attacker.c);

    TacticalMult am = get_tactical_multipliers(att_state);
    TacticalMult dm = get_tactical_multipliers(def_state);
    er.S_conv = S * am.self_mult * dm.opponent_mult * rate_factor;

    if (attacker.has_cc) {
        double Tc = kill_probability(defender.D_cc, attacker.P_cc);
        double Gc = distance_degradation(distance_m, attacker.f_cc, attacker.A_cc);
        er.S_cc_static = static_rate_cc(Tc, Gc, attacker.c_cc);
    }
    return er;
}

// Agregacion post-tasa: calcula tasa por tipo de vehiculo y pondera
EffectiveRates compute_effective_rates_post(
    const std::vector<CompositionEntry>& att_comp,
    const AggregatedParams& defender,
    double distance_m, const std::string& att_state,
    const std::string& def_state, double rate_factor)
{
    EffectiveRates er{};
    er.rate_factor = rate_factor;

    TacticalMult am = get_tactical_multipliers(att_state);
    TacticalMult dm = get_tactical_multipliers(def_state);
    double tac_mult = am.self_mult * dm.opponent_mult;

    int n_total = 0, n_cc = 0;
    double weighted_S_conv = 0;
    double weighted_S_cc   = 0;
    double weighted_c      = 0;
    double weighted_c_cc   = 0;
    double weighted_M      = 0;

    for (const auto& entry : att_comp) {
        int n = entry.count;
        const auto& v = entry.vehicle;
        n_total += n;

        double T = kill_probability(defender.D, v.P);
        double G = distance_degradation(distance_m, v.f, v.A_max);
        double S = static_rate_conv(T, G, v.U, v.c);
        weighted_S_conv += n * S;
        weighted_c      += n * v.c;

        if (v.CC) {
            n_cc += n;
            double Tc = kill_probability(defender.D_cc, v.P_cc);
            double Gc = distance_degradation(distance_m, v.f_cc, v.A_cc);
            double Scc = static_rate_cc(Tc, Gc, v.c_cc);
            weighted_S_cc += n * Scc;
            weighted_c_cc += n * v.c_cc;
            weighted_M    += n * v.M;
        }
    }

    er.n_total = n_total;
    er.n_cc    = n_cc;
    er.has_cc  = (n_cc > 0);

    if (n_total > 0) {
        er.S_conv = (weighted_S_conv / n_total) * tac_mult * rate_factor;
        er.c_agg  = weighted_c / n_total;
    }
    if (n_cc > 0) {
        er.S_cc_static = weighted_S_cc / n_cc;
        er.c_cc_agg    = weighted_c_cc / n_cc;
        er.M_agg       = weighted_M    / n_cc;
    }
    return er;
}

double total_rate(const EffectiveRates& er, double t,
                  double N_att, double N_att0) {
    double s_cc = 0.0;
    if (er.has_cc && er.S_cc_static > 0.0)
        s_cc = dynamic_rate_cc(er.S_cc_static, N_att, N_att0,
                               er.M_agg, er.c_cc_agg, t) * er.rate_factor;
    return er.S_conv + s_cc;
}

// ---------------------------------------------------------------------------
// Fuerzas iniciales
// ---------------------------------------------------------------------------

double initial_forces(int n_total, double aft_pct, double eng_frac, double cnt_fac) {
    double n = static_cast<double>(n_total);
    return (n - n * aft_pct) * eng_frac * cnt_fac;
}

// ---------------------------------------------------------------------------
// Simulacion de un combate individual
// ---------------------------------------------------------------------------

struct CombatInput {
    int combat_id = 0;
    std::vector<CompositionEntry> blue_composition;
    std::vector<CompositionEntry> red_composition;
    std::string blue_state, red_state;
    std::string blue_mobility_str, red_mobility_str;
    double blue_aft_pct = 0, red_aft_pct = 0;
    double blue_engagement_fraction = 1.0, red_engagement_fraction = 1.0;
    double blue_rate_factor = 1.0, red_rate_factor = 1.0;
    double blue_count_factor = 1.0, red_count_factor = 1.0;
    double distance_m = 2000;
    double h       = 1.0 / 600.0;
    double t_max   = 30.0;
    // For chaining: override initial forces (survivors from previous combat)
    double blue_override_initial = -1;  // <0 means use composition
    double red_override_initial  = -1;
};

CombatResult simulate_combat(const CombatInput& input) {
    AggregatedParams blue_agg = aggregate(input.blue_composition);
    AggregatedParams red_agg  = aggregate(input.red_composition);

    double A0, R0;
    if (input.blue_override_initial >= 0)
        A0 = input.blue_override_initial;
    else
        A0 = initial_forces(blue_agg.n_total, input.blue_aft_pct,
                            input.blue_engagement_fraction, input.blue_count_factor);

    if (input.red_override_initial >= 0)
        R0 = input.red_override_initial;
    else
        R0 = initial_forces(red_agg.n_total, input.red_aft_pct,
                            input.red_engagement_fraction, input.red_count_factor);

    EffectiveRates blue_rates, red_rates;
    if (g_aggregation_mode == AggregationMode::POST) {
        blue_rates = compute_effective_rates_post(
            input.blue_composition, red_agg, input.distance_m,
            input.blue_state, input.red_state, input.blue_rate_factor);
        red_rates = compute_effective_rates_post(
            input.red_composition, blue_agg, input.distance_m,
            input.red_state, input.blue_state, input.red_rate_factor);
    } else {
        blue_rates = compute_effective_rates(
            blue_agg, red_agg, input.distance_m,
            input.blue_state, input.red_state, input.blue_rate_factor);
        red_rates = compute_effective_rates(
            red_agg, blue_agg, input.distance_m,
            input.red_state, input.blue_state, input.red_rate_factor);
    }

    double S_blue_t0 = total_rate(blue_rates, 0.0, A0, A0);
    double S_red_t0  = total_rate(red_rates,  0.0, R0, R0);
    double static_adv = 0.0;
    if (S_red_t0 * R0 * R0 > 0.0)
        static_adv = (S_blue_t0 * A0 * A0) / (S_red_t0 * R0 * R0);

    double A = A0, R = R0, t = 0.0;
    double blue_ammo = 0, red_ammo = 0, blue_cc_ammo = 0, red_cc_ammo = 0;

    double blue_cc_max = blue_rates.has_cc
        ? blue_rates.M_agg * std::max(0.0, A0 * (blue_agg.n_cc > 0
            ? static_cast<double>(blue_agg.n_cc) / blue_agg.n_total : 0.0))
        : 0.0;
    double red_cc_max = red_rates.has_cc
        ? red_rates.M_agg * std::max(0.0, R0 * (red_agg.n_cc > 0
            ? static_cast<double>(red_agg.n_cc) / red_agg.n_total : 0.0))
        : 0.0;

    double cc_ratio_blue = (blue_agg.n_total > 0)
        ? static_cast<double>(blue_agg.n_cc) / blue_agg.n_total : 0.0;
    double cc_ratio_red = (red_agg.n_total > 0)
        ? static_cast<double>(red_agg.n_cc) / red_agg.n_total : 0.0;

    while (A > 0.0 && R > 0.0 && t < input.t_max) {
        double Sr = total_rate(red_rates,  t, R, R0);
        double Sb = total_rate(blue_rates, t, A, A0);
        double A_new = std::max(0.0, A - Sr * R * input.h);
        double R_new = std::max(0.0, R - Sb * A * input.h);

        blue_ammo += blue_rates.c_agg * A * input.h;
        red_ammo  += red_rates.c_agg  * R * input.h;
        if (blue_rates.has_cc)
            blue_cc_ammo = std::min(blue_cc_ammo +
                blue_rates.c_cc_agg * A * cc_ratio_blue * input.h, blue_cc_max);
        if (red_rates.has_cc)
            red_cc_ammo = std::min(red_cc_ammo +
                red_rates.c_cc_agg * R * cc_ratio_red * input.h, red_cc_max);

        A = A_new; R = R_new; t += input.h;
    }

    Outcome outcome;
    if (A <= 0.0 && R <= 0.0)      outcome = Outcome::DRAW;
    else if (A <= 0.0)             outcome = Outcome::RED_WINS;
    else if (R <= 0.0)             outcome = Outcome::BLUE_WINS;
    else                           outcome = Outcome::INDETERMINATE;
    if (A > 0.0 && A < 0.5 && R > 0.0 && R < 0.5)
        outcome = Outcome::DRAW;

    CombatResult res;
    res.combat_id               = input.combat_id;
    res.outcome                 = outcome;
    res.duration_contact_minutes = t;
    res.duration_total_minutes   = t;  // updated by caller for chaining
    res.blue_initial            = A0;
    res.red_initial             = R0;
    res.blue_survivors          = std::max(0.0, A);
    res.red_survivors           = std::max(0.0, R);
    res.blue_casualties         = A0 - res.blue_survivors;
    res.red_casualties          = R0 - res.red_survivors;
    res.blue_ammo_consumed      = blue_ammo;
    res.red_ammo_consumed       = red_ammo;
    res.blue_cc_ammo_consumed   = blue_cc_ammo;
    res.red_cc_ammo_consumed    = red_cc_ammo;
    res.static_advantage        = static_adv;
    return res;
}

// ---------------------------------------------------------------------------
// Outcome string
// ---------------------------------------------------------------------------

const char* outcome_str(Outcome o) {
    switch (o) {
        case Outcome::BLUE_WINS:     return "BLUE_WINS";
        case Outcome::RED_WINS:      return "RED_WINS";
        case Outcome::DRAW:          return "DRAW";
        case Outcome::INDETERMINATE: return "INDETERMINATE";
    }
    return "UNKNOWN";
}

// ---------------------------------------------------------------------------
// Catalogo de vehiculos
// ---------------------------------------------------------------------------

using VehicleCatalog = std::map<std::string, VehicleParams>;

VehicleParams vehicle_from_json(const json& j) {
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

VehicleCatalog load_catalog(const std::string& path) {
    VehicleCatalog cat;
    std::ifstream ifs(path);
    if (!ifs.is_open()) return cat;
    json j = json::parse(ifs);
    for (const auto& vj : j.at("vehicles"))
        cat[vj.at("name").get<std::string>()] = vehicle_from_json(vj);
    return cat;
}

// Busqueda cruzada: primero en catalogo primario, luego en secundario
VehicleParams find_vehicle(const std::string& name,
                           const VehicleCatalog& primary,
                           const VehicleCatalog& secondary) {
    auto it = primary.find(name);
    if (it != primary.end()) return it->second;
    it = secondary.find(name);
    if (it != secondary.end()) return it->second;
    std::fprintf(stderr, "Error: vehiculo '%s' no encontrado en catalogos.\n",
                 name.c_str());
    std::exit(1);
}

// ---------------------------------------------------------------------------
// Lectura de composicion desde JSON
// ---------------------------------------------------------------------------

std::vector<CompositionEntry> parse_composition(
    const json& arr, const VehicleCatalog& primary,
    const VehicleCatalog& secondary)
{
    std::vector<CompositionEntry> comp;
    for (const auto& item : arr) {
        CompositionEntry ce;
        ce.vehicle = find_vehicle(item.at("vehicle").get<std::string>(),
                                  primary, secondary);
        ce.count = item.at("count").get<int>();
        comp.push_back(ce);
    }
    return comp;
}

// Contar vehiculos totales en una composicion
int total_count(const std::vector<CompositionEntry>& comp) {
    int n = 0;
    for (const auto& e : comp) n += e.count;
    return n;
}

// ---------------------------------------------------------------------------
// Ejecucion de un escenario completo (con encadenamiento)
// ---------------------------------------------------------------------------

struct ScenarioOutput {
    std::string scenario_id;
    std::vector<CombatResult> combats;
};

ScenarioOutput run_scenario(const json& scenario,
                            const VehicleCatalog& blue_cat,
                            const VehicleCatalog& red_cat) {
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

    double blue_survivors_prev = -1;
    double red_survivors_prev  = -1;
    double accumulated_time = 0;

    // Composicion que se arrastra entre combates (supervivientes + refuerzos)
    std::vector<CompositionEntry> blue_comp_carry, red_comp_carry;

    const auto& seq = scenario.at("combat_sequence");
    for (size_t ci = 0; ci < seq.size(); ++ci) {
        const auto& combat = seq[ci];
        int combat_id = combat.at("combat_id").get<int>();

        const auto& blue_j = combat.at("blue");
        const auto& red_j  = combat.at("red");

        // Leer composicion de este combate
        auto blue_comp_this = parse_composition(
            blue_j.at("composition"), blue_cat, red_cat);
        auto red_comp_this = parse_composition(
            red_j.at("composition"), red_cat, blue_cat);

        // Leer refuerzos
        std::vector<CompositionEntry> reinf_blue, reinf_red;
        if (combat.contains("reinforcements_blue"))
            reinf_blue = parse_composition(combat["reinforcements_blue"],
                                           blue_cat, red_cat);
        if (combat.contains("reinforcements_red"))
            reinf_red = parse_composition(combat["reinforcements_red"],
                                          red_cat, blue_cat);

        // Determinar composicion efectiva
        std::vector<CompositionEntry> blue_comp, red_comp;
        if (ci == 0) {
            // Primer combate: usar composicion directa
            blue_comp = blue_comp_this;
            red_comp  = red_comp_this;
        } else {
            // Combates 2+: supervivientes del anterior + composicion nueva + refuerzos
            blue_comp = blue_comp_carry;
            red_comp  = red_comp_carry;
            // Anadir composicion nueva de este combate
            for (auto& e : blue_comp_this) blue_comp.push_back(e);
            for (auto& e : red_comp_this)  red_comp.push_back(e);
            // Anadir refuerzos
            for (auto& e : reinf_blue) blue_comp.push_back(e);
            for (auto& e : reinf_red)  red_comp.push_back(e);
        }

        // Tiempo de desplazamiento
        double disp_dist = 0;
        if (combat.contains("displacement_distance_m"))
            disp_dist = combat["displacement_distance_m"].get<double>();

        Mobility mob_blue = parse_mobility(blue_j.at("mobility").get<std::string>());
        Mobility mob_red  = parse_mobility(red_j.at("mobility").get<std::string>());
        double t_disp = displacement_time_minutes(disp_dist, mob_blue, mob_red, terrain);

        // Construir input del combate
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

        // Override fuerzas iniciales para combates encadenados
        if (ci > 0 && blue_survivors_prev >= 0) {
            // Sumar refuerzos y nueva composicion al numero de supervivientes
            double extra_blue = static_cast<double>(
                total_count(blue_comp_this) + total_count(reinf_blue));
            double extra_red = static_cast<double>(
                total_count(red_comp_this) + total_count(reinf_red));
            cinput.blue_override_initial = blue_survivors_prev + extra_blue;
            cinput.red_override_initial  = red_survivors_prev + extra_red;
            // AFT del combate 2+ se aplican sobre el total
            cinput.blue_override_initial *= (1.0 - cinput.blue_aft_pct);
            cinput.red_override_initial  *= (1.0 - cinput.red_aft_pct);
            // engagement_fraction y count_factor para combates encadenados
            cinput.blue_override_initial *= cinput.blue_engagement_fraction
                                          * cinput.blue_count_factor;
            cinput.red_override_initial  *= cinput.red_engagement_fraction
                                          * cinput.red_count_factor;
        }

        CombatResult result = simulate_combat(cinput);

        accumulated_time += t_disp + result.duration_contact_minutes;
        result.duration_total_minutes = accumulated_time;

        // Guardar supervivientes para encadenamiento
        blue_survivors_prev = result.blue_survivors;
        red_survivors_prev  = result.red_survivors;

        // Arrastrar la composicion para agregacion en el siguiente combate
        // (mantener la misma composicion para que la agregacion sea correcta)
        blue_comp_carry = blue_comp;
        red_comp_carry  = red_comp;

        out.combats.push_back(result);
    }

    return out;
}

// ---------------------------------------------------------------------------
// Escritura de salida JSON
// ---------------------------------------------------------------------------

json result_to_json(const CombatResult& r) {
    json j;
    j["combat_id"]                = r.combat_id;
    j["outcome"]                  = outcome_str(r.outcome);
    j["duration_contact_minutes"] = std::round(r.duration_contact_minutes * 100) / 100;
    j["duration_total_minutes"]   = std::round(r.duration_total_minutes * 100) / 100;
    j["blue_initial"]             = std::round(r.blue_initial * 100) / 100;
    j["red_initial"]              = std::round(r.red_initial * 100) / 100;
    j["blue_survivors"]           = std::round(r.blue_survivors * 100) / 100;
    j["red_survivors"]            = std::round(r.red_survivors * 100) / 100;
    j["blue_casualties"]          = std::round(r.blue_casualties * 100) / 100;
    j["red_casualties"]           = std::round(r.red_casualties * 100) / 100;
    j["blue_ammo_consumed"]       = std::round(r.blue_ammo_consumed * 100) / 100;
    j["red_ammo_consumed"]        = std::round(r.red_ammo_consumed * 100) / 100;
    j["blue_cc_ammo_consumed"]    = std::round(r.blue_cc_ammo_consumed * 100) / 100;
    j["red_cc_ammo_consumed"]     = std::round(r.red_cc_ammo_consumed * 100) / 100;
    j["static_advantage"]         = std::round(r.static_advantage * 10000) / 10000;
    return j;
}

json scenario_to_json(const ScenarioOutput& out) {
    json j;
    j["scenario_id"] = out.scenario_id;
    j["combats"] = json::array();
    for (const auto& r : out.combats)
        j["combats"].push_back(result_to_json(r));
    return j;
}

// ---------------------------------------------------------------------------
// Localizacion del directorio del ejecutable (para catalogos)
// ---------------------------------------------------------------------------

std::string exe_directory(const char* argv0) {
    std::string path(argv0);
    auto pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return ".";
    return path.substr(0, pos);
}

// ---------------------------------------------------------------------------
// main — CLI
// ---------------------------------------------------------------------------

void print_usage(const char* prog) {
    std::fprintf(stderr,
        "Uso:\n"
        "  %s <escenario.json> [--output <resultado.json>] [--aggregation pre|post]\n"
        "\n"
        "Opciones:\n"
        "  --output <file>         Escribir resultado a fichero (por defecto: stdout)\n"
        "  --aggregation pre|post  Modo de agregacion (por defecto: pre)\n",
        prog);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string scenario_path;
    std::string output_path;

    // Parse CLI args
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--output" && i + 1 < argc) {
            output_path = argv[++i];
        } else if (arg == "--aggregation" && i + 1 < argc) {
            std::string mode = argv[++i];
            if (mode == "post") g_aggregation_mode = AggregationMode::POST;
            else                g_aggregation_mode = AggregationMode::PRE;
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg[0] != '-') {
            scenario_path = arg;
        } else {
            std::fprintf(stderr, "Opcion desconocida: %s\n", arg.c_str());
            print_usage(argv[0]);
            return 1;
        }
    }

    if (scenario_path.empty()) {
        std::fprintf(stderr, "Error: se requiere un fichero de escenario.\n");
        print_usage(argv[0]);
        return 1;
    }

    // Cargar catalogos desde el directorio del ejecutable
    std::string exe_dir = exe_directory(argv[0]);
    VehicleCatalog blue_cat = load_catalog(exe_dir + "/vehicle_db.json");
    VehicleCatalog red_cat  = load_catalog(exe_dir + "/vehicle_db_en.json");

    if (blue_cat.empty() && red_cat.empty()) {
        std::fprintf(stderr, "Aviso: no se encontraron catalogos de vehiculos en '%s'.\n",
                     exe_dir.c_str());
    }

    // Leer escenario
    std::ifstream ifs(scenario_path);
    if (!ifs.is_open()) {
        std::fprintf(stderr, "Error: no se pudo abrir '%s'.\n", scenario_path.c_str());
        return 1;
    }

    json scenario;
    try {
        scenario = json::parse(ifs);
    } catch (const json::parse_error& e) {
        std::fprintf(stderr, "Error JSON en '%s': %s\n",
                     scenario_path.c_str(), e.what());
        return 1;
    }

    // Ejecutar escenario
    ScenarioOutput result = run_scenario(scenario, blue_cat, red_cat);

    // Escribir salida
    json output_json = scenario_to_json(result);
    std::string output_str = output_json.dump(2);

    if (output_path.empty()) {
        std::cout << output_str << std::endl;
    } else {
        std::ofstream ofs(output_path);
        if (!ofs.is_open()) {
            std::fprintf(stderr, "Error: no se pudo escribir '%s'.\n",
                         output_path.c_str());
            return 1;
        }
        ofs << output_str << std::endl;
        std::fprintf(stderr, "Resultado escrito en '%s'.\n", output_path.c_str());
    }

    return 0;
}
