// gui_step_simulation.h — Pantalla 4: simulacion, grafica 2D y metricas
#pragma once

#include "gui_state.h"
#include "imgui.h"
#include "implot.h"

#include <chrono>

// ---------------------------------------------------------------------------
// Construir ScenarioConfig desde el estado de la GUI
// ---------------------------------------------------------------------------

static const char* SIM_TERRAIN_NAMES[] = {"FACIL", "MEDIO", "DIFICIL"};

inline ScenarioConfig buildScenarioConfig(const AppState& app) {
    ScenarioConfig config;
    config.scenario_id = "GUI-SCENARIO";
    config.terrain = static_cast<Terrain>(app.terrain_idx);
    config.distance_m = static_cast<double>(app.distance_m);
    config.t_max = static_cast<double>(app.t_max_minutes);
    config.h = lanchester::DEFAULT_TIMESTEP;
    config.aggregation = app.aggregation_idx == 0
        ? AggregationMode::PRE : AggregationMode::POST;

    auto build_side = [&](const GuiSideConfig& gui, bool is_blue) {
        ::SideConfig side;
        if (gui.tactical_state_idx < static_cast<int>(app.tactical_state_names.size()))
            side.tactical_state = app.tactical_state_names[gui.tactical_state_idx];
        side.mobility = static_cast<Mobility>(gui.mobility_idx);
        side.aft_pct = static_cast<double>(gui.aft_casualties_pct);
        side.engagement_fraction = static_cast<double>(gui.engagement_fraction);
        side.rate_factor = static_cast<double>(gui.rate_factor);

        const auto& catalog = is_blue
            ? app.service->blueCatalog() : app.service->redCatalog();
        const auto& names = is_blue ? app.blue_names : app.red_names;
        for (int i = 0; i < gui.num_types; ++i) {
            if (gui.vehicle_count[i] > 0 &&
                gui.vehicle_idx[i] < static_cast<int>(names.size())) {
                CompositionEntry ce;
                ce.vehicle = catalog.find(names[gui.vehicle_idx[i]]);
                ce.count = gui.vehicle_count[i];
                side.composition.push_back(ce);
            }
        }
        return side;
    };

    config.blue = build_side(app.blue, true);
    config.red = build_side(app.red, false);
    return config;
}

// ---------------------------------------------------------------------------
// Resumen del escenario (cabecera)
// ---------------------------------------------------------------------------

inline void render_scenario_summary(const AppState& app, float width) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4{0.13f, 0.13f, 0.15f, 1.0f});
    ImGui::BeginChild("##summary", ImVec2(width, 80), ImGuiChildFlags_Border);

    ImGui::Columns(3, nullptr, false);

    // Columna izquierda: escenario
    ImGui::TextColored(colors::text_secondary, "Escenario");
    ImGui::Text("Terreno: %s  |  Distancia: %.0f m",
        SIM_TERRAIN_NAMES[app.terrain_idx], app.distance_m);
    ImGui::Text("Modo: %s  |  T max: %.0f min",
        app.mode == 0 ? "Determinista" : "Monte Carlo",
        app.t_max_minutes);

    // Columna central: azul
    ImGui::NextColumn();
    ImGui::TextColored(colors::blue_side, "Bando Azul");
    for (int i = 0; i < app.blue.num_types; ++i) {
        if (app.blue.vehicle_count[i] > 0 && app.blue.vehicle_idx[i] < static_cast<int>(app.blue_names.size()))
            ImGui::Text("  %d x %s", app.blue.vehicle_count[i],
                app.blue_names[app.blue.vehicle_idx[i]].c_str());
    }

    // Columna derecha: rojo
    ImGui::NextColumn();
    ImGui::TextColored(colors::red_side, "Bando Rojo");
    for (int i = 0; i < app.red.num_types; ++i) {
        if (app.red.vehicle_count[i] > 0 && app.red.vehicle_idx[i] < static_cast<int>(app.red_names.size()))
            ImGui::Text("  %d x %s", app.red.vehicle_count[i],
                app.red_names[app.red.vehicle_idx[i]].c_str());
    }

    ImGui::Columns(1);
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ---------------------------------------------------------------------------
// Grafica 2D evolutiva
// ---------------------------------------------------------------------------

inline void render_evolution_chart(AppState& app, float width, float height) {
    const auto& ts = app.has_result
        ? app.result.combats[0].time_series
        : std::vector<TimeStep>{};

    // Determinar cuantos puntos mostrar (animacion)
    int total_points = static_cast<int>(ts.size());
    int show_points = total_points;

    if (app.animating && total_points > 0) {
        show_points = std::min(app.anim_step, total_points);
        if (show_points >= total_points) {
            app.animating = false;
        }
    }

    if (ImPlot::BeginPlot("Evolucion del Combate", ImVec2(width, height))) {
        ImPlot::SetupAxes("Tiempo (min)", "Efectivos");

        if (show_points > 0) {
            // Extraer datos para implot
            std::vector<double> t_data(show_points);
            std::vector<double> blue_data(show_points);
            std::vector<double> red_data(show_points);

            for (int i = 0; i < show_points; ++i) {
                t_data[i] = ts[i].t;
                blue_data[i] = ts[i].blue_forces;
                red_data[i] = ts[i].red_forces;
            }

            // Limites de ejes
            double t_max = t_data.back();
            double y_max = std::max(ts[0].blue_forces, ts[0].red_forces) * 1.1;
            ImPlot::SetupAxisLimits(ImAxis_X1, 0, std::max(1.0, t_max * 1.05),
                                    ImPlotCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0, std::max(1.0, y_max),
                                    ImPlotCond_Always);

            // Curvas
            ImPlot::SetNextLineStyle(
                ImVec4{colors::blue_side.x, colors::blue_side.y, colors::blue_side.z, 1.0f}, 2.5f);
            ImPlot::PlotLine("Azul", t_data.data(), blue_data.data(), show_points);

            ImPlot::SetNextLineStyle(
                ImVec4{colors::red_side.x, colors::red_side.y, colors::red_side.z, 1.0f}, 2.5f);
            ImPlot::PlotLine("Rojo", t_data.data(), red_data.data(), show_points);

            // Area bajo las curvas (efecto visual sutil)
            ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.15f);
            ImPlot::SetNextFillStyle(
                ImVec4{colors::blue_side.x, colors::blue_side.y, colors::blue_side.z, 1.0f});
            ImPlot::PlotShaded("##blue_shade", t_data.data(), blue_data.data(), show_points);
            ImPlot::SetNextFillStyle(
                ImVec4{colors::red_side.x, colors::red_side.y, colors::red_side.z, 1.0f});
            ImPlot::PlotShaded("##red_shade", t_data.data(), red_data.data(), show_points);
            ImPlot::PopStyleVar();
        } else {
            ImPlot::SetupAxisLimits(ImAxis_X1, 0, 30);
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 20);
        }

        ImPlot::EndPlot();
    }
}

// ---------------------------------------------------------------------------
// Metricas deterministas
// ---------------------------------------------------------------------------

inline void render_deterministic_metrics(const CombatResult& r) {
    // Outcome con color
    ImVec4 outcome_col;
    if (r.outcome == Outcome::BLUE_WINS)       outcome_col = colors::blue_side;
    else if (r.outcome == Outcome::RED_WINS)    outcome_col = colors::red_side;
    else if (r.outcome == Outcome::DRAW)        outcome_col = colors::outcome_draw;
    else                                        outcome_col = colors::text_secondary;

    ImGui::TextColored(outcome_col, "RESULTADO: %s", outcome_str(r.outcome));
    ImGui::Spacing();

    if (ImGui::BeginTable("##det_metrics", 3,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Metrica", ImGuiTableColumnFlags_WidthFixed, 180);
        ImGui::TableSetupColumn("Azul");
        ImGui::TableSetupColumn("Rojo");
        ImGui::TableHeadersRow();

        auto row = [](const char* label, double blue, double red) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(label);
            ImGui::TableSetColumnIndex(1); ImGui::Text("%.2f", blue);
            ImGui::TableSetColumnIndex(2); ImGui::Text("%.2f", red);
        };

        row("Fuerzas iniciales", r.blue_initial, r.red_initial);
        row("Supervivientes", r.blue_survivors, r.red_survivors);
        row("Bajas", r.blue_casualties, r.red_casualties);
        row("Municion conv.", r.blue_ammo_consumed, r.red_ammo_consumed);
        row("Municion C/C", r.blue_cc_ammo_consumed, r.red_cc_ammo_consumed);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Duracion (min)");
        ImGui::TableSetColumnIndex(1); ImGui::Text("%.2f", r.duration_contact_minutes);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Ventaja estatica");
        ImGui::TableSetColumnIndex(1); ImGui::Text("%.4f", r.static_advantage);

        ImGui::EndTable();
    }
}

// ---------------------------------------------------------------------------
// Metricas Monte Carlo
// ---------------------------------------------------------------------------

inline void render_mc_metrics(const MonteCarloScenarioOutput& mc_out) {
    ImGui::Text("Monte Carlo: %d replicas (seed=%llu)",
        mc_out.n_replicas, (unsigned long long)mc_out.seed);
    ImGui::Spacing();

    for (const auto& mc : mc_out.combats) {
        int n = mc.n_replicas;
        double bw = n > 0 ? static_cast<double>(mc.count_blue_wins) / n : 0;
        double rw = n > 0 ? static_cast<double>(mc.count_red_wins) / n : 0;
        double dw = n > 0 ? static_cast<double>(mc.count_draw) / n : 0;

        // Probabilidades
        ImGui::TextColored(colors::blue_side, "P(Azul gana) = %.1f%%", bw * 100);
        ImGui::SameLine(0, 20);
        ImGui::TextColored(colors::red_side, "P(Rojo gana) = %.1f%%", rw * 100);
        ImGui::SameLine(0, 20);
        ImGui::TextColored(colors::outcome_draw, "P(Empate) = %.1f%%", dw * 100);

        ImGui::Spacing();

        // Grafica de distribucion
        if (ImPlot::BeginPlot("Distribucion de resultados", ImVec2(-1, 140))) {
            ImPlot::SetupAxes("", "Probabilidad");
            double positions[] = {0, 1, 2};
            double probs[] = {bw, rw, dw};
            const char* labels[] = {"Azul gana", "Rojo gana", "Empate"};
            ImPlot::SetupAxisTicks(ImAxis_X1, positions, 3, labels);
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 1.05);

            ImVec4 bar_colors[] = {colors::blue_side, colors::red_side, colors::outcome_draw};
            for (int i = 0; i < 3; ++i) {
                ImPlot::SetNextFillStyle(bar_colors[i]);
                ImPlot::PlotBars(labels[i], &positions[i], &probs[i], 1, 0.6);
            }
            ImPlot::EndPlot();
        }

        ImGui::Spacing();

        // Tabla de percentiles
        if (ImGui::BeginTable("##mc_stats", 4,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Estadistica");
            ImGui::TableSetupColumn("Azul superv.");
            ImGui::TableSetupColumn("Rojo superv.");
            ImGui::TableSetupColumn("Duracion (min)");
            ImGui::TableHeadersRow();

            auto stat_row = [](const char* label, double b, double r, double d) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(label);
                ImGui::TableSetColumnIndex(1); ImGui::Text("%.2f", b);
                ImGui::TableSetColumnIndex(2); ImGui::Text("%.2f", r);
                ImGui::TableSetColumnIndex(3); ImGui::Text("%.2f", d);
            };

            stat_row("Media", mc.blue_survivors.mean, mc.red_survivors.mean, mc.duration.mean);
            stat_row("Std", mc.blue_survivors.std, mc.red_survivors.std, mc.duration.std);
            stat_row("P05", mc.blue_survivors.p05, mc.red_survivors.p05, mc.duration.p05);
            stat_row("P25", mc.blue_survivors.p25, mc.red_survivors.p25, mc.duration.p25);
            stat_row("Mediana", mc.blue_survivors.median, mc.red_survivors.median, mc.duration.median);
            stat_row("P75", mc.blue_survivors.p75, mc.red_survivors.p75, mc.duration.p75);
            stat_row("P95", mc.blue_survivors.p95, mc.red_survivors.p95, mc.duration.p95);

            // Referencia determinista
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(colors::text_secondary, "Determin.");
            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(colors::text_secondary, "%.2f", mc.deterministic.blue_survivors);
            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(colors::text_secondary, "%.2f", mc.deterministic.red_survivors);
            ImGui::TableSetColumnIndex(3);
            ImGui::TextColored(colors::text_secondary, "%.2f", mc.deterministic.duration_contact_minutes);

            ImGui::EndTable();
        }
    }
}

// ---------------------------------------------------------------------------
// Pantalla completa de simulacion
// ---------------------------------------------------------------------------

inline void render_step_simulation(AppState& app) {
    float content_w = ImGui::GetContentRegionAvail().x;
    float center_w = content_w * 0.85f;
    float margin = (content_w - center_w) * 0.5f;

    ImGui::SetCursorPosX(margin);
    ImGui::BeginGroup();

    // Titulo
    ImGui::PushStyleColor(ImGuiCol_Text, colors::text_primary);
    ImGui::SetWindowFontScale(1.3f);
    ImGui::Text("Simulacion");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // Resumen del escenario
    render_scenario_summary(app, center_w);
    ImGui::Spacing();

    // Boton EJECUTAR
    if (app.running) {
        // Spinner animado
        const char* spinner_frames[] = {"|", "/", "-", "\\"};
        int frame = static_cast<int>(ImGui::GetTime() * 8.0) % 4;
        char buf[64];
        snprintf(buf, sizeof(buf), "  %s  Simulando...  %s  ", spinner_frames[frame], spinner_frames[frame]);
        ImGui::BeginDisabled();
        ImGui::Button(buf, ImVec2(center_w, 44));
        ImGui::EndDisabled();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, colors::btn_execute);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colors::btn_execute_h);
        if (ImGui::Button("EJECUTAR SIMULACION", ImVec2(center_w, 44))) {
            app.error_msg[0] = '\0';
            app.has_result = false;
            app.has_mc_result = false;
            app.animating = false;
            app.anim_step = 0;
            try {
                auto config = buildScenarioConfig(app);
                if (app.mode == 0) {
                    app.running = true;
                    app.future_result = app.service->runScenarioAsync(std::move(config));
                } else {
                    app.running = true;
                    app.future_mc = app.service->runMonteCarloAsync(
                        std::move(config), app.mc_replicas,
                        static_cast<uint64_t>(app.mc_seed));
                }
            } catch (const std::exception& e) {
                std::snprintf(app.error_msg, sizeof(app.error_msg), "Error: %s", e.what());
            }
        }
        ImGui::PopStyleColor(2);
    }

    ImGui::Spacing();

    // Grafica 2D evolutiva (solo modo determinista con resultado)
    if (app.mode == 0 && app.has_result && !app.result.combats.empty()) {
        // Iniciar animacion al recibir resultado
        if (!app.animating && app.anim_step == 0) {
            app.animating = true;
            app.anim_step = 1;
            app.anim_timer = 0;
        }

        // Avanzar animacion
        if (app.animating) {
            double now = ImGui::GetTime() * 1000.0; // ms
            if (app.anim_timer == 0) app.anim_timer = now;
            double elapsed = now - app.anim_timer;
            int speed = std::max(1, app.gui_config.animation_speed_ms_per_step);
            int target_step = static_cast<int>(elapsed / speed) + 1;
            app.anim_step = std::max(app.anim_step, target_step);
        }

        render_evolution_chart(app, center_w, 280);
        ImGui::Spacing();
    }

    // Metricas
    if (app.mode == 0 && app.has_result && !app.result.combats.empty()) {
        // Solo mostrar metricas completas cuando la animacion termine
        if (!app.animating) {
            render_deterministic_metrics(app.result.combats[0]);
        } else {
            ImGui::TextColored(colors::text_secondary, "Simulacion en curso...");
        }
    }

    if (app.mode == 1 && app.has_mc_result) {
        render_mc_metrics(app.mc_result);
    }

    if (!app.has_result && !app.has_mc_result && !app.running) {
        ImGui::Spacing();
        ImGui::TextColored(colors::text_secondary,
            "Pulsa EJECUTAR SIMULACION para ver los resultados.");
    }

    ImGui::Spacing(); ImGui::Spacing();

    // Boton volver
    if (ImGui::Button("<<  VOLVER A CONFIGURACION", ImVec2(240, 36)))
        app.current_step = WizardStep::SCENARIO;

    ImGui::EndGroup();
}
