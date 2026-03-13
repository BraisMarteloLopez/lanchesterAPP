// lanchester_model.h — Funciones matematicas, agregacion y simulacion
#pragma once

#include "lanchester_types.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <utility>

// ---------------------------------------------------------------------------
// Funciones del modelo
// ---------------------------------------------------------------------------

inline double kill_probability(double D_target, double P_attacker) {
    return 1.0 / (1.0 + std::exp(
        (D_target - P_attacker) / g_model_params.kill_probability_slope));
}

inline double distance_degradation(double d, double f_dist, double range_max) {
    if (d > range_max) return 0.0;
    const auto& c = g_model_params.dist_coeff;
    double dk = d / 1000.0;
    double g = c.c_dk * dk
             + c.c_f * f_dist
             + c.c_dk2 * dk * dk
             + c.c_dk_f * dk * f_dist
             + c.c_f2 * f_dist * f_dist
             + c.c_const;
    return std::clamp(g, 0.0, 1.0);
}

inline double static_rate_conv(double T, double G, double U, double c) {
    return T * G * U * c;
}

inline double static_rate_cc(double T_cc, double G_cc, double c_cc) {
    return c_cc * T_cc * G_cc;
}

inline double dynamic_rate_cc(double S_cc_static, double A_current, double A0,
                              double cc_ammo_consumed, double cc_ammo_max) {
    if (cc_ammo_max <= 0.0 || A0 <= 0.0) return 0.0;
    double ammo_remaining_frac = std::max(0.0, cc_ammo_max - cc_ammo_consumed) / cc_ammo_max;
    return (A_current / A0) * S_cc_static * ammo_remaining_frac;
}

// ---------------------------------------------------------------------------
// Multiplicadores tacticos
// ---------------------------------------------------------------------------

inline TacticalMult get_tactical_multipliers(const std::string& state) {
    // Buscar en parametros cargados desde model_params.json
    const auto& tm = g_model_params.tactical_multipliers;
    auto it = tm.find(state);
    if (it != tm.end())
        return {it->second.self_mult, it->second.opponent_mult};

    // Fallback hardcoded si no hay parametros cargados
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
// Velocidades tacticas y terreno
// ---------------------------------------------------------------------------

inline Mobility parse_mobility(const std::string& s) {
    if (s == "MUY_ALTA") return Mobility::MUY_ALTA;
    if (s == "ALTA")     return Mobility::ALTA;
    if (s == "MEDIA")    return Mobility::MEDIA;
    return Mobility::BAJA;
}

inline Terrain parse_terrain(const std::string& s) {
    if (s == "FACIL")    return Terrain::FACIL;
    if (s == "DIFICIL")  return Terrain::DIFICIL;
    return Terrain::MEDIO;
}

inline double terrain_fire_multiplier(Terrain ter) {
    switch (ter) {
        case Terrain::FACIL:   return g_model_params.terrain_fire_mult_facil;
        case Terrain::MEDIO:   return g_model_params.terrain_fire_mult_medio;
        case Terrain::DIFICIL: return g_model_params.terrain_fire_mult_dificil;
    }
    return 1.0;
}

inline double tactical_speed(Mobility mob, Terrain ter) {
    static const double table[4][3] = {
        {40, 25, 12},
        {30, 20, 10},
        {20, 12,  6},
        {10,  6,  3},
    };
    return table[static_cast<int>(mob)][static_cast<int>(ter)];
}

inline double displacement_time_minutes(double distance_m, Mobility mob_blue,
                                        Mobility mob_red, Terrain terrain) {
    double v_blue = tactical_speed(mob_blue, terrain);
    double v_red  = tactical_speed(mob_red, terrain);
    double v_min  = std::min(v_blue, v_red);
    if (v_min <= 0.0 || distance_m <= 0.0) return 0.0;
    return (distance_m / 1000.0) * 60.0 / v_min;
}

// ---------------------------------------------------------------------------
// Agregacion de fuerzas mixtas
// ---------------------------------------------------------------------------

inline AggregatedParams aggregate(const std::vector<CompositionEntry>& composition) {
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

inline EffectiveRates compute_effective_rates(
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

inline EffectiveRates compute_effective_rates_post(
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

inline double total_rate(const EffectiveRates& er,
                         double N_att, double N_att0,
                         double cc_ammo_consumed, double cc_ammo_max) {
    double s_cc = 0.0;
    if (er.has_cc && er.S_cc_static > 0.0)
        s_cc = dynamic_rate_cc(er.S_cc_static, N_att, N_att0,
                               cc_ammo_consumed, cc_ammo_max) * er.rate_factor;
    return er.S_conv + s_cc;
}

inline double initial_forces(int n_total, double aft_pct, double eng_frac, double cnt_fac) {
    double n = static_cast<double>(n_total);
    return (n - n * aft_pct) * eng_frac * cnt_fac;
}

// ---------------------------------------------------------------------------
// Simulacion de un combate individual (integrador RK4)
// ---------------------------------------------------------------------------

inline CombatResult simulate_combat(const CombatInput& input) {
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

    double terrain_mult = terrain_fire_multiplier(input.terrain);
    double approach_speed_m_per_min = input.approach_speed_kmh * 1000.0 / 60.0;

    auto compute_rates_at_distance = [&](double dist) {
        EffectiveRates br, rr;
        if (input.aggregation_mode == AggregationMode::POST) {
            br = compute_effective_rates_post(
                input.blue_composition, red_agg, dist,
                input.blue_state, input.red_state, input.blue_rate_factor);
            rr = compute_effective_rates_post(
                input.red_composition, blue_agg, dist,
                input.red_state, input.blue_state, input.red_rate_factor);
        } else {
            br = compute_effective_rates(
                blue_agg, red_agg, dist,
                input.blue_state, input.red_state, input.blue_rate_factor);
            rr = compute_effective_rates(
                red_agg, blue_agg, dist,
                input.red_state, input.blue_state, input.red_rate_factor);
        }
        br.S_conv *= terrain_mult;
        rr.S_conv *= terrain_mult;
        return std::make_pair(br, rr);
    };

    auto [blue_rates, red_rates] = compute_rates_at_distance(input.distance_m);

    double S_blue_t0 = total_rate(blue_rates, A0, A0, 0.0, 1.0);
    double S_red_t0  = total_rate(red_rates,  R0, R0, 0.0, 1.0);
    double static_adv = 0.0;
    if (S_red_t0 * R0 * R0 > 0.0)
        static_adv = (S_blue_t0 * A0 * A0) / (S_red_t0 * R0 * R0);

    double A = A0, R = R0, t = 0.0;
    double blue_ammo = 0, red_ammo = 0, blue_cc_ammo = 0, red_cc_ammo = 0;
    double current_distance = input.distance_m;

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

    auto f_A = [&](double /*ti*/, double /*Ai*/, double Ri) {
        return -total_rate(red_rates, Ri, R0, red_cc_ammo, red_cc_max) * Ri;
    };
    auto f_R = [&](double /*ti*/, double Ai, double /*Ri*/) {
        return -total_rate(blue_rates, Ai, A0, blue_cc_ammo, blue_cc_max) * Ai;
    };

    double h = input.h;
    while (A > 0.0 && R > 0.0 && t < input.t_max) {
        if (approach_speed_m_per_min > 0.0) {
            current_distance = std::max(50.0,
                input.distance_m - approach_speed_m_per_min * t);
            auto [br, rr] = compute_rates_at_distance(current_distance);
            blue_rates = br;
            red_rates = rr;
        }

        double k1a = f_A(t, A, R);
        double k1r = f_R(t, A, R);
        double A2 = std::max(0.0, A + 0.5 * h * k1a);
        double R2 = std::max(0.0, R + 0.5 * h * k1r);
        double k2a = f_A(t + 0.5 * h, A2, R2);
        double k2r = f_R(t + 0.5 * h, A2, R2);
        double A3 = std::max(0.0, A + 0.5 * h * k2a);
        double R3 = std::max(0.0, R + 0.5 * h * k2r);
        double k3a = f_A(t + 0.5 * h, A3, R3);
        double k3r = f_R(t + 0.5 * h, A3, R3);
        double A4 = std::max(0.0, A + h * k3a);
        double R4 = std::max(0.0, R + h * k3r);
        double k4a = f_A(t + h, A4, R4);
        double k4r = f_R(t + h, A4, R4);

        double A_new = std::max(0.0, A + (h / 6.0) * (k1a + 2*k2a + 2*k3a + k4a));
        double R_new = std::max(0.0, R + (h / 6.0) * (k1r + 2*k2r + 2*k3r + k4r));

        blue_ammo += blue_rates.c_agg * A * h;
        red_ammo  += red_rates.c_agg  * R * h;
        if (blue_rates.has_cc)
            blue_cc_ammo = std::min(blue_cc_ammo +
                blue_rates.c_cc_agg * A * cc_ratio_blue * h, blue_cc_max);
        if (red_rates.has_cc)
            red_cc_ammo = std::min(red_cc_ammo +
                red_rates.c_cc_agg * R * cc_ratio_red * h, red_cc_max);

        A = A_new; R = R_new; t += h;
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
    res.duration_total_minutes   = t;
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

inline const char* outcome_str(Outcome o) {
    switch (o) {
        case Outcome::BLUE_WINS:     return "BLUE_WINS";
        case Outcome::RED_WINS:      return "RED_WINS";
        case Outcome::DRAW:          return "DRAW";
        case Outcome::INDETERMINATE: return "INDETERMINATE";
    }
    return "UNKNOWN";
}

inline int total_count(const std::vector<CompositionEntry>& comp) {
    int n = 0;
    for (const auto& e : comp) n += e.count;
    return n;
}

inline void distribute_casualties_by_vulnerability(
    std::vector<CompositionEntry>& comp, double total_casualties)
{
    if (comp.empty() || total_casualties <= 0.0) return;
    int n_total = total_count(comp);
    if (n_total <= 0) return;

    std::vector<double> vulnerability(comp.size());
    double vuln_sum = 0;
    for (size_t i = 0; i < comp.size(); ++i) {
        vulnerability[i] = comp[i].count / (comp[i].vehicle.D_cc + 1.0);
        vuln_sum += vulnerability[i];
    }
    if (vuln_sum <= 0.0) {
        double frac = std::max(0.0, 1.0 - total_casualties / n_total);
        for (auto& e : comp)
            e.count = std::max(0, static_cast<int>(std::round(e.count * frac)));
        return;
    }

    double remaining_casualties = total_casualties;
    for (size_t i = 0; i < comp.size(); ++i) {
        double share = total_casualties * (vulnerability[i] / vuln_sum);
        int losses = std::min(comp[i].count,
                              static_cast<int>(std::round(share)));
        comp[i].count -= losses;
        remaining_casualties -= losses;
    }
    while (remaining_casualties >= 1.0) {
        for (size_t i = 0; i < comp.size() && remaining_casualties >= 1.0; ++i) {
            if (comp[i].count > 0) {
                comp[i].count--;
                remaining_casualties -= 1.0;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Simulacion estocastica (Monte Carlo con Poisson)
// ---------------------------------------------------------------------------

inline CombatResult simulate_combat_stochastic(const CombatInput& input,
                                                std::mt19937& rng) {
    AggregatedParams blue_agg = aggregate(input.blue_composition);
    AggregatedParams red_agg  = aggregate(input.red_composition);

    double A0_real, R0_real;
    if (input.blue_override_initial >= 0)
        A0_real = input.blue_override_initial;
    else
        A0_real = initial_forces(blue_agg.n_total, input.blue_aft_pct,
                                 input.blue_engagement_fraction, input.blue_count_factor);

    if (input.red_override_initial >= 0)
        R0_real = input.red_override_initial;
    else
        R0_real = initial_forces(red_agg.n_total, input.red_aft_pct,
                                 input.red_engagement_fraction, input.red_count_factor);

    // Fuerzas discretas (enteros)
    int A = static_cast<int>(std::round(A0_real));
    int R = static_cast<int>(std::round(R0_real));
    int A0 = A, R0_int = R;
    double A0d = static_cast<double>(A0);
    double R0d = static_cast<double>(R0_int);

    if (A <= 0 || R <= 0) {
        CombatResult res;
        res.combat_id = input.combat_id;
        res.blue_initial = A0d;
        res.red_initial  = R0d;
        res.blue_survivors = static_cast<double>(A);
        res.red_survivors  = static_cast<double>(R);
        res.blue_casualties = A0d - res.blue_survivors;
        res.red_casualties  = R0d - res.red_survivors;
        if (A <= 0 && R <= 0)      res.outcome = Outcome::DRAW;
        else if (A <= 0)           res.outcome = Outcome::RED_WINS;
        else                       res.outcome = Outcome::BLUE_WINS;
        return res;
    }

    double terrain_mult = terrain_fire_multiplier(input.terrain);
    double approach_speed_m_per_min = input.approach_speed_kmh * 1000.0 / 60.0;

    auto compute_rates_at_distance = [&](double dist) {
        EffectiveRates br, rr;
        if (input.aggregation_mode == AggregationMode::POST) {
            br = compute_effective_rates_post(
                input.blue_composition, red_agg, dist,
                input.blue_state, input.red_state, input.blue_rate_factor);
            rr = compute_effective_rates_post(
                input.red_composition, blue_agg, dist,
                input.red_state, input.blue_state, input.red_rate_factor);
        } else {
            br = compute_effective_rates(
                blue_agg, red_agg, dist,
                input.blue_state, input.red_state, input.blue_rate_factor);
            rr = compute_effective_rates(
                red_agg, blue_agg, dist,
                input.red_state, input.blue_state, input.red_rate_factor);
        }
        br.S_conv *= terrain_mult;
        rr.S_conv *= terrain_mult;
        return std::make_pair(br, rr);
    };

    auto [blue_rates, red_rates] = compute_rates_at_distance(input.distance_m);

    // Static advantage (same as deterministic)
    double S_blue_t0 = total_rate(blue_rates, A0d, A0d, 0.0, 1.0);
    double S_red_t0  = total_rate(red_rates,  R0d, R0d, 0.0, 1.0);
    double static_adv = 0.0;
    if (S_red_t0 * R0d * R0d > 0.0)
        static_adv = (S_blue_t0 * A0d * A0d) / (S_red_t0 * R0d * R0d);

    double blue_cc_max = blue_rates.has_cc
        ? blue_rates.M_agg * std::max(0.0, A0d * (blue_agg.n_cc > 0
            ? static_cast<double>(blue_agg.n_cc) / blue_agg.n_total : 0.0))
        : 0.0;
    double red_cc_max = red_rates.has_cc
        ? red_rates.M_agg * std::max(0.0, R0d * (red_agg.n_cc > 0
            ? static_cast<double>(red_agg.n_cc) / red_agg.n_total : 0.0))
        : 0.0;

    double cc_ratio_blue = (blue_agg.n_total > 0)
        ? static_cast<double>(blue_agg.n_cc) / blue_agg.n_total : 0.0;
    double cc_ratio_red = (red_agg.n_total > 0)
        ? static_cast<double>(red_agg.n_cc) / red_agg.n_total : 0.0;

    double blue_ammo = 0, red_ammo = 0, blue_cc_ammo = 0, red_cc_ammo = 0;
    double t = 0.0;
    double h = input.h;

    while (A > 0 && R > 0 && t < input.t_max) {
        if (approach_speed_m_per_min > 0.0) {
            double current_distance = std::max(50.0,
                input.distance_m - approach_speed_m_per_min * t);
            auto [br, rr] = compute_rates_at_distance(current_distance);
            blue_rates = br;
            red_rates = rr;
        }

        double Ad = static_cast<double>(A);
        double Rd = static_cast<double>(R);

        // Tasa de bajas esperadas por paso
        double lambda_blue = total_rate(red_rates, Rd, R0d, red_cc_ammo, red_cc_max) * Rd * h;
        double lambda_red  = total_rate(blue_rates, Ad, A0d, blue_cc_ammo, blue_cc_max) * Ad * h;

        // Subdivision automatica si lambda > 2 (evitar distorsion de Poisson)
        int sub_steps = 1;
        double max_lambda = std::max(lambda_blue, lambda_red);
        if (max_lambda > 2.0) {
            sub_steps = static_cast<int>(std::ceil(max_lambda / 1.5));
            lambda_blue /= sub_steps;
            lambda_red  /= sub_steps;
        }

        for (int ss = 0; ss < sub_steps && A > 0 && R > 0; ++ss) {
            int blue_losses = 0, red_losses = 0;
            if (lambda_blue > 0.0) {
                std::poisson_distribution<int> dist_b(lambda_blue);
                blue_losses = std::min(dist_b(rng), A);
            }
            if (lambda_red > 0.0) {
                std::poisson_distribution<int> dist_r(lambda_red);
                red_losses = std::min(dist_r(rng), R);
            }
            A -= blue_losses;
            R -= red_losses;
        }

        // Ammo tracking (approximation — same as deterministic)
        blue_ammo += blue_rates.c_agg * Ad * h;
        red_ammo  += red_rates.c_agg  * Rd * h;
        if (blue_rates.has_cc)
            blue_cc_ammo = std::min(blue_cc_ammo +
                blue_rates.c_cc_agg * Ad * cc_ratio_blue * h, blue_cc_max);
        if (red_rates.has_cc)
            red_cc_ammo = std::min(red_cc_ammo +
                red_rates.c_cc_agg * Rd * cc_ratio_red * h, red_cc_max);

        t += h;
    }

    Outcome outcome;
    if (A <= 0 && R <= 0)    outcome = Outcome::DRAW;
    else if (A <= 0)         outcome = Outcome::RED_WINS;
    else if (R <= 0)         outcome = Outcome::BLUE_WINS;
    else                     outcome = Outcome::INDETERMINATE;

    CombatResult res;
    res.combat_id               = input.combat_id;
    res.outcome                 = outcome;
    res.duration_contact_minutes = t;
    res.duration_total_minutes   = t;
    res.blue_initial            = A0d;
    res.red_initial             = R0d;
    res.blue_survivors          = static_cast<double>(std::max(0, A));
    res.red_survivors           = static_cast<double>(std::max(0, R));
    res.blue_casualties         = A0d - res.blue_survivors;
    res.red_casualties          = R0d - res.red_survivors;
    res.blue_ammo_consumed      = blue_ammo;
    res.red_ammo_consumed       = red_ammo;
    res.blue_cc_ammo_consumed   = blue_cc_ammo;
    res.red_cc_ammo_consumed    = red_cc_ammo;
    res.static_advantage        = static_adv;
    return res;
}

// ---------------------------------------------------------------------------
// Estadisticas de Monte Carlo
// ---------------------------------------------------------------------------

inline PercentileStats compute_stats(std::vector<double>& data) {
    PercentileStats ps;
    if (data.empty()) return ps;
    std::sort(data.begin(), data.end());
    int n = static_cast<int>(data.size());

    double sum = std::accumulate(data.begin(), data.end(), 0.0);
    ps.mean = sum / n;

    double sq_sum = 0;
    for (double d : data) sq_sum += (d - ps.mean) * (d - ps.mean);
    ps.std = (n > 1) ? std::sqrt(sq_sum / (n - 1)) : 0.0;

    auto pct = [&](double p) -> double {
        double idx = p * (n - 1);
        int lo = static_cast<int>(std::floor(idx));
        int hi = std::min(lo + 1, n - 1);
        double frac = idx - lo;
        return data[lo] * (1.0 - frac) + data[hi] * frac;
    };

    ps.p05    = pct(0.05);
    ps.p25    = pct(0.25);
    ps.median = pct(0.50);
    ps.p75    = pct(0.75);
    ps.p95    = pct(0.95);
    return ps;
}

inline MonteCarloResult run_montecarlo_combat(const CombatInput& input,
                                               int n_replicas, std::mt19937& rng) {
    MonteCarloResult mc;
    mc.combat_id  = input.combat_id;
    mc.n_replicas = n_replicas;

    // Deterministic reference
    mc.deterministic = simulate_combat(input);

    std::vector<double> blue_surv(n_replicas);
    std::vector<double> red_surv(n_replicas);
    std::vector<double> durations(n_replicas);

    for (int i = 0; i < n_replicas; ++i) {
        CombatResult r = simulate_combat_stochastic(input, rng);
        blue_surv[i] = r.blue_survivors;
        red_surv[i]  = r.red_survivors;
        durations[i] = r.duration_contact_minutes;

        switch (r.outcome) {
            case Outcome::BLUE_WINS:     ++mc.count_blue_wins; break;
            case Outcome::RED_WINS:      ++mc.count_red_wins; break;
            case Outcome::DRAW:          ++mc.count_draw; break;
            case Outcome::INDETERMINATE: ++mc.count_indeterminate; break;
        }
    }

    mc.blue_survivors = compute_stats(blue_surv);
    mc.red_survivors  = compute_stats(red_surv);
    mc.duration       = compute_stats(durations);
    return mc;
}
