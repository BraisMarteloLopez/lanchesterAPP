// gui_nav_bar.h — Barra de navegacion tipo stepper
#pragma once

#include "gui_state.h"
#include "imgui.h"

inline void render_nav_bar(AppState& app) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, colors::nav_bar);
    ImGui::BeginChild("##nav_bar", ImVec2(-1, 56), ImGuiChildFlags_None);

    float total_w = ImGui::GetContentRegionAvail().x;
    float step_w = total_w / WIZARD_STEP_COUNT;

    for (int i = 0; i < WIZARD_STEP_COUNT; ++i) {
        auto step = static_cast<WizardStep>(i);
        bool is_current = (app.current_step == step);
        bool is_complete = app.isStepComplete(step) && !is_current;
        bool is_accessible = app.isStepAccessible(step);

        if (i > 0) ImGui::SameLine();

        ImGui::BeginGroup();

        // Posicion centrada en el slot
        float slot_start = i * step_w;
        float text_w = ImGui::CalcTextSize(WIZARD_STEP_NAMES[i]).x;
        float circle_r = 10.0f;
        float total_item_w = circle_r * 2 + 8 + text_w;
        float offset_x = (step_w - total_item_w) * 0.5f;
        if (i > 0) offset_x -= ImGui::GetStyle().ItemSpacing.x * 0.5f;

        ImGui::SetCursorPosY(14);

        // Circulo indicador
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        cursor.x += offset_x;
        ImVec2 center = {cursor.x + circle_r, cursor.y + circle_r};
        ImDrawList* dl = ImGui::GetWindowDrawList();

        ImVec4 circle_col;
        if (is_current)       circle_col = colors::step_active;
        else if (is_complete) circle_col = colors::step_complete;
        else                  circle_col = colors::step_blocked;

        dl->AddCircleFilled(center, circle_r, ImGui::ColorConvertFloat4ToU32(circle_col));

        // Check mark para completados
        if (is_complete && !is_current) {
            ImU32 white = ImGui::ColorConvertFloat4ToU32({1,1,1,1});
            dl->AddLine({center.x - 4, center.y}, {center.x - 1, center.y + 4}, white, 2.0f);
            dl->AddLine({center.x - 1, center.y + 4}, {center.x + 5, center.y - 3}, white, 2.0f);
        } else {
            // Numero del paso
            char num[4];
            snprintf(num, sizeof(num), "%d", i + 1);
            float num_w = ImGui::CalcTextSize(num).x;
            dl->AddText({center.x - num_w * 0.5f, center.y - 7},
                        ImGui::ColorConvertFloat4ToU32({1,1,1,1}), num);
        }

        // Linea conectora al siguiente paso
        if (i < WIZARD_STEP_COUNT - 1) {
            float line_y = center.y;
            float line_start = center.x + circle_r + 4;
            float line_end = slot_start + step_w - 4;
            ImVec4 line_col = is_complete ? colors::step_complete : colors::step_blocked;
            dl->AddLine({line_start, line_y}, {line_end + cursor.x - slot_start, line_y},
                        ImGui::ColorConvertFloat4ToU32(line_col), 2.0f);
        }

        // Nombre del paso (clickable)
        float label_x = cursor.x + circle_r * 2 + 8;
        ImGui::SetCursorScreenPos({label_x, cursor.y - 1});

        if (is_accessible && !is_current) {
            ImGui::PushStyleColor(ImGuiCol_Text, is_complete ? colors::step_complete : colors::text_primary);
            if (ImGui::Selectable(WIZARD_STEP_NAMES[i], false, 0, {text_w + 4, 22}))
                app.current_step = step;
            ImGui::PopStyleColor();
        } else if (is_current) {
            ImGui::PushStyleColor(ImGuiCol_Text, colors::step_active);
            ImGui::Text("%s", WIZARD_STEP_NAMES[i]);
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, colors::step_blocked);
            ImGui::Text("%s", WIZARD_STEP_NAMES[i]);
            ImGui::PopStyleColor();
        }

        ImGui::EndGroup();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}
