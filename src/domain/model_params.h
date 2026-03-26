// model_params.h — Clase ModelParamsClass (parametros del modelo, inmutables)
#pragma once

#include "lanchester_types.h"
#include <map>
#include <string>

class ModelParamsClass {
public:
    // Factory: carga desde fichero JSON. Devuelve defaults si el fichero no existe.
    static ModelParamsClass load(const std::string& path);

    // Accessors (inmutable tras construccion)
    double killProbabilitySlope() const { return data_.kill_probability_slope; }
    const DistanceDegradationCoeffs& distCoeffs() const { return data_.dist_coeff; }

    double terrainFireMult(Terrain t) const;
    TacticalMult tacticalMult(const std::string& state) const;

    // Acceso directo al struct interno
    const ModelParams& raw() const { return data_; }

private:
    ModelParams data_;
};
