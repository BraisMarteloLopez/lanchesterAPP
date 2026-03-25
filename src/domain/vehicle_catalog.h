// vehicle_catalog.h — Clase VehicleCatalog (encapsula carga y busqueda de vehiculos)
#pragma once

#include "lanchester_types.h"
#include <map>
#include <string>
#include <vector>
#include <stdexcept>

class VehicleCatalogClass {
public:
    // Factory: carga desde fichero JSON. Devuelve catalogo vacio si el fichero no existe.
    static VehicleCatalogClass load(const std::string& path);

    // Busca un vehiculo. Lanza runtime_error si no existe.
    const VehicleParams& find(const std::string& name) const;

    // Busca en este catalogo primero, luego en secondary. Lanza si no existe en ninguno.
    static VehicleParams findInEither(const std::string& name,
                                      const VehicleCatalogClass& primary,
                                      const VehicleCatalogClass& secondary);

    // Lista de nombres de vehiculos (orden del map)
    std::vector<std::string> names() const;

    // Existe?
    bool contains(const std::string& name) const;

    // Numero de vehiculos
    size_t size() const { return vehicles_.size(); }

    // Acceso al map interno (compatibilidad legacy)
    const VehicleCatalog& raw() const { return vehicles_; }

private:
    VehicleCatalog vehicles_;
};
