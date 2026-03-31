// square_law_model.cpp — Implementacion del modelo Lanchester ley cuadrada
#include "square_law_model.h"
#include "combat_utils.h"
#include "model_factory.h"

#include <algorithm>
#include <cmath>
#include <utility>

// Auto-registro en el factory global
static const bool registered = ModelFactory::instance().registerModel(
    "Lanchester Square Law (Euler)",
    [](std::shared_ptr<const IModelParams> p) -> std::shared_ptr<IStochasticModel> {
        return std::make_shared<SquareLawModel>(std::move(p));
    });

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

SquareLawModel::SquareLawModel(std::shared_ptr<const IModelParams> params)
    : params_(std::move(params)) {}

// ---------------------------------------------------------------------------
// Funciones internas del modelo
// ---------------------------------------------------------------------------

double SquareLawModel::killProbability(double D_target, double P_attacker) const {
    return 1.0 / (1.0 + std::exp(
        (D_target - P_attacker) / params_->killProbabilitySlope()));
}

double SquareLawModel::distanceDegradation(double d, double f_dist, double range_max) const {
    if (d > range_max) return 0.0;
    const auto& c = params_->distCoeffs();
    double dk = d / lanchester::METERS_PER_KM;
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
    double rate = (A_current / A0) * S_cc_static * ammo_remaining_frac;
    return std::clamp(rate, -10.0, 10.0);
}

TacticalMult SquareLawModel::getTacticalMultipliers(const std::string& state) const {
    return params_->tacticalMult(state);
}

double SquareLawModel::terrainFireMultiplier(Terrain ter) const {
    return params_->terrainFireMult(ter);
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
    double weighted_S_conv = 0, weighted_S_cc = 0;
    double weighted_c = 0, weighted_c_cc = 0, weighted_M = 0;

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

// ---------------------------------------------------------------------------
// Simulacion determinista (Euler explicito)
// ---------------------------------------------------------------------------

CombatResult SquareLawModel::simulate(const CombatInput& input) const {
    AggregatedParams blue_agg = lanchester::aggregate(input.blue_composition);
    AggregatedParams red_agg  = lanchester::aggregate(input.red_composition);

    double A0, R0;
    if (input.blue_override_initial >= 0)
        A0 = input.blue_override_initial;
    else
        A0 = lanchester::initialForces(blue_agg.n_total, input.blue_aft_pct,
                                       input.blue_engagement_fraction);

    if (input.red_override_initial >= 0)
        R0 = input.red_override_initial;
    else
        R0 = lanchester::initialForces(red_agg.n_total, input.red_aft_pct,
                                       input.red_engagement_fraction);

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

    // Serie temporal: punto inicial
    std::vector<TimeStep> time_series;
    time_series.push_back({t, A, R});

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

    double h = input.h;
    while (A > 0.0 && R > 0.0 && t < input.t_max) {
        if (approach_speed_m_per_min > 0.0) {
            double current_distance = std::max(lanchester::MIN_ENGAGEMENT_DISTANCE_M,
                input.distance_m - approach_speed_m_per_min * t);
            auto [br, rr] = compute_rates_at_distance(current_distance);
            blue_rates = br;
            red_rates = rr;
        }

        // Euler explicito (docx §85, Anexo 2)
        double dA = -totalRate(red_rates, R, R0, red_cc_ammo, red_cc_max) * R;
        double dR = -totalRate(blue_rates, A, A0, blue_cc_ammo, blue_cc_max) * A;
        double A_new = std::max(0.0, A + h * dA);
        double R_new = std::max(0.0, R + h * dR);

        blue_ammo += blue_rates.c_agg * A * h;
        red_ammo  += red_rates.c_agg  * R * h;
        if (blue_rates.has_cc)
            blue_cc_ammo = std::min(blue_cc_ammo +
                blue_rates.c_cc_agg * A * cc_ratio_blue * h, blue_cc_max);
        if (red_rates.has_cc)
            red_cc_ammo = std::min(red_cc_ammo +
                red_rates.c_cc_agg * R * cc_ratio_red * h, red_cc_max);

        A = A_new; R = R_new; t += h;
        time_series.push_back({t, std::max(0.0, A), std::max(0.0, R)});
    }

    Outcome outcome;
    if (A <= 0.0 && R <= 0.0)      outcome = Outcome::DRAW;
    else if (A <= 0.0)             outcome = Outcome::RED_WINS;
    else if (R <= 0.0)             outcome = Outcome::BLUE_WINS;
    else                           outcome = Outcome::INDETERMINATE;
    if (A > 0.0 && A < 0.5 && R > 0.0 && R < 0.5)
        outcome = Outcome::DRAW;

    // Tiempo de desplazamiento (docx §78-82): distancia / velocidad equipo mas rapido
    double v_max = std::max(input.blue_speed_kmh, input.red_speed_kmh);
    double displacement_t = (v_max > 0.0)
        ? (input.distance_m / 1000.0) / v_max * 60.0  // km / (km/h) * 60 = minutos
        : 0.0;

    // Equipo mas rapido (docx §75-76)
    FasterTeam faster = FasterTeam::EQUAL;
    if (input.blue_speed_kmh > input.red_speed_kmh) faster = FasterTeam::BLUE;
    else if (input.red_speed_kmh > input.blue_speed_kmh) faster = FasterTeam::RED;

    // Bajas a escala original (docx §97): sumar lo restado por proporcion
    double blue_total = static_cast<double>(blue_agg.n_total);
    double red_total  = static_cast<double>(red_agg.n_total);

    CombatResult res;
    res.combat_id               = input.combat_id;
    res.outcome                 = outcome;
    res.duration_contact_minutes = t;
    res.displacement_time_minutes = displacement_t;
    res.duration_total_minutes   = displacement_t + t;
    res.blue_initial            = A0;
    res.red_initial             = R0;
    res.blue_initial_total      = blue_total;
    res.red_initial_total       = red_total;
    res.blue_survivors          = std::max(0.0, A);
    res.red_survivors           = std::max(0.0, R);
    res.blue_casualties         = blue_total - res.blue_survivors;
    res.red_casualties          = red_total  - res.red_survivors;
    res.blue_ammo_consumed      = blue_ammo;
    res.red_ammo_consumed       = red_ammo;
    res.blue_cc_ammo_consumed   = blue_cc_ammo;
    res.red_cc_ammo_consumed    = red_cc_ammo;
    res.static_advantage        = static_adv;
    res.faster_team             = faster;
    res.time_series             = std::move(time_series);
    return res;
}

// ---------------------------------------------------------------------------
// Simulacion estocastica (Poisson)
// ---------------------------------------------------------------------------

CombatResult SquareLawModel::simulateStochastic(const CombatInput& input,
                                                 std::mt19937& rng) const {
    AggregatedParams blue_agg = lanchester::aggregate(input.blue_composition);
    AggregatedParams red_agg  = lanchester::aggregate(input.red_composition);

    double A0_real, R0_real;
    if (input.blue_override_initial >= 0)
        A0_real = input.blue_override_initial;
    else
        A0_real = lanchester::initialForces(blue_agg.n_total, input.blue_aft_pct,
                                            input.blue_engagement_fraction);

    if (input.red_override_initial >= 0)
        R0_real = input.red_override_initial;
    else
        R0_real = lanchester::initialForces(red_agg.n_total, input.red_aft_pct,
                                            input.red_engagement_fraction);

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

    std::vector<TimeStep> time_series;
    time_series.push_back({t, static_cast<double>(A), static_cast<double>(R)});

    while (A > 0 && R > 0 && t < input.t_max) {
        if (approach_speed_m_per_min > 0.0) {
            double current_distance = std::max(lanchester::MIN_ENGAGEMENT_DISTANCE_M,
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
        if (max_lambda > lanchester::POISSON_LAMBDA_THRESHOLD) {
            sub_steps = static_cast<int>(std::ceil(max_lambda / lanchester::POISSON_SUBSTEP_TARGET));
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
        time_series.push_back({t, static_cast<double>(std::max(0, A)),
                                  static_cast<double>(std::max(0, R))});
    }

    Outcome outcome;
    if (A <= 0 && R <= 0)    outcome = Outcome::DRAW;
    else if (A <= 0)         outcome = Outcome::RED_WINS;
    else if (R <= 0)         outcome = Outcome::BLUE_WINS;
    else                     outcome = Outcome::INDETERMINATE;

    // Tiempo de desplazamiento (docx §78-82)
    double v_max = std::max(input.blue_speed_kmh, input.red_speed_kmh);
    double displacement_t = (v_max > 0.0)
        ? (input.distance_m / 1000.0) / v_max * 60.0
        : 0.0;

    // Equipo mas rapido (docx §75-76)
    FasterTeam faster = FasterTeam::EQUAL;
    if (input.blue_speed_kmh > input.red_speed_kmh) faster = FasterTeam::BLUE;
    else if (input.red_speed_kmh > input.blue_speed_kmh) faster = FasterTeam::RED;

    // Bajas a escala original (docx §97)
    double blue_total = static_cast<double>(blue_agg.n_total);
    double red_total  = static_cast<double>(red_agg.n_total);

    CombatResult res;
    res.combat_id               = input.combat_id;
    res.outcome                 = outcome;
    res.duration_contact_minutes = t;
    res.displacement_time_minutes = displacement_t;
    res.duration_total_minutes   = displacement_t + t;
    res.blue_initial            = A0d;
    res.red_initial             = R0d;
    res.blue_initial_total      = blue_total;
    res.red_initial_total       = red_total;
    res.blue_survivors          = static_cast<double>(std::max(0, A));
    res.red_survivors           = static_cast<double>(std::max(0, R));
    res.blue_casualties         = blue_total - res.blue_survivors;
    res.red_casualties          = red_total  - res.red_survivors;
    res.blue_ammo_consumed      = blue_ammo;
    res.red_ammo_consumed       = red_ammo;
    res.blue_cc_ammo_consumed   = blue_cc_ammo;
    res.red_cc_ammo_consumed    = red_cc_ammo;
    res.static_advantage        = static_adv;
    res.faster_team             = faster;
    res.time_series             = std::move(time_series);
    return res;
}
