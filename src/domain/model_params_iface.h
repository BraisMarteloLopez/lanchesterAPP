// model_params_iface.h — Interfaz abstracta para parametros del modelo
#pragma once

#include "lanchester_types.h"
#include <string>
#include <vector>

class IModelParams {
public:
    virtual ~IModelParams() = default;

    virtual double killProbabilitySlope() const = 0;
    virtual const DistanceDegradationCoeffs& distCoeffs() const = 0;
    virtual double terrainFireMult(Terrain t) const = 0;
    virtual TacticalMult tacticalMult(const std::string& state) const = 0;
    virtual double tacticalSpeed(Mobility mob, Terrain ter) const = 0;
    virtual std::vector<std::string> tacticalStateNames() const = 0;
};
