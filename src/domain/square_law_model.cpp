// square_law_model.cpp — Implementacion del modelo Lanchester ley cuadrada
#include "square_law_model.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

SquareLawModel::SquareLawModel(const ModelParamsClass& params)
    : params_(params) {}

// ---------------------------------------------------------------------------
// Funciones internas del modelo
// ---------------------------------------------------------------------------

double SquareLawModel::killProbability(double D_target, double P_attacker) const {
    return 1.0 / (1.0 + std::exp(
        (D_target - P_attacker) / params_.killProbabilitySlope()));
}

double SquareLawModel::distanceDegradation(double d, double f_dist, double range_max) const {
    if (d > range_max) return 0.0;
    const auto& c = params_.distCoeffs();
    double dk = d / 1000.0;
    double g = c.c_dk * dk
             + c.c_f * f_dist
             + c.c_dk2 * dk * dk
             + c.c_dk_f * dk * f_dist
             + c.c_f2 * f_dist * f_dist
             + c.c_const;
    return std::clamp(g, 0.0, 1.0);
}

double SquareLawModel::staticRateConv(double T, double G, double U, double c) const {
    return T * G * U * c;
}

double SquareLawModel::staticRateCc(double T_cc, double G_cc, double c_cc) const {
    return c_cc * T_cc * G_cc;
}

double SquareLawModel::dynamicRateCc(double S_cc_static, double A_current, double A0,
                                      double cc_ammo_consumed, double cc_ammo_max) const {
    if (cc_ammo_max <= 0.0 || A0 <= 0.0) return 0.0;
    double ammo_remaining_frac = std::max(0.0, cc_ammo_max - cc_ammo_consumed) / cc_ammo_max;
    return (A_current / A0) * S_cc_static * ammo_remaining_frac;
}

TacticalMult SquareLawModel::getTacticalMultipliers(const std::string& state) const {
    return params_.tacticalMult(state);
}

double SquareLawModel::terrainFireMultiplier(Terrain ter) const {
    return params_.terrainFireMult(ter);
}

// ---------------------------------------------------------------------------
// Agregacion
// ---------------------------------------------------------------------------

AggregatedParams SquareLawModel::aggregate(const std::vector<CompositionEntry>& composition) {
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

EffectiveRates SquareLawModel::computeEffectiveRates(
    const AggregatedParams& attacker, const AggregatedParams& defender,
    double distance_m, const std::string& att_state,
    const std::string& def_state, double rate_factor) const
{
    EffectiveRates er{};
    er.rate_factor = rate_factor;
    er.c_agg    = attacker.c;
    er.c_cc_agg = attacker.c_cc;
    er.M_agg    = attacker.M;
    er.has_cc   = attacker.has_cc;
    er.n_cc     = attacker.n_cc;
    er.n_total  = attacker.n_total;

    double T = killProbability(defender.D, attacker.P);
    double G = distanceDegradation(distance_m, attacker.f, attacker.A_max);
    double S = staticRateConv(T, G, attacker.U, attacker.c);

    TacticalMult am = getTacticalMultipliers(att_state);
    TacticalMult dm = getTacticalMultipliers(def_state);
    er.S_conv = S * am.self_mult * dm.opponent_mult * rate_factor;

    if (attacker.has_cc) {
        double Tc = killProbability(defender.D_cc, attacker.P_cc);
        double Gc = distanceDegradation(distance_m, attacker.f_cc, attacker.A_cc);
        er.S_cc_static = staticRateCc(Tc, Gc, attacker.c_cc);
    }
    return er;
}

EffectiveRates SquareLawModel::computeEffectiveRatesPost(
    const std::vector<CompositionEntry>& att_comp,
    const AggregatedParams& defender,
    double distance_m, const std::string& att_state,
    const std::string& def_state, double rate_factor) const
{
    EffectiveRates er{};
    er.rate_factor = rate_factor;

    TacticalMult am = getTacticalMultipliers(att_state);
    TacticalMult dm = getTacticalMultipliers(def_state);
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

        double T = killProbability(defender.D, v.P);
        double G = distanceDegradation(distance_m, v.f, v.A_max);
        double S = staticRateConv(T, G, v.U, v.c);
        weighted_S_conv += n * S;
        weighted_c      += n * v.c;

        if (v.CC) {
            n_cc += n;
            double Tc = killProbability(defender.D_cc, v.P_cc);
            double Gc = distanceDegradation(distance_m, v.f_cc, v.A_cc);
            double Scc = staticRateCc(Tc, Gc, v.c_cc);
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

double SquareLawModel::totalRate(const EffectiveRates& er,
                                  double N_att, double N_att0,
                                  double cc_ammo_consumed, double cc_ammo_max) const {
    double s_cc = 0.0;
    if (er.has_cc && er.S_cc_static > 0.0)
        s_cc = dynamicRateCc(er.S_cc_static, N_att, N_att0,
                              cc_ammo_consumed, cc_ammo_max) * er.rate_factor;
    return er.S_conv + s_cc;
}

double SquareLawModel::initialForces(int n_total, double aft_pct,
                                      double eng_frac, double cnt_fac) {
    double n = static_cast<double>(n_total);
    return (n - n * aft_pct) * eng_frac * cnt_fac;
}

int SquareLawModel::totalCount(const std::vector<CompositionEntry>& comp) {
    int n = 0;
    for (const auto& e : comp) n += e.count;
    return n;
}

void SquareLawModel::distributeCasualtiesByVulnerability(
    std::vector<CompositionEntry>& comp, double total_casualties)
{
    if (comp.empty() || total_casualties <= 0.0) return;
    int n_total = totalCount(comp);
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
// Simulacion determinista (RK4)
// ---------------------------------------------------------------------------

CombatResult SquareLawModel::simulate(const CombatInput& input) const {
    AggregatedParams blue_agg = aggregate(input.blue_composition);
    AggregatedParams red_agg  = aggregate(input.red_composition);

    double A0, R0;
    if (input.blue_override_initial >= 0)
        A0 = input.blue_override_initial;
    else
        A0 = initialForces(blue_agg.n_total, input.blue_aft_pct,
                           input.blue_engagement_fraction, input.blue_count_factor);

    if (input.red_override_initial >= 0)
        R0 = input.red_override_initial;
    else
        R0 = initialForces(red_agg.n_total, input.red_aft_pct,
                           input.red_engagement_fraction, input.red_count_factor);

    double terrain_mult = terrainFireMultiplier(input.terrain);
    double approach_speed_m_per_min = input.approach_speed_kmh * 1000.0 / 60.0;

    auto compute_rates_at_distance = [&](double dist) {
        EffectiveRates br, rr;
        if (input.aggregation_mode == AggregationMode::POST) {
            br = computeEffectiveRatesPost(
                input.blue_composition, red_agg, dist,
                input.blue_state, input.red_state, input.blue_rate_factor);
            rr = computeEffectiveRatesPost(
                input.red_composition, blue_agg, dist,
                input.red_state, input.blue_state, input.red_rate_factor);
        } else {
            br = computeEffectiveRates(
                blue_agg, red_agg, dist,
                input.blue_state, input.red_state, input.blue_rate_factor);
            rr = computeEffectiveRates(
                red_agg, blue_agg, dist,
                input.red_state, input.blue_state, input.red_rate_factor);
        }
        br.S_conv *= terrain_mult;
        rr.S_conv *= terrain_mult;
        return std::make_pair(br, rr);
    };

    auto [blue_rates, red_rates] = compute_rates_at_distance(input.distance_m);

    double S_blue_t0 = totalRate(blue_rates, A0, A0, 0.0, 1.0);
    double S_red_t0  = totalRate(red_rates,  R0, R0, 0.0, 1.0);
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

    auto f_A = [&](double, double, double Ri) {
        return -totalRate(red_rates, Ri, R0, red_cc_ammo, red_cc_max) * Ri;
    };
    auto f_R = [&](double, double Ai, double) {
        return -totalRate(blue_rates, Ai, A0, blue_cc_ammo, blue_cc_max) * Ai;
    };

    double h = input.h;
    while (A > 0.0 && R > 0.0 && t < input.t_max) {
        if (approach_speed_m_per_min > 0.0) {
            double current_distance = std::max(50.0,
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

// ---------------------------------------------------------------------------
// Simulacion estocastica (Monte Carlo con Poisson)
// ---------------------------------------------------------------------------

CombatResult SquareLawModel::simulateStochastic(const CombatInput& input,
                                                 std::mt19937& rng) const {
    AggregatedParams blue_agg = aggregate(input.blue_composition);
    AggregatedParams red_agg  = aggregate(input.red_composition);

    double A0_real, R0_real;
    if (input.blue_override_initial >= 0)
        A0_real = input.blue_override_initial;
    else
        A0_real = initialForces(blue_agg.n_total, input.blue_aft_pct,
                                input.blue_engagement_fraction, input.blue_count_factor);

    if (input.red_override_initial >= 0)
        R0_real = input.red_override_initial;
    else
        R0_real = initialForces(red_agg.n_total, input.red_aft_pct,
                                input.red_engagement_fraction, input.red_count_factor);

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

    double terrain_mult = terrainFireMultiplier(input.terrain);
    double approach_speed_m_per_min = input.approach_speed_kmh * 1000.0 / 60.0;

    auto compute_rates_at_distance = [&](double dist) {
        EffectiveRates br, rr;
        if (input.aggregation_mode == AggregationMode::POST) {
            br = computeEffectiveRatesPost(
                input.blue_composition, red_agg, dist,
                input.blue_state, input.red_state, input.blue_rate_factor);
            rr = computeEffectiveRatesPost(
                input.red_composition, blue_agg, dist,
                input.red_state, input.blue_state, input.red_rate_factor);
        } else {
            br = computeEffectiveRates(
                blue_agg, red_agg, dist,
                input.blue_state, input.red_state, input.blue_rate_factor);
            rr = computeEffectiveRates(
                red_agg, blue_agg, dist,
                input.red_state, input.blue_state, input.red_rate_factor);
        }
        br.S_conv *= terrain_mult;
        rr.S_conv *= terrain_mult;
        return std::make_pair(br, rr);
    };

    auto [blue_rates, red_rates] = compute_rates_at_distance(input.distance_m);

    double S_blue_t0 = totalRate(blue_rates, A0d, A0d, 0.0, 1.0);
    double S_red_t0  = totalRate(red_rates,  R0d, R0d, 0.0, 1.0);
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

        double lambda_blue = totalRate(red_rates, Rd, R0d, red_cc_ammo, red_cc_max) * Rd * h;
        double lambda_red  = totalRate(blue_rates, Ad, A0d, blue_cc_ammo, blue_cc_max) * Ad * h;

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
// Estadisticas y Monte Carlo
// ---------------------------------------------------------------------------

PercentileStats SquareLawModel::computeStats(std::vector<double>& data) const {
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

MonteCarloResult SquareLawModel::runMonteCarlo(const CombatInput& input,
                                                int n_replicas,
                                                std::mt19937& rng) const {
    MonteCarloResult mc;
    mc.combat_id  = input.combat_id;
    mc.n_replicas = n_replicas;

    mc.deterministic = simulate(input);

    std::vector<double> blue_surv(n_replicas);
    std::vector<double> red_surv(n_replicas);
    std::vector<double> durations(n_replicas);

    for (int i = 0; i < n_replicas; ++i) {
        CombatResult r = simulateStochastic(input, rng);
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

    mc.blue_survivors = computeStats(blue_surv);
    mc.red_survivors  = computeStats(red_surv);
    mc.duration       = computeStats(durations);
    return mc;
}
