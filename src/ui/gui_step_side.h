// gui_step_side.h — Pantalla 2/3: configuracion de bando (placeholder)
#pragma once

#include "gui_state.h"
#include "imgui.h"

inline void render_step_side(AppState& app, bool is_blue) {
    const char* title = is_blue ? "Bando Azul (Propio)" : "Bando Rojo (Enemigo)";
    ImVec4 accent = is_blue ? colors::blue_side : colors::red_side;

    ImGui::PushStyleColor(ImGuiCol_Text, accent);
    ImGui::SetWindowFontScale(1.3f);
    ImGui::Text("%s", title);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::Spacing(); ImGui::Spacing();
    ImGui::TextColored(colors::text_secondary, "Pantalla pendiente de implementacion (Fase G4).");
    ImGui::Spacing(); ImGui::Spacing();

    // Navegacion
    float btn_w = 160;
    if (ImGui::Button("<<  ANTERIOR", ImVec2(btn_w, 36))) {
        app.current_step = is_blue ? WizardStep::SCENARIO : WizardStep::BLUE_SIDE;
    }
    ImGui::SameLine(0, 20);

    WizardStep next = is_blue ? WizardStep::RED_SIDE : WizardStep::SIMULATION;
    bool can_advance = is_blue ? true : app.isStepAccessible(WizardStep::SIMULATION);

    if (!can_advance) ImGui::BeginDisabled();
    ImGui::PushStyleColor(ImGuiCol_Button, colors::step_active);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.35f, 0.62f, 0.90f, 1.0f});
    if (ImGui::Button("SIGUIENTE  >>", ImVec2(btn_w, 36)))
        app.current_step = next;
    ImGui::PopStyleColor(2);
    if (!can_advance) ImGui::EndDisabled();
}
