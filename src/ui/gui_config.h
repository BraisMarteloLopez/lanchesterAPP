// gui_config.h — Configuracion de la GUI (cargada desde fichero, no editable por usuario)
#pragma once

#include <string>

struct GuiConfig {
    int animation_speed_ms_per_step = 50;

    // Factory: carga desde fichero JSON. Devuelve defaults si no existe.
    static GuiConfig load(const std::string& path);
};
