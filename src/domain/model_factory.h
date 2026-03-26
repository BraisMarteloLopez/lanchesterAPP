// model_factory.h — Registro y factory de modelos de simulacion
#pragma once

#include "lanchester_model_iface.h"
#include "model_params_iface.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

class ModelFactory {
public:
    using Creator = std::function<
        std::shared_ptr<IStochasticModel>(std::shared_ptr<const IModelParams>)>;

    // Singleton — unico punto de registro global
    static ModelFactory& instance();

    // Registra un modelo. Devuelve true (util para inicializacion estatica).
    bool registerModel(const std::string& name, Creator creator);

    // Crea una instancia del modelo por nombre. Lanza si no existe.
    std::shared_ptr<IStochasticModel> create(
        const std::string& name,
        std::shared_ptr<const IModelParams> params) const;

    // Nombres de todos los modelos registrados.
    std::vector<std::string> availableModels() const;

    // Nombre del modelo por defecto (el primero registrado).
    std::string defaultModel() const;

private:
    ModelFactory() = default;
    struct Entry {
        std::string name;
        Creator creator;
    };
    std::vector<Entry> registry_;
};
