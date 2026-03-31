// gui_step_side.h — Pantalla 2/3: configuracion de bando
#pragma once

#include "gui_state.h"
#include "imgui.h"

// ---------------------------------------------------------------------------
// Tabla de composicion de fuerzas
// ---------------------------------------------------------------------------

inline void render_composition_table(GuiSideConfig& side,
                                      const std::vector<std::string>& vehicle_names,
                                      const IVehicleCatalog& catalog,
                                      ImVec4 accent) {
    ImGui::PushStyleColor(ImGuiCol_Text, colors::text_primary);
    ImGui::Text("COMPOSICION DE FUERZAS");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    float avail_w = ImGui::GetContentRegionAvail().x;

    for (int i = 0; i < side.num_types; ++i) {
        ImGui::PushID(i);

        // Selector de vehiculo
        float combo_w = avail_w * 0.55f;
        ImGui::SetNextItemWidth(combo_w);
        const char* preview = vehicle_names.empty() ? "---"
            : vehicle_names[side.vehicle_idx[i]].c_str();

        if (ImGui::BeginCombo("##veh", preview)) {
            for (int v = 0; v < static_cast<int>(vehicle_names.size()); ++v) {
                bool selected = (side.vehicle_idx[i] == v);
                if (ImGui::Selectable(vehicle_names[v].c_str(), selected))
                    side.vehicle_idx[i] = v;

                if (ImGui::IsItemHovered() && catalog.contains(vehicle_names[v])) {
                    const auto& vp = catalog.find(vehicle_names[v]);
                    ImGui::SetTooltip(
                        "Blindaje (D): %.0f\n"
                        "Penetracion (P): %.0f\n"
                        "Punteria (U): %.2f\n"
                        "Cadencia (c): %.1f disp/min\n"
                        "Alcance max: %.0f m\n"
                        "Contracarro: %s",
                        vp.D, vp.P, vp.U, vp.c, vp.A_max,
                        vp.CC ? "Si" : "No");
                }
            }
            ImGui::EndCombo();
        }

        // Cantidad
        ImGui::SameLine();
        ImGui::SetNextItemWidth(avail_w * 0.20f);
        ImGui::InputInt("##cnt", &side.vehicle_count[i], 1, 5);
        if (side.vehicle_count[i] < 0) side.vehicle_count[i] = 0;

        // Boton eliminar
        if (side.num_types > 1) {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.5f, 0.15f, 0.15f, 1.0f});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.65f, 0.2f, 0.2f, 1.0f});
            if (ImGui::Button("X", ImVec2(28, 0))) {
                for (int j = i; j < side.num_types - 1; ++j) {
                    side.vehicle_idx[j] = side.vehicle_idx[j + 1];
                    side.vehicle_count[j] = side.vehicle_count[j + 1];
                }
                side.num_types--;
                i--;
            }
            ImGui::PopStyleColor(2);
        }

        ImGui::PopID();
    }

    // Boton anadir
    if (side.num_types < GuiSideConfig::MAX_TYPES) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{accent.x * 0.4f, accent.y * 0.4f, accent.z * 0.4f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{accent.x * 0.55f, accent.y * 0.55f, accent.z * 0.55f, 1.0f});
        if (ImGui::Button("+ Anadir unidad", ImVec2(160, 30))) {
            side.vehicle_idx[side.num_types] = 0;
            side.vehicle_count[side.num_types] = 5;
            side.num_types++;
        }
        ImGui::PopStyleColor(2);
    }
}

// ---------------------------------------------------------------------------
// Parametros tacticos y avanzados
// ---------------------------------------------------------------------------

inline void render_tactical_params(GuiSideConfig& side,
                                    const std::vector<std::string>& tactical_names,
                                    float content_w) {
    // Estado tactico
    ImGui::Text("ESTADO TACTICO");
    ImGui::SetNextItemWidth(content_w);
    if (!tactical_names.empty()) {
        if (side.tactical_state_idx >= static_cast<int>(tactical_names.size()))
            side.tactical_state_idx = 0;
        if (ImGui::BeginCombo("##tac", tactical_names[side.tactical_state_idx].c_str())) {
            for (int i = 0; i < static_cast<int>(tactical_names.size()); ++i) {
                bool sel = (side.tactical_state_idx == i);
                if (ImGui::Selectable(tactical_names[i].c_str(), sel))
                    side.tactical_state_idx = i;
            }
            ImGui::EndCombo();
        }
    }

    ImGui::Spacing(); ImGui::Spacing();

    // Movilidad
    static const char* MOB_NAMES[] = {"MUY_ALTA", "ALTA", "MEDIA", "BAJA"};
    ImGui::Text("MOVILIDAD");
    ImGui::SetNextItemWidth(content_w);
    ImGui::Combo("##mob", &side.mobility_idx, MOB_NAMES, IM_ARRAYSIZE(MOB_NAMES));

    ImGui::Spacing(); ImGui::Spacing();

    // Parametros avanzados
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4{0.20f, 0.20f, 0.23f, 1.0f});
    if (ImGui::TreeNode("Parametros avanzados")) {
        ImGui::Spacing();
        ImGui::SetNextItemWidth(content_w * 0.6f);
        ImGui::SliderFloat("Bajas AFT (%)", &side.aft_casualties_pct, 0.0f, 1.0f, "%.0f%%");

        ImGui::SetNextItemWidth(content_w * 0.6f);
        ImGui::SliderFloat("Fraccion empenamiento", &side.engagement_fraction, 0.1f, 1.0f, "%.2f");

        ImGui::SetNextItemWidth(content_w * 0.6f);
        ImGui::SliderFloat("Factor cadencia", &side.rate_factor, 0.1f, 3.0f, "%.2f");


        ImGui::TreePop();
    }
    ImGui::PopStyleColor();
}

// ---------------------------------------------------------------------------
// Pantalla completa de bando
// ---------------------------------------------------------------------------

inline void render_step_side(AppState& app, bool is_blue) {
    const char* title = is_blue ? "Bando Azul (Propio)" : "Bando Rojo (Enemigo)";
    ImVec4 accent = is_blue ? colors::blue_side : colors::red_side;

    GuiSideConfig& side = is_blue ? app.blue : app.red;
    const auto& vehicle_names = is_blue ? app.blue_names : app.red_names;

    float content_w = ImGui::GetContentRegionAvail().x;
    float center_w = content_w * 0.7f;
    float margin = (content_w - center_w) * 0.5f;

    ImGui::SetCursorPosX(margin);
    ImGui::BeginGroup();

    // Titulo con color de bando
    ImGui::PushStyleColor(ImGuiCol_Text, accent);
    ImGui::SetWindowFontScale(1.3f);
    ImGui::Text("%s", title);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    // Linea decorativa del color del bando
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(
        p, {p.x + center_w, p.y + 3},
        ImGui::ColorConvertFloat4ToU32(accent));
    ImGui::Dummy(ImVec2(0, 8));

    ImGui::Spacing();

    // Composicion
    render_composition_table(side, vehicle_names,
        is_blue ? app.service->blueCatalog() : app.service->redCatalog(),
        accent);

    ImGui::Spacing(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    // Tacticos
    render_tactical_params(side, app.tactical_state_names, center_w);

    ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();

    // Navegacion
    float btn_w = 160;
    if (ImGui::Button("<<  ANTERIOR", ImVec2(btn_w, 36)))
        app.current_step = is_blue ? WizardStep::SCENARIO : WizardStep::BLUE_SIDE;

    ImGui::SameLine(0, 20);

    WizardStep next = is_blue ? WizardStep::RED_SIDE : WizardStep::SIMULATION;
    GuiSideConfig& current_side = is_blue ? app.blue : app.red;
    bool can_advance = current_side.isValid() && app.isStepAccessible(next);

    if (!can_advance) ImGui::BeginDisabled();
    ImGui::PushStyleColor(ImGuiCol_Button, colors::step_active);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.35f, 0.62f, 0.90f, 1.0f});
    if (ImGui::Button("SIGUIENTE  >>", ImVec2(btn_w, 36)))
        app.current_step = next;
    ImGui::PopStyleColor(2);
    if (!can_advance) ImGui::EndDisabled();

    if (!current_side.isValid()) {
        ImGui::SameLine(0, 20);
        ImGui::TextColored(colors::text_secondary, "Configura al menos 1 unidad");
    }

    ImGui::EndGroup();
}
