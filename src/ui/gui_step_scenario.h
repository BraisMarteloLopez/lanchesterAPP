// gui_step_scenario.h — Pantalla 1: configuracion del escenario
#pragma once

#include "gui_state.h"
#include "imgui.h"

// Descripciones de terreno para las tarjetas
static const char* TERRAIN_NAMES[] = {"FACIL", "MEDIO", "DIFICIL"};
static const char* TERRAIN_DESC[]  = {
    "Terreno abierto.\nLinea de vision despejada.\nMaxima efectividad de fuego.",
    "Terreno mixto.\nVegetacion y ondulaciones.\nEfectividad moderada.",
    "Terreno cerrado.\nBosque, urbano, montana.\nEfectividad reducida."
};

static const char* MOBILITY_NAMES[] = {"MUY_ALTA", "ALTA", "MEDIA", "BAJA"};
static const char* AGG_NAMES[] = {"PRE (por defecto)", "POST (mas realista)"};

inline void render_step_scenario(AppState& app) {
    float content_w = ImGui::GetContentRegionAvail().x;
    float center_w = content_w * 0.7f;
    float margin = (content_w - center_w) * 0.5f;

    ImGui::SetCursorPosX(margin);
    ImGui::BeginGroup();

    // --- Titulo ---
    ImGui::PushStyleColor(ImGuiCol_Text, colors::text_primary);
    ImGui::SetWindowFontScale(1.3f);
    ImGui::Text("Configuracion del Escenario");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::Spacing(); ImGui::Spacing();

    // --- Terreno (tarjetas seleccionables) ---
    ImGui::Text("TERRENO");
    ImGui::Spacing();
    {
        float card_w = (center_w - ImGui::GetStyle().ItemSpacing.x * 2) / 3.0f;
        float card_h = 90.0f;

        for (int i = 0; i < 3; ++i) {
            if (i > 0) ImGui::SameLine();

            bool selected = (app.terrain_idx == i);
            ImVec4 bg = selected ? colors::step_active : colors::panel;
            ImVec4 border = selected ? colors::step_active : colors::step_blocked;

            ImGui::PushID(i);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);

            ImGui::BeginChild("##terrain_card", ImVec2(card_w, card_h),
                              ImGuiChildFlags_Border);

            // Zona clickable invisible
            ImGui::SetCursorPos({0, 0});
            if (ImGui::InvisibleButton("##sel", ImVec2(card_w, card_h)))
                app.terrain_idx = i;

            // Contenido de la tarjeta
            ImGui::SetCursorPos({10, 8});
            ImGui::PushStyleColor(ImGuiCol_Text, selected ? ImVec4{1,1,1,1} : colors::text_primary);
            ImGui::SetWindowFontScale(1.1f);
            ImGui::Text("%s", TERRAIN_NAMES[i]);
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();

            ImGui::SetCursorPos({10, 32});
            ImGui::PushStyleColor(ImGuiCol_Text, selected ? ImVec4{0.9f,0.9f,0.9f,1} : colors::text_secondary);
            ImGui::TextWrapped("%s", TERRAIN_DESC[i]);
            ImGui::PopStyleColor();

            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
    }

    ImGui::Spacing(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    // --- Distancia ---
    ImGui::Text("DISTANCIA DE EMPENAMIENTO");
    ImGui::SetNextItemWidth(center_w);
    ImGui::SliderFloat("##dist", &app.distance_m, 100.0f, 9000.0f, "%.0f m");

    ImGui::Spacing(); ImGui::Spacing();

    // --- Modelo de simulacion ---
    ImGui::Text("MODELO DE SIMULACION");
    ImGui::SetNextItemWidth(center_w);
    if (!app.model_names.empty()) {
        if (ImGui::BeginCombo("##model", app.model_names[app.model_idx].c_str())) {
            for (int i = 0; i < static_cast<int>(app.model_names.size()); ++i) {
                bool sel = (app.model_idx == i);
                if (ImGui::Selectable(app.model_names[i].c_str(), sel))
                    app.model_idx = i;
            }
            ImGui::EndCombo();
        }
    }

    ImGui::Spacing(); ImGui::Spacing();

    // --- Modo ---
    ImGui::Text("MODO");
    ImGui::RadioButton("Determinista", &app.mode, 0);
    ImGui::SameLine(0, 30);
    ImGui::RadioButton("Monte Carlo", &app.mode, 1);

    if (app.mode == 1) {
        ImGui::Indent(20);
        ImGui::SetNextItemWidth(140);
        ImGui::InputInt("Replicas", &app.mc_replicas, 100, 1000);
        if (app.mc_replicas < 10) app.mc_replicas = 10;
        ImGui::SameLine(0, 20);
        ImGui::SetNextItemWidth(100);
        ImGui::InputInt("Seed", &app.mc_seed);
        ImGui::Unindent(20);
    }

    ImGui::Spacing(); ImGui::Spacing();

    // --- Agregacion ---
    ImGui::Text("AGREGACION");
    ImGui::RadioButton(AGG_NAMES[0], &app.aggregation_idx, 0);
    ImGui::SameLine(0, 30);
    ImGui::RadioButton(AGG_NAMES[1], &app.aggregation_idx, 1);

    ImGui::Spacing(); ImGui::Spacing();

    // --- Tiempo maximo ---
    ImGui::Text("TIEMPO MAXIMO DE COMBATE");
    ImGui::SetNextItemWidth(center_w);
    ImGui::SliderFloat("##tmax", &app.t_max_minutes, 5.0f, 120.0f, "%.0f min");

    ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();

    // --- Boton siguiente ---
    float btn_w = 180;
    ImGui::SetCursorPosX(margin + center_w - btn_w);
    ImGui::PushStyleColor(ImGuiCol_Button, colors::step_active);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.35f, 0.62f, 0.90f, 1.0f});
    if (ImGui::Button("SIGUIENTE  >>", ImVec2(btn_w, 36)))
        app.current_step = WizardStep::BLUE_SIDE;
    ImGui::PopStyleColor(2);

    ImGui::EndGroup();
}
