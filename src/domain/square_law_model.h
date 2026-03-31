// square_law_model.h — Modelo de Lanchester ley cuadrada (Euler + Poisson)
#pragma once

#include "lanchester_model_iface.h"
#include "model_params_iface.h"
#include <memory>
#include <string>

class SquareLawModel : public IStochasticModel {
public:
    explicit SquareLawModel(std::shared_ptr<const IModelParams> params);

    CombatResult simulate(const CombatInput& input) const override;
    CombatResult simulateStochastic(const CombatInput& input,
                                    std::mt19937& rng) const override;
    std::string name() const override { return "Lanchester Square Law (Euler)"; }

private:
    std::shared_ptr<const IModelParams> params_;

    double killProbability(double D_target, double P_attacker) const;
    double distanceDegradation(double d, double f_dist, double range_max) const;
    double staticRateConv(double T, double G, double U, double c) const;
    double staticRateCc(double T_cc, double G_cc, double c_cc) const;
    double dynamicRateCc(double S_cc_static, double A_current, double A0,
                         double cc_ammo_consumed, double cc_ammo_max) const;
    TacticalMult getTacticalMultipliers(const std::string& state) const;
    double terrainFireMultiplier(Terrain ter) const;

    EffectiveRates computeEffectiveRates(
        const AggregatedParams& attacker, const AggregatedParams& defender,
        double distance_m, const std::string& att_state,
        const std::string& def_state, double rate_factor) const;

    EffectiveRates computeEffectiveRatesPost(
        const std::vector<CompositionEntry>& att_comp,
        const AggregatedParams& defender,
        double distance_m, const std::string& att_state,
        const std::string& def_state, double rate_factor) const;

    double totalRate(const EffectiveRates& er,
                     double N_att, double N_att0,
                     double cc_ammo_consumed, double cc_ammo_max) const;
};
