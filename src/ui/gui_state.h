// gui_state.h — Estado de la aplicacion y tipos de la GUI
#pragma once

#include "../application/simulation_service.h"
#include "../domain/model_factory.h"
#include "gui_config.h"

#include <future>
#include <memory>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Paleta de colores
// ---------------------------------------------------------------------------

namespace colors {
    constexpr ImVec4 bg_dark       = {0.10f, 0.10f, 0.12f, 1.0f};
    constexpr ImVec4 panel         = {0.15f, 0.15f, 0.16f, 1.0f};
    constexpr ImVec4 nav_bar       = {0.17f, 0.17f, 0.19f, 1.0f};
    constexpr ImVec4 step_active   = {0.29f, 0.56f, 0.85f, 1.0f};  // #4A90D9
    constexpr ImVec4 step_complete = {0.36f, 0.67f, 0.41f, 1.0f};  // #5DAA68
    constexpr ImVec4 step_blocked  = {0.40f, 0.40f, 0.40f, 1.0f};
    constexpr ImVec4 blue_side     = {0.20f, 0.50f, 0.80f, 1.0f};  // #3380CC
    constexpr ImVec4 red_side      = {0.80f, 0.20f, 0.20f, 1.0f};  // #CC3333
    constexpr ImVec4 btn_execute   = {0.18f, 0.56f, 0.24f, 1.0f};  // #2D8E3D
    constexpr ImVec4 btn_execute_h = {0.24f, 0.65f, 0.30f, 1.0f};
    constexpr ImVec4 text_primary  = {0.88f, 0.88f, 0.88f, 1.0f};
    constexpr ImVec4 text_secondary= {0.60f, 0.60f, 0.60f, 1.0f};
    constexpr ImVec4 error_text    = {1.00f, 0.40f, 0.40f, 1.0f};
    constexpr ImVec4 outcome_draw  = {0.80f, 0.80f, 0.00f, 1.0f};
}

// ---------------------------------------------------------------------------
// Wizard
// ---------------------------------------------------------------------------

enum class WizardStep { SCENARIO = 0, BLUE_SIDE = 1, RED_SIDE = 2, SIMULATION = 3 };
constexpr int WIZARD_STEP_COUNT = 4;

static const char* WIZARD_STEP_NAMES[] = {
    "1. Escenario", "2. Bando Azul", "3. Bando Rojo", "4. Simulacion"
};

// ---------------------------------------------------------------------------
// Configuracion de un bando (estado UI)
// ---------------------------------------------------------------------------

struct GuiSideConfig {
    int tactical_state_idx  = 0;
    int mobility_idx        = 1; // ALTA
    float aft_casualties_pct = 0.0f;
    float engagement_fraction = 2.0f / 3.0f;
    float rate_factor       = 1.0f;
    float count_factor      = 1.0f;

    static constexpr int MAX_TYPES = 4;
    int vehicle_idx[MAX_TYPES]  = {0, 0, 0, 0};
    int vehicle_count[MAX_TYPES] = {10, 0, 0, 0};
    int num_types = 1;

    // Tiene al menos un vehiculo con count >= 1?
    bool isValid() const {
        for (int i = 0; i < num_types; ++i)
            if (vehicle_count[i] >= 1) return true;
        return false;
    }
};

// ---------------------------------------------------------------------------
// Estado global de la aplicacion
// ---------------------------------------------------------------------------

struct AppState {
    // Wizard
    WizardStep current_step = WizardStep::SCENARIO;

    // Datos cargados
    std::vector<std::string> blue_names, red_names;
    std::vector<std::string> tactical_state_names;
    std::vector<std::string> model_names;
    std::string exe_dir;

    // Configuracion escenario
    int terrain_idx = 1; // MEDIO
    float distance_m = 2000.0f;
    float t_max_minutes = 30.0f;
    int aggregation_idx = 0; // PRE
    int model_idx = 0;
    int mode = 0; // 0=Determinista, 1=Monte Carlo
    int mc_replicas = 1000;
    int mc_seed = 42;

    // Bandos
    GuiSideConfig blue, red;

    // Resultados
    bool has_result = false;
    ScenarioOutput result;
    bool has_mc_result = false;
    MonteCarloScenarioOutput mc_result;

    // Animacion
    bool animating = false;
    int anim_step = 0;
    double anim_timer = 0;

    // Estado de simulacion
    bool running = false;
    std::future<ScenarioOutput> future_result;
    std::future<MonteCarloScenarioOutput> future_mc;

    // Error
    char error_msg[256] = "";

    // Servicios
    std::shared_ptr<SimulationService> service;
    GuiConfig gui_config;

    // Validacion de pasos
    bool isStepAccessible(WizardStep step) const {
        switch (step) {
            case WizardStep::SCENARIO:   return true;
            case WizardStep::BLUE_SIDE:  return true;
            case WizardStep::RED_SIDE:   return blue.isValid();
            case WizardStep::SIMULATION: return blue.isValid() && red.isValid();
        }
        return false;
    }

    bool isStepComplete(WizardStep step) const {
        switch (step) {
            case WizardStep::SCENARIO:   return true; // always valid with defaults
            case WizardStep::BLUE_SIDE:  return blue.isValid();
            case WizardStep::RED_SIDE:   return red.isValid();
            case WizardStep::SIMULATION: return has_result || has_mc_result;
        }
        return false;
    }
};
