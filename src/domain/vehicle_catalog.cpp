// vehicle_catalog.cpp — Implementacion de VehicleCatalogClass
#include "vehicle_catalog.h"
#include "nlohmann/json.hpp"

#include <fstream>

using json = nlohmann::json;

static VehicleParams vehicle_from_json_impl(const json& j) {
    VehicleParams v;
    v.name  = j.at("name").get<std::string>();
    v.D     = j.at("D").get<double>();
    v.P     = j.at("P").get<double>();
    v.U     = j.at("U").get<double>();
    v.c     = j.at("c").get<double>();
    v.A_max = j.at("A_max").get<double>();
    v.f     = j.at("f").get<double>();
    v.CC    = j.at("CC").get<int>();
    v.P_cc  = j.at("P_cc").get<double>();
    v.D_cc  = j.at("D_cc").get<double>();
    v.c_cc  = j.at("c_cc").get<double>();
    v.A_cc  = j.at("A_cc").get<double>();
    v.M     = j.at("M").get<double>();
    v.f_cc  = j.at("f_cc").get<double>();
    return v;
}

VehicleCatalogClass VehicleCatalogClass::load(const std::string& path) {
    VehicleCatalogClass cat;
    std::ifstream ifs(path);
    if (!ifs.is_open()) return cat;
    json j = json::parse(ifs);
    for (const auto& vj : j.at("vehicles"))
        cat.vehicles_[vj.at("name").get<std::string>()] = vehicle_from_json_impl(vj);
    return cat;
}

const VehicleParams& VehicleCatalogClass::find(const std::string& name) const {
    auto it = vehicles_.find(name);
    if (it != vehicles_.end()) return it->second;
    throw std::runtime_error("Vehiculo '" + name + "' no encontrado en catalogo.");
}

VehicleParams VehicleCatalogClass::findInEither(const std::string& name,
                                                 const IVehicleCatalog& primary,
                                                 const IVehicleCatalog& secondary) {
    if (primary.contains(name)) return primary.find(name);
    if (secondary.contains(name)) return secondary.find(name);
    throw std::runtime_error("Vehiculo '" + name + "' no encontrado en catalogos.");
}

std::vector<std::string> VehicleCatalogClass::names() const {
    std::vector<std::string> result;
    result.reserve(vehicles_.size());
    for (const auto& [name, _] : vehicles_)
        result.push_back(name);
    return result;
}

bool VehicleCatalogClass::contains(const std::string& name) const {
    return vehicles_.count(name) > 0;
}
