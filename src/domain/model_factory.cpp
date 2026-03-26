// model_factory.cpp — Implementacion del registro de modelos
#include "model_factory.h"

#include <stdexcept>

ModelFactory& ModelFactory::instance() {
    static ModelFactory factory;
    return factory;
}

bool ModelFactory::registerModel(const std::string& name, Creator creator) {
    registry_.push_back({name, std::move(creator)});
    return true;
}

std::shared_ptr<IStochasticModel> ModelFactory::create(
    const std::string& name,
    std::shared_ptr<const IModelParams> params) const
{
    for (const auto& entry : registry_) {
        if (entry.name == name)
            return entry.creator(std::move(params));
    }
    throw std::runtime_error("Modelo '" + name + "' no registrado en ModelFactory.");
}

std::vector<std::string> ModelFactory::availableModels() const {
    std::vector<std::string> names;
    names.reserve(registry_.size());
    for (const auto& entry : registry_)
        names.push_back(entry.name);
    return names;
}

std::string ModelFactory::defaultModel() const {
    if (registry_.empty())
        throw std::runtime_error("No hay modelos registrados en ModelFactory.");
    return registry_.front().name;
}
