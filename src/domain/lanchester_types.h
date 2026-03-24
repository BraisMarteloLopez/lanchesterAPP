// lanchester_types.h — Tipos de datos del modelo Lanchester-CIO
#pragma once

#include <map>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

// ---------------------------------------------------------------------------
// Parametros del modelo (externalizados, cargables desde model_params.json)
// ---------------------------------------------------------------------------

struct DistanceDegradationCoeffs {
    double c_dk    = -0.188;
    double c_f     = -0.865;
    double c_dk2   =  0.018;
    double c_dk_f  = -0.162;
    double c_f2    =  0.755;
    double c_const =  1.295;
};

struct TacticalMultDef {
    double self_mult     = 1.0;
    double opponent_mult = 1.0;
};

struct ModelParams {
    double kill_probability_slope = 175.0;
    DistanceDegradationCoeffs dist_coeff;
    double terrain_fire_mult_facil   = 1.0;
    double terrain_fire_mult_medio   = 0.85;
    double terrain_fire_mult_dificil = 0.65;
    std::map<std::string, TacticalMultDef> tactical_multipliers;
};

// Instancia global de parametros del modelo (definida en main.cpp)
extern ModelParams g_model_params;

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
enum class AggregationMode { PRE, POST };

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
    AggregationMode aggregation_mode = AggregationMode::PRE;
    Terrain terrain = Terrain::MEDIO;
    double approach_speed_kmh = 0;
    double blue_override_initial = -1;
    double red_override_initial  = -1;
};

struct ScenarioOutput {
    std::string scenario_id;
    std::vector<CombatResult> combats;
};

// ---------------------------------------------------------------------------
// Monte Carlo
// ---------------------------------------------------------------------------

struct PercentileStats {
    double mean   = 0;
    double std    = 0;
    double p05    = 0;
    double p25    = 0;
    double median = 0;
    double p75    = 0;
    double p95    = 0;
};

struct MonteCarloResult {
    int combat_id       = 0;
    int n_replicas      = 0;
    PercentileStats blue_survivors;
    PercentileStats red_survivors;
    PercentileStats duration;
    int count_blue_wins     = 0;
    int count_red_wins      = 0;
    int count_draw          = 0;
    int count_indeterminate = 0;
    CombatResult deterministic;
};

struct MonteCarloScenarioOutput {
    std::string scenario_id;
    int n_replicas = 0;
    uint64_t seed  = 0;
    std::vector<MonteCarloResult> combats;
};

using VehicleCatalog = std::map<std::string, VehicleParams>;
