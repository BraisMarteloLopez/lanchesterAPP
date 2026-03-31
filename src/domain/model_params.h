// model_params.h — Implementacion concreta: carga parametros desde JSON
#pragma once

#include "model_params_iface.h"
#include <string>

class ModelParamsClass : public IModelParams {
public:
    // Factory: carga desde fichero JSON. Devuelve defaults si el fichero no existe.
    static ModelParamsClass load(const std::string& path);

    // IModelParams
    double killProbabilitySlope() const override { return data_.kill_probability_slope; }
    const DistanceDegradationCoeffs& distCoeffs() const override { return data_.dist_coeff; }
    double terrainFireMult(Terrain t) const override;
    TacticalMult tacticalMult(const std::string& state) const override;
    bool tacticalMobilityAllowed(const std::string& state) const override;
    double tacticalSpeed(Mobility mob, Terrain ter) const override;
    std::vector<std::string> tacticalStateNames() const override;

private:
    ModelParams data_;
};
