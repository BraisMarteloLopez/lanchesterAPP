// model_params.h — Clase ModelParams (reemplaza g_model_params global)
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

    // Acceso directo al struct interno (para compatibilidad con codigo legacy)
    const ModelParams& raw() const { return data_; }

    // Aplicar al global legacy (puente temporal para Fase 1)
    void applyToGlobal() const;

private:
    ModelParams data_;
};
