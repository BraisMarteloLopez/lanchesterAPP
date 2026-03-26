// vehicle_catalog.h — Implementacion concreta: carga catalogo desde JSON
#pragma once

#include "vehicle_catalog_iface.h"
#include <map>
#include <string>
#include <stdexcept>

class VehicleCatalogClass : public IVehicleCatalog {
public:
    // Factory: carga desde fichero JSON. Devuelve catalogo vacio si el fichero no existe.
    static VehicleCatalogClass load(const std::string& path);

    // IVehicleCatalog
    const VehicleParams& find(const std::string& name) const override;
    bool contains(const std::string& name) const override;
    std::vector<std::string> names() const override;
    size_t size() const override { return vehicles_.size(); }

    // Utilidad: busca en primary, luego en secondary. Lanza si no existe en ninguno.
    static VehicleParams findInEither(const std::string& name,
                                      const IVehicleCatalog& primary,
                                      const IVehicleCatalog& secondary);

private:
    std::map<std::string, VehicleParams> vehicles_;
};
