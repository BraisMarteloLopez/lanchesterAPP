// model_params.h — Clase ModelParamsClass (parametros del modelo, inmutables)
#pragma once

#include "lanchester_types.h"
#include <map>
#include <string>
#include <vector>

class ModelParamsClass {
public:
    // Factory: carga desde fichero JSON. Devuelve defaults si el fichero no existe.
    static ModelParamsClass load(const std::string& path);

    // Accessors (inmutable tras construccion)
    double killProbabilitySlope() const { return data_.kill_probability_slope; }
    const DistanceDegradationCoeffs& distCoeffs() const { return data_.dist_coeff; }

    // Multiplicador de terreno (devuelve 1.0 si el terreno no existe en el map)
    double terrainFireMult(Terrain t) const;

    // Multiplicadores tacticos (devuelve {1,1} si el estado no existe)
    TacticalMult tacticalMult(const std::string& state) const;

    // Velocidad tactica en km/h (devuelve 0 si la combinacion no existe)
    double tacticalSpeed(Mobility mob, Terrain ter) const;

    // Nombres de los estados tacticos conocidos (del JSON)
    std::vector<std::string> tacticalStateNames() const;

private:
    ModelParams data_;
};
