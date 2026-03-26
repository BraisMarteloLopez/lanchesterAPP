// vehicle_catalog_iface.h — Interfaz abstracta para catalogos de vehiculos
#pragma once

#include "lanchester_types.h"
#include <string>
#include <vector>

class IVehicleCatalog {
public:
    virtual ~IVehicleCatalog() = default;

    virtual const VehicleParams& find(const std::string& name) const = 0;
    virtual bool contains(const std::string& name) const = 0;
    virtual std::vector<std::string> names() const = 0;
    virtual size_t size() const = 0;
};
