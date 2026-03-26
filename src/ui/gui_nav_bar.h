// gui_nav_bar.h — Barra de navegacion tipo stepper
#pragma once

#include "gui_state.h"
#include "imgui.h"

inline void render_nav_bar(AppState& app) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 bar_pos = ImGui::GetCursorScreenPos();
    float total_w = ImGui::GetContentRegionAvail().x;
    float bar_h = 52.0f;

    // Fondo de la barra
    dl->AddRectFilled(bar_pos, {bar_pos.x + total_w, bar_pos.y + bar_h},
                      ImGui::ColorConvertFloat4ToU32(colors::nav_bar), 0);

    float step_w = total_w / WIZARD_STEP_COUNT;
    float circle_r = 12.0f;
    float cy = bar_pos.y + bar_h * 0.5f;

    // Calcular centro X de cada paso
    float centers_x[WIZARD_STEP_COUNT];
    for (int i = 0; i < WIZARD_STEP_COUNT; ++i)
        centers_x[i] = bar_pos.x + step_w * i + step_w * 0.35f;

    // Lineas conectoras (dibujar primero, debajo de los circulos)
    for (int i = 0; i < WIZARD_STEP_COUNT - 1; ++i) {
        bool segment_done = app.isStepComplete(static_cast<WizardStep>(i));
        ImVec4 line_col = segment_done ? colors::step_complete : ImVec4{0.30f, 0.30f, 0.33f, 1.0f};
        float lx0 = centers_x[i] + circle_r + 4;
        float lx1 = centers_x[i + 1] - circle_r - 4;
        dl->AddLine({lx0, cy}, {lx1, cy},
                    ImGui::ColorConvertFloat4ToU32(line_col), 2.0f);
    }

    // Pasos
    for (int i = 0; i < WIZARD_STEP_COUNT; ++i) {
        auto step = static_cast<WizardStep>(i);
        bool is_current = (app.current_step == step);
        bool is_complete = app.isStepComplete(step) && !is_current;
        bool is_accessible = app.isStepAccessible(step);

        float cx = centers_x[i];

        // Color del circulo
        ImVec4 fill_col;
        if (is_current)       fill_col = colors::step_active;
        else if (is_complete) fill_col = colors::step_complete;
        else                  fill_col = ImVec4{0.25f, 0.25f, 0.28f, 1.0f};

        // Halo para paso activo
        if (is_current) {
            ImVec4 halo = {fill_col.x, fill_col.y, fill_col.z, 0.25f};
            dl->AddCircleFilled({cx, cy}, circle_r + 4,
                                ImGui::ColorConvertFloat4ToU32(halo));
        }

        dl->AddCircleFilled({cx, cy}, circle_r,
                            ImGui::ColorConvertFloat4ToU32(fill_col));

        // Contenido del circulo
        ImU32 white = ImGui::ColorConvertFloat4ToU32({1,1,1,1});
        if (is_complete) {
            // Check mark
            dl->AddLine({cx - 4, cy + 1}, {cx - 1, cy + 4}, white, 2.2f);
            dl->AddLine({cx - 1, cy + 4}, {cx + 5, cy - 3}, white, 2.2f);
        } else {
            // Numero
            char num[4];
            snprintf(num, sizeof(num), "%d", i + 1);
            ImVec2 tsz = ImGui::CalcTextSize(num);
            dl->AddText({cx - tsz.x * 0.5f, cy - tsz.y * 0.5f}, white, num);
        }

        // Etiqueta del paso
        const char* label = WIZARD_STEP_NAMES[i];
        ImVec2 label_sz = ImGui::CalcTextSize(label);
        float lx = cx + circle_r + 8;
        float ly = cy - label_sz.y * 0.5f;

        ImVec4 text_col;
        if (is_current)       text_col = colors::step_active;
        else if (is_complete) text_col = colors::step_complete;
        else if (is_accessible) text_col = colors::text_primary;
        else                  text_col = colors::step_blocked;

        dl->AddText({lx, ly}, ImGui::ColorConvertFloat4ToU32(text_col), label);

        // Zona clickable invisible
        if (is_accessible && !is_current) {
            ImGui::SetCursorScreenPos({cx - circle_r, cy - circle_r});
            ImGui::PushID(i);
            if (ImGui::InvisibleButton("##step", {circle_r * 2 + 8 + label_sz.x + 8, circle_r * 2}))
                app.current_step = step;
            ImGui::PopID();
        }
    }

    // Reservar espacio para la barra
    ImGui::SetCursorScreenPos({bar_pos.x, bar_pos.y + bar_h});
    ImGui::Dummy(ImVec2(0, 0));
}
