// gui_step_simulation.h — Pantalla 4: simulacion + resultado (placeholder)
#pragma once

#include "gui_state.h"
#include "imgui.h"

inline void render_step_simulation(AppState& app) {
    ImGui::PushStyleColor(ImGuiCol_Text, colors::text_primary);
    ImGui::SetWindowFontScale(1.3f);
    ImGui::Text("Simulacion");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::Spacing(); ImGui::Spacing();
    ImGui::TextColored(colors::text_secondary, "Pantalla pendiente de implementacion (Fase G5).");
    ImGui::Spacing();
    ImGui::TextColored(colors::text_secondary,
        "Aqui ira: resumen escenario, grafica 2D evolutiva, metricas, boton EJECUTAR.");
    ImGui::Spacing(); ImGui::Spacing();

    // Boton volver
    if (ImGui::Button("<<  VOLVER A CONFIGURACION", ImVec2(240, 36)))
        app.current_step = WizardStep::SCENARIO;
}
