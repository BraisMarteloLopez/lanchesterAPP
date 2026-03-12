// Modelo Lanchester-CIO — Nucleo matematico + Bucle Euler
// CIO / ET — Herramienta de investigacion interna
// Sesion 1: Sin I/O JSON todavia. Funciones puras + simulacion.

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Estructuras de datos
// ---------------------------------------------------------------------------

struct VehicleParams {
    std::string name;
    double D;       // Dureza [1-1000]
    double P;       // Potencia [1-1000]
    double U;       // Punteria [0.2-1.0]
    double c;       // Cadencia (disparos/min) [0.1-5]
    double A_max;   // Alcance maximo (m) [200-5000]
    double f;       // Factor distancia [0-2]
    int    CC;      // Capacidad C/C {0,1}
    double P_cc;    // Potencia C/C [1-1200]
    double D_cc;    // Dureza C/C [1-1200]
    double c_cc;    // Cadencia C/C (disparos/min) [0.1-1]
    double A_cc;    // Alcance C/C (m) [200-5500]
    double M;       // Municion C/C por vehiculo [1-10]
    double f_cc;    // Factor distancia C/C [0-2]
};

struct CompositionEntry {
    VehicleParams vehicle;
    int count;
};

struct AggregatedParams {
    // Convencional
    double D;
    double P;
    double U;
    double c;
    double A_max;
    double f;
    int    n_total;
    // C/C
    double P_cc;
    double D_cc;
    double c_cc;
    double A_cc;
    double M;
    double f_cc;
    int    n_cc;      // unidades con capacidad C/C
    bool   has_cc;
};

enum class Outcome { BLUE_WINS, RED_WINS, DRAW, INDETERMINATE };

struct CombatResult {
    Outcome outcome;
    double duration_contact_minutes;
    double blue_initial;
    double red_initial;
    double blue_survivors;
    double red_survivors;
    double blue_casualties;
    double red_casualties;
    double blue_ammo_consumed;
    double red_ammo_consumed;
    double blue_cc_ammo_consumed;
    double red_cc_ammo_consumed;
    double static_advantage;
};

// Multiplicadores tacticos: {mult_propio, mult_oponente}
struct TacticalMult {
    double self_mult;
    double opponent_mult;
};

// ---------------------------------------------------------------------------
// Funciones del modelo
// ---------------------------------------------------------------------------

// Probabilidad de destruccion al impactar (sigmoide)
// Sirve para ambos canales: convencional (D,P) y C/C (D_cc,P_cc)
double kill_probability(double D_target, double P_attacker) {
    return 1.0 / (1.0 + std::exp((D_target - P_attacker) / 175.0));
}

// Degradacion del disparo por distancia (polinomio comun a ambos canales)
// d: distancia en metros, f_dist: factor de distancia, range_max: alcance maximo
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

// Tasa de destruccion estatica convencional
double static_rate_conv(double T, double G, double U, double c) {
    return T * G * U * c;
}

// Tasa de destruccion C/C estatica (sin punteria: misiles guiados)
double static_rate_cc(double T_cc, double G_cc, double c_cc) {
    return c_cc * T_cc * G_cc;
}

// Tasa de destruccion C/C dinamica
// A_current: unidades propias vivas, A0: unidades propias iniciales
// t: tiempo transcurrido en minutos
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
    if (state == "Ataque a posicion defensiva")   return {1.0, 1.0};
    if (state == "Busqueda del contacto")         return {0.9, 1.0};
    if (state == "En posicion de tiro")           return {1.0, 0.9};
    if (state == "Defensiva condiciones minimas")  return {1.0, 1.0 / (2.25 * 2.25)};
    if (state == "Defensiva organizacion ligera")  return {1.0, 1.0 / (2.75 * 2.75)};
    if (state == "Defensiva organizacion media")   return {1.0, 1.0 / (4.25 * 4.25)};
    if (state == "Retardo")                        return {1.0, 1.0 / (6.0 * 6.0)};
    if (state == "Retrocede")                      return {0.9, 1.0};
    // Valor por defecto conservador: sin bonus
    return {1.0, 1.0};
}

// ---------------------------------------------------------------------------
// Tabla de velocidades tacticas (km/h)
// ---------------------------------------------------------------------------

enum class Mobility { MUY_ALTA, ALTA, MEDIA, BAJA };
enum class Terrain  { FACIL, MEDIO, DIFICIL };

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
    // [movilidad][terreno] — FACIL, MEDIO, DIFICIL
    static const double table[4][3] = {
        {40, 25, 12},  // MUY_ALTA
        {30, 20, 10},  // ALTA
        {20, 12,  6},  // MEDIA
        {10,  6,  3},  // BAJA
    };
    return table[static_cast<int>(mob)][static_cast<int>(ter)];
}

double displacement_time(double distance_m, Mobility mob_blue, Mobility mob_red,
                         Terrain terrain) {
    // Se usa la velocidad de la fuerza mas lenta
    double v_blue = tactical_speed(mob_blue, terrain);
    double v_red  = tactical_speed(mob_red, terrain);
    double v_min  = std::min(v_blue, v_red);
    if (v_min <= 0.0) return 0.0;
    return (distance_m / 1000.0) * 60.0 / v_min; // minutos
}

// ---------------------------------------------------------------------------
// Agregacion de fuerzas mixtas (modo pre-tasa)
// ---------------------------------------------------------------------------

AggregatedParams aggregate(const std::vector<CompositionEntry>& composition) {
    AggregatedParams agg{};
    int n_total = 0;
    int n_cc = 0;

    // Sumas ponderadas convencionales
    double sum_D = 0, sum_P = 0, sum_U = 0, sum_c = 0, sum_Amax = 0, sum_f = 0;
    // Sumas ponderadas C/C
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
            n_cc     += n;
            sum_Pcc  += n * v.P_cc;
            sum_Dcc  += n * v.D_cc;
            sum_ccc  += n * v.c_cc;
            sum_Acc  += n * v.A_cc;
            sum_M    += n * v.M;
            sum_fcc  += n * v.f_cc;
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

    // Proteccion DT-001: division por cero si no hay unidades C/C
    if (n_cc > 0) {
        agg.has_cc = true;
        agg.P_cc   = sum_Pcc / n_cc;
        agg.D_cc   = sum_Dcc / n_cc;
        agg.c_cc   = sum_ccc / n_cc;
        agg.A_cc   = sum_Acc / n_cc;
        agg.M      = sum_M   / n_cc;
        agg.f_cc   = sum_fcc / n_cc;
    } else {
        agg.has_cc = false;
        agg.P_cc = agg.D_cc = agg.c_cc = agg.A_cc = agg.M = agg.f_cc = 0.0;
    }

    return agg;
}

// ---------------------------------------------------------------------------
// Calculo de tasas efectivas para un bando
// ---------------------------------------------------------------------------

struct EffectiveRates {
    double S_conv;       // Tasa convencional efectiva (con multiplicadores y rate_factor)
    double S_cc_static;  // Tasa C/C estatica (sin rate_factor, se aplica aparte)
    double rate_factor;  // Para aplicar a S_cc en el bucle
    double c_agg;        // Cadencia convencional agregada (para conteo de municion)
    double c_cc_agg;     // Cadencia C/C agregada
    double M_agg;        // Municion C/C agregada por vehiculo
    bool   has_cc;
    int    n_cc;
    int    n_total;
};

EffectiveRates compute_effective_rates(
    const AggregatedParams& attacker,
    const AggregatedParams& defender,
    double distance_m,
    const std::string& attacker_state,
    const std::string& defender_state,
    double rate_factor)
{
    EffectiveRates er{};
    er.rate_factor = rate_factor;
    er.c_agg       = attacker.c;
    er.c_cc_agg    = attacker.c_cc;
    er.M_agg       = attacker.M;
    er.has_cc      = attacker.has_cc;
    er.n_cc        = attacker.n_cc;
    er.n_total     = attacker.n_total;

    // Canal convencional
    double T = kill_probability(defender.D, attacker.P);
    double G = distance_degradation(distance_m, attacker.f, attacker.A_max);
    double S = static_rate_conv(T, G, attacker.U, attacker.c);

    // Multiplicadores tacticos
    TacticalMult att_mult = get_tactical_multipliers(attacker_state);
    TacticalMult def_mult = get_tactical_multipliers(defender_state);

    // S_efectiva = S * mult_propio * mult_oponente_del_defensor * rate_factor
    er.S_conv = S * att_mult.self_mult * def_mult.opponent_mult * rate_factor;

    // Canal C/C
    if (attacker.has_cc) {
        double T_cc = kill_probability(defender.D_cc, attacker.P_cc);
        double G_cc = distance_degradation(distance_m, attacker.f_cc, attacker.A_cc);
        er.S_cc_static = static_rate_cc(T_cc, G_cc, attacker.c_cc);
    } else {
        er.S_cc_static = 0.0;
    }

    return er;
}

// Tasa total en un instante t, con N_attacker unidades vivas del atacante
double total_rate(const EffectiveRates& er, double t,
                  double N_attacker, double N_attacker_0) {
    double s_cc = 0.0;
    if (er.has_cc && er.S_cc_static > 0.0) {
        s_cc = dynamic_rate_cc(er.S_cc_static, N_attacker, N_attacker_0,
                               er.M_agg, er.c_cc_agg, t) * er.rate_factor;
    }
    return er.S_conv + s_cc;
}

// ---------------------------------------------------------------------------
// Fuerzas iniciales
// ---------------------------------------------------------------------------

double initial_forces(int n_total, double aft_casualties_pct,
                      double engagement_fraction, double count_factor) {
    double n = static_cast<double>(n_total);
    return (n - n * aft_casualties_pct) * engagement_fraction * count_factor;
}

// ---------------------------------------------------------------------------
// Bucle de integracion Euler + resultado del combate
// ---------------------------------------------------------------------------

struct CombatInput {
    std::vector<CompositionEntry> blue_composition;
    std::vector<CompositionEntry> red_composition;
    std::string blue_state;
    std::string red_state;
    double blue_aft_pct;
    double red_aft_pct;
    double blue_engagement_fraction;
    double red_engagement_fraction;
    double blue_rate_factor;
    double red_rate_factor;
    double blue_count_factor;
    double red_count_factor;
    double distance_m;
    double h;           // paso temporal (minutos)
    double t_max;       // tiempo maximo (minutos)
};

CombatResult simulate_combat(const CombatInput& input) {
    // Agregar fuerzas
    AggregatedParams blue_agg = aggregate(input.blue_composition);
    AggregatedParams red_agg  = aggregate(input.red_composition);

    // Fuerzas iniciales
    double A0 = initial_forces(blue_agg.n_total, input.blue_aft_pct,
                               input.blue_engagement_fraction,
                               input.blue_count_factor);
    double R0 = initial_forces(red_agg.n_total, input.red_aft_pct,
                               input.red_engagement_fraction,
                               input.red_count_factor);

    // Tasas efectivas: azul ataca a rojo, rojo ataca a azul
    EffectiveRates blue_rates = compute_effective_rates(
        blue_agg, red_agg, input.distance_m,
        input.blue_state, input.red_state, input.blue_rate_factor);

    EffectiveRates red_rates = compute_effective_rates(
        red_agg, blue_agg, input.distance_m,
        input.red_state, input.blue_state, input.red_rate_factor);

    // Static advantage (tasa total en t=0)
    double S_blue_t0 = total_rate(blue_rates, 0.0, A0, A0);
    double S_red_t0  = total_rate(red_rates, 0.0, R0, R0);
    double static_adv = 0.0;
    if (S_red_t0 * R0 * R0 > 0.0) {
        static_adv = (S_blue_t0 * A0 * A0) / (S_red_t0 * R0 * R0);
    }

    // Bucle Euler
    double A = A0;
    double R = R0;
    double t = 0.0;

    double blue_ammo = 0.0, red_ammo = 0.0;
    double blue_cc_ammo = 0.0, red_cc_ammo = 0.0;

    double blue_cc_max = (blue_rates.has_cc)
        ? blue_rates.M_agg * static_cast<double>(blue_rates.n_cc)
          * input.blue_engagement_fraction * (1.0 - input.blue_aft_pct)
          * input.blue_count_factor
        : 0.0;
    double red_cc_max = (red_rates.has_cc)
        ? red_rates.M_agg * static_cast<double>(red_rates.n_cc)
          * input.red_engagement_fraction * (1.0 - input.red_aft_pct)
          * input.red_count_factor
        : 0.0;

    double cc_ratio_blue = (blue_agg.n_total > 0)
        ? static_cast<double>(blue_agg.n_cc) / blue_agg.n_total : 0.0;
    double cc_ratio_red = (red_agg.n_total > 0)
        ? static_cast<double>(red_agg.n_cc) / red_agg.n_total : 0.0;

    while (A > 0.0 && R > 0.0 && t < input.t_max) {
        double S_red_t  = total_rate(red_rates, t, R, R0);   // rojo destruye azul
        double S_blue_t = total_rate(blue_rates, t, A, A0);  // azul destruye rojo

        double A_new = std::max(0.0, A - S_red_t * R * input.h);
        double R_new = std::max(0.0, R - S_blue_t * A * input.h);

        // Acumular municion convencional
        blue_ammo += blue_rates.c_agg * A * input.h;
        red_ammo  += red_rates.c_agg  * R * input.h;

        // Acumular municion C/C (con tope)
        if (blue_rates.has_cc) {
            double delta = blue_rates.c_cc_agg * (A * cc_ratio_blue) * input.h;
            blue_cc_ammo = std::min(blue_cc_ammo + delta, blue_cc_max);
        }
        if (red_rates.has_cc) {
            double delta = red_rates.c_cc_agg * (R * cc_ratio_red) * input.h;
            red_cc_ammo = std::min(red_cc_ammo + delta, red_cc_max);
        }

        A = A_new;
        R = R_new;
        t += input.h;
    }

    // Determinar outcome
    Outcome outcome;
    if (A <= 0.0 && R <= 0.0) {
        outcome = Outcome::DRAW;
    } else if (A <= 0.0) {
        outcome = Outcome::RED_WINS;
    } else if (R <= 0.0) {
        outcome = Outcome::BLUE_WINS;
    } else {
        outcome = Outcome::INDETERMINATE;
    }

    // Threshold DRAW: ambas < 0.5
    if (A > 0.0 && A < 0.5 && R > 0.0 && R < 0.5) {
        outcome = Outcome::DRAW;
    }

    CombatResult result;
    result.outcome                  = outcome;
    result.duration_contact_minutes = t;
    result.blue_initial             = A0;
    result.red_initial              = R0;
    result.blue_survivors           = std::max(0.0, A);
    result.red_survivors            = std::max(0.0, R);
    result.blue_casualties          = A0 - result.blue_survivors;
    result.red_casualties           = R0 - result.red_survivors;
    result.blue_ammo_consumed       = blue_ammo;
    result.red_ammo_consumed        = red_ammo;
    result.blue_cc_ammo_consumed    = blue_cc_ammo;
    result.red_cc_ammo_consumed     = red_cc_ammo;
    result.static_advantage         = static_adv;

    return result;
}

// ---------------------------------------------------------------------------
// Utilidad: nombre del outcome
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
// main — prueba basica del nucleo (Sesion 1, sin I/O JSON)
// ---------------------------------------------------------------------------

#include <cstdio>

int main() {
    // Vehiculo azul de ejemplo: TOA con SPIKE
    VehicleParams toa_spike;
    toa_spike.name  = "TOA_SPIKE_I";
    toa_spike.D     = 150;
    toa_spike.P     = 80;
    toa_spike.U     = 0.6;
    toa_spike.c     = 2.0;
    toa_spike.A_max = 2500;
    toa_spike.f     = 1.0;
    toa_spike.CC    = 1;
    toa_spike.P_cc  = 900;
    toa_spike.D_cc  = 150;
    toa_spike.c_cc  = 0.5;
    toa_spike.A_cc  = 4000;
    toa_spike.M     = 4;
    toa_spike.f_cc  = 0.5;

    // Vehiculo rojo de ejemplo: T-80U
    VehicleParams t80u;
    t80u.name  = "T-80U";
    t80u.D     = 800;
    t80u.P     = 750;
    t80u.U     = 0.7;
    t80u.c     = 3.0;
    t80u.A_max = 4000;
    t80u.f     = 0.8;
    t80u.CC    = 0;
    t80u.P_cc  = 0;
    t80u.D_cc  = 800;
    t80u.c_cc  = 0;
    t80u.A_cc  = 0;
    t80u.M     = 0;
    t80u.f_cc  = 0;

    // Escenario: 6 TOA SPIKE vs 10 T-80U a 3000m
    CombatInput input;
    input.blue_composition = {{toa_spike, 6}};
    input.red_composition  = {{t80u, 10}};
    input.blue_state = "En posicion de tiro";
    input.red_state  = "Ataque a posicion defensiva";
    input.blue_aft_pct = 0.025;
    input.red_aft_pct  = 0.0;
    input.blue_engagement_fraction = 0.666;
    input.red_engagement_fraction  = 0.666;
    input.blue_rate_factor = 1.0;
    input.red_rate_factor  = 1.0;
    input.blue_count_factor = 1.0;
    input.red_count_factor  = 1.0;
    input.distance_m = 3000;
    input.h = 1.0 / 600.0;   // ~0.1 segundos
    input.t_max = 30.0;      // 30 minutos

    CombatResult r = simulate_combat(input);

    std::printf("=== Resultado del combate ===\n");
    std::printf("Outcome:            %s\n", outcome_str(r.outcome));
    std::printf("Duracion (min):     %.4f\n", r.duration_contact_minutes);
    std::printf("Azul inicial:       %.2f\n", r.blue_initial);
    std::printf("Rojo inicial:       %.2f\n", r.red_initial);
    std::printf("Azul superviv.:     %.4f\n", r.blue_survivors);
    std::printf("Rojo superviv.:     %.4f\n", r.red_survivors);
    std::printf("Azul bajas:         %.4f\n", r.blue_casualties);
    std::printf("Rojo bajas:         %.4f\n", r.red_casualties);
    std::printf("Azul municion conv: %.2f\n", r.blue_ammo_consumed);
    std::printf("Rojo municion conv: %.2f\n", r.red_ammo_consumed);
    std::printf("Azul municion C/C:  %.2f\n", r.blue_cc_ammo_consumed);
    std::printf("Rojo municion C/C:  %.2f\n", r.red_cc_ammo_consumed);
    std::printf("Static advantage:   %.4f\n", r.static_advantage);

    return 0;
}
