// Lanchester-CIO — Interfaz Grafica (Dear ImGui + SDL2 + OpenGL3)
// Cross-compilable desde Linux a Windows (.exe) con MinGW-w64

#ifdef _WIN32
#include <windows.h>
#endif

#include "../application/simulation_service.h"
#include "../domain/model_factory.h"
#include "../domain/model_params.h"
#include "../domain/vehicle_catalog.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"

#include <SDL.h>
#include <SDL_opengl.h>

#include <filesystem>
#include <future>
#include <chrono>
#include <cstring>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Utilidad: directorio del ejecutable
// ---------------------------------------------------------------------------

static std::string exe_directory([[maybe_unused]] const char* argv0) {
#ifdef _WIN32
    char buf[260];
    unsigned long len = GetModuleFileNameA(nullptr, buf, 260);
    if (len > 0 && len < 260) {
        std::string path(buf, len);
        auto pos = path.find_last_of("\\/");
        if (pos != std::string::npos) return path.substr(0, pos);
    }
#else
    try {
        auto real = fs::read_symlink("/proc/self/exe");
        return real.parent_path().string();
    } catch (...) {}
#endif
    std::string path(argv0);
    auto pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return ".";
    return path.substr(0, pos);
}

// ---------------------------------------------------------------------------
// Estado de la aplicacion
// ---------------------------------------------------------------------------

struct GuiSideConfig {
    int tactical_state_idx  = 0;
    int mobility_idx        = 1; // ALTA
    float aft_casualties_pct = 0.0f;
    float engagement_fraction = 1.0f;
    float rate_factor       = 1.0f;
    float count_factor      = 1.0f;

    // Composicion: hasta 4 tipos de vehiculo
    static constexpr int MAX_TYPES = 4;
    int vehicle_idx[MAX_TYPES]  = {0, 0, 0, 0};
    int vehicle_count[MAX_TYPES] = {10, 0, 0, 0};
    int num_types = 1;
};

struct AppState {
    // Nombres de vehiculos (para UI)
    std::vector<std::string> blue_names, red_names;

    // Configuracion del escenario
    int terrain_idx = 1; // MEDIO
    float distance_m = 2000.0f;
    float t_max_minutes = 30.0f;
    int aggregation_idx = 0; // PRE

    GuiSideConfig blue, red;

    // Monte Carlo
    int mc_replicas = 1000;
    int mc_seed = 42;

    // Modo: 0=Single, 1=Monte Carlo
    int mode = 0;

    // Resultados
    bool has_result = false;
    ScenarioOutput result;
    bool has_mc_result = false;
    MonteCarloScenarioOutput mc_result;

    // Estado de simulacion
    bool running = false;
    std::future<ScenarioOutput> future_result;
    std::future<MonteCarloScenarioOutput> future_mc;

    // Error
    char error_msg[256] = "";

    // Datos de la aplicacion
    std::string exe_dir;

    // Servicio de simulacion (OOP)
    std::shared_ptr<SimulationService> service;
};

// ---------------------------------------------------------------------------
// Constantes de la UI
// ---------------------------------------------------------------------------

static const char* TERRAIN_NAMES[]  = {"FACIL", "MEDIO", "DIFICIL"};
static const char* TACTICAL_STATES[] = {
    "Ataque a posicion defensiva",
    "Busqueda del contacto",
    "En posicion de tiro",
    "Defensiva condiciones minimas",
    "Defensiva organizacion ligera",
    "Defensiva organizacion media",
    "Retardo",
    "Retrocede"
};
static const char* MOBILITY_NAMES[] = {"MUY_ALTA", "ALTA", "MEDIA", "BAJA"};
static const char* AGG_NAMES[]      = {"PRE (por defecto)", "POST (mas realista)"};

// ---------------------------------------------------------------------------
// Construir ScenarioConfig tipado desde la configuracion de la UI
// ---------------------------------------------------------------------------

static ScenarioConfig buildScenarioConfig(const AppState& app) {
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
        side.tactical_state = TACTICAL_STATES[gui.tactical_state_idx];
        side.mobility = static_cast<Mobility>(gui.mobility_idx);
        side.aft_pct = static_cast<double>(gui.aft_casualties_pct);
        side.engagement_fraction = static_cast<double>(gui.engagement_fraction);
        side.rate_factor = static_cast<double>(gui.rate_factor);
        side.count_factor = static_cast<double>(gui.count_factor);

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
// Panel de configuracion de un bando
// ---------------------------------------------------------------------------

static void render_side_config(const char* label, GuiSideConfig& side,
                                const std::vector<std::string>& vehicle_names,
                                const IVehicleCatalog& catalog) {
    ImGui::PushID(label);

    // Estado tactico
    ImGui::Text("Estado tactico:");
    ImGui::SetNextItemWidth(-1);
    ImGui::Combo("##tac", &side.tactical_state_idx, TACTICAL_STATES, IM_ARRAYSIZE(TACTICAL_STATES));

    // Movilidad
    ImGui::Text("Movilidad:");
    ImGui::SetNextItemWidth(-1);
    ImGui::Combo("##mob", &side.mobility_idx, MOBILITY_NAMES, IM_ARRAYSIZE(MOBILITY_NAMES));

    ImGui::Separator();

    // Composicion
    ImGui::Text("Composicion:");
    for (int i = 0; i < side.num_types; ++i) {
        ImGui::PushID(i);

        // Selector de vehiculo
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.65f);
        if (ImGui::BeginCombo("##veh", vehicle_names.empty() ? "---" :
                              vehicle_names[side.vehicle_idx[i]].c_str())) {
            for (int v = 0; v < static_cast<int>(vehicle_names.size()); ++v) {
                bool selected = (side.vehicle_idx[i] == v);
                if (ImGui::Selectable(vehicle_names[v].c_str(), selected))
                    side.vehicle_idx[i] = v;
                // Tooltip con info del vehiculo
                if (ImGui::IsItemHovered() && catalog.contains(vehicle_names[v])) {
                    const auto& vp = catalog.find(vehicle_names[v]);
                    ImGui::SetTooltip("D=%.0f P=%.0f U=%.2f c=%.1f\nA_max=%.0f CC=%s",
                        vp.D, vp.P, vp.U, vp.c, vp.A_max, vp.CC ? "Si" : "No");
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 25);
        ImGui::InputInt("##cnt", &side.vehicle_count[i], 1, 5);
        if (side.vehicle_count[i] < 0) side.vehicle_count[i] = 0;

        // Boton eliminar tipo
        if (side.num_types > 1) {
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) {
                for (int j = i; j < side.num_types - 1; ++j) {
                    side.vehicle_idx[j] = side.vehicle_idx[j + 1];
                    side.vehicle_count[j] = side.vehicle_count[j + 1];
                }
                side.num_types--;
                i--;
            }
        }

        ImGui::PopID();
    }

    if (side.num_types < GuiSideConfig::MAX_TYPES) {
        if (ImGui::SmallButton("+ Anadir tipo")) {
            side.vehicle_idx[side.num_types] = 0;
            side.vehicle_count[side.num_types] = 5;
            side.num_types++;
        }
    }

    ImGui::Separator();

    // Parametros avanzados
    if (ImGui::TreeNode("Parametros avanzados")) {
        ImGui::SliderFloat("Bajas AFT (%)", &side.aft_casualties_pct, 0.0f, 1.0f, "%.0f%%");
        ImGui::SliderFloat("Fraccion empenamiento", &side.engagement_fraction, 0.1f, 1.0f, "%.2f");
        ImGui::SliderFloat("Factor cadencia", &side.rate_factor, 0.1f, 3.0f, "%.2f");
        ImGui::SliderFloat("Factor efectivos", &side.count_factor, 0.1f, 3.0f, "%.2f");
        ImGui::TreePop();
    }

    ImGui::PopID();
}

// ---------------------------------------------------------------------------
// Panel de resultados
// ---------------------------------------------------------------------------

static void render_results(const AppState& app) {
    if (!app.has_result && !app.has_mc_result) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
            "Configura el escenario y pulsa 'Ejecutar simulacion'");
        return;
    }

    if (app.mode == 0 && app.has_result) {
        // Resultado determinista
        for (const auto& r : app.result.combats) {
            // Outcome con color
            ImVec4 color;
            const char* outcome = outcome_str(r.outcome);
            if (r.outcome == Outcome::BLUE_WINS)
                color = ImVec4(0.2f, 0.5f, 1.0f, 1.0f);
            else if (r.outcome == Outcome::RED_WINS)
                color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
            else if (r.outcome == Outcome::DRAW)
                color = ImVec4(1.0f, 0.8f, 0.0f, 1.0f);
            else
                color = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);

            ImGui::TextColored(color, "RESULTADO: %s", outcome);
            ImGui::Separator();

            if (ImGui::BeginTable("results", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Metrica", ImGuiTableColumnFlags_WidthFixed, 200);
                ImGui::TableSetupColumn("Azul", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Rojo", ImGuiTableColumnFlags_WidthStretch);
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
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.2f", r.duration_contact_minutes);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Ventaja estatica");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.4f", r.static_advantage);

                ImGui::EndTable();
            }

            // Grafica de barras supervivientes
            ImGui::Spacing();
            if (ImPlot::BeginPlot("Fuerzas", ImVec2(-1, 200))) {
                ImPlot::SetupAxes("", "Efectivos");
                double positions[] = {0, 1};
                double blue_vals[] = {r.blue_initial, r.blue_survivors};
                double red_vals[] = {r.red_initial, r.red_survivors};
                const char* labels[] = {"Iniciales", "Supervivientes"};
                ImPlot::SetupAxisTicks(ImAxis_X1, positions, 2, labels);

                ImPlot::SetNextFillStyle(ImVec4(0.2f, 0.5f, 1.0f, 0.7f));
                ImPlot::PlotBars("Azul", positions, blue_vals, 2, 0.3, ImPlotBarsFlags_None, 0, sizeof(double));
                double pos_shifted[] = {0.35, 1.35};
                ImPlot::SetNextFillStyle(ImVec4(1.0f, 0.3f, 0.3f, 0.7f));
                ImPlot::PlotBars("Rojo", pos_shifted, red_vals, 2, 0.3, ImPlotBarsFlags_None, 0, sizeof(double));

                ImPlot::EndPlot();
            }
        }
    }

    if (app.mode == 1 && app.has_mc_result) {
        ImGui::Text("Monte Carlo: %d replicas (seed=%llu)",
            app.mc_result.n_replicas, (unsigned long long)app.mc_result.seed);
        ImGui::Separator();

        for (const auto& mc : app.mc_result.combats) {
            int n = mc.n_replicas;
            double bw = n > 0 ? static_cast<double>(mc.count_blue_wins) / n : 0;
            double rw = n > 0 ? static_cast<double>(mc.count_red_wins) / n : 0;
            double dw = n > 0 ? static_cast<double>(mc.count_draw) / n : 0;

            // Distribucion de outcomes
            ImGui::Text("P(Azul gana) = %.1f%%  |  P(Rojo gana) = %.1f%%  |  P(Empate) = %.1f%%",
                bw * 100, rw * 100, dw * 100);

            if (ImPlot::BeginPlot("Distribucion de resultados", ImVec2(-1, 150))) {
                ImPlot::SetupAxes("", "Probabilidad");
                double positions[] = {0, 1, 2};
                double probs[] = {bw, rw, dw};
                const char* labels[] = {"Azul gana", "Rojo gana", "Empate"};
                ImPlot::SetupAxisTicks(ImAxis_X1, positions, 3, labels);
                ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 1.05);

                ImVec4 colors[] = {
                    {0.2f, 0.5f, 1.0f, 0.8f},
                    {1.0f, 0.3f, 0.3f, 0.8f},
                    {1.0f, 0.8f, 0.0f, 0.8f}
                };
                for (int i = 0; i < 3; ++i) {
                    ImPlot::SetNextFillStyle(colors[i]);
                    ImPlot::PlotBars(labels[i], &positions[i], &probs[i], 1, 0.6);
                }
                ImPlot::EndPlot();
            }

            ImGui::Separator();

            // Tabla de estadisticas
            if (ImGui::BeginTable("mc_stats", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
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
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Determin.");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%.2f", mc.deterministic.blue_survivors);
                ImGui::TableSetColumnIndex(2);
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%.2f", mc.deterministic.red_survivors);
                ImGui::TableSetColumnIndex(3);
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%.2f", mc.deterministic.duration_contact_minutes);

                ImGui::EndTable();
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    const char* argv0 = "";
#else
int main(int /*argc*/, char* argv[]) {
    const char* argv0 = argv[0];
#endif

    // Inicializar SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::fprintf(stderr, "Error SDL: %s\n", SDL_GetError());
        return 1;
    }

    // OpenGL 3.0 context
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window* window = SDL_CreateWindow(
        "Lanchester-CIO — Simulador de Combate",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 800,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        std::fprintf(stderr, "Error creando ventana: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // vsync

    // Inicializar ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    // Estilo personalizado
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.FramePadding = ImVec2(8, 4);
    style.ItemSpacing = ImVec2(8, 6);

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 130");

    // Cargar datos del modelo (OOP)
    AppState app;
    app.exe_dir = exe_directory(argv0);

    auto model_params = std::make_shared<ModelParamsClass>(
        ModelParamsClass::load(app.exe_dir + "/model_params.json"));
    auto blue_catalog = std::make_shared<VehicleCatalogClass>(
        VehicleCatalogClass::load(app.exe_dir + "/vehicle_db.json"));
    auto red_catalog = std::make_shared<VehicleCatalogClass>(
        VehicleCatalogClass::load(app.exe_dir + "/vehicle_db_en.json"));
    auto model = ModelFactory::instance().create(
        ModelFactory::instance().defaultModel(), model_params);

    app.service = std::make_shared<SimulationService>(
        model, model_params, blue_catalog, red_catalog);

    app.blue_names = app.service->blueCatalog().names();
    app.red_names  = app.service->redCatalog().names();

    if (app.blue_names.empty() || app.red_names.empty()) {
        std::snprintf(app.error_msg, sizeof(app.error_msg),
            "No se encontraron catalogos de vehiculos en '%s'", app.exe_dir.c_str());
    }

    // Bucle principal
    bool done = false;
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) done = true;
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        // Comprobar si la simulacion en background termino
        if (app.running) {
            if (app.mode == 0 && app.future_result.valid() &&
                app.future_result.wait_for(std::chrono::milliseconds(0)) ==
                std::future_status::ready) {
                try {
                    app.result = app.future_result.get();
                    app.has_result = true;
                    app.error_msg[0] = '\0';
                } catch (const std::exception& e) {
                    std::snprintf(app.error_msg, sizeof(app.error_msg), "Error: %s", e.what());
                }
                app.running = false;
            }
            if (app.mode == 1 && app.future_mc.valid() &&
                app.future_mc.wait_for(std::chrono::milliseconds(0)) ==
                std::future_status::ready) {
                try {
                    app.mc_result = app.future_mc.get();
                    app.has_mc_result = true;
                    app.error_msg[0] = '\0';
                } catch (const std::exception& e) {
                    std::snprintf(app.error_msg, sizeof(app.error_msg), "Error: %s", e.what());
                }
                app.running = false;
            }
        }

        // Nuevo frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Ventana principal (ocupa toda la pantalla)
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("##main", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_MenuBar);

        // Barra de menu
        if (ImGui::BeginMenuBar()) {
            ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f),
                "LANCHESTER-CIO");
            ImGui::SameLine(0, 20);
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                "Simulador de Combate Terrestre");
            ImGui::EndMenuBar();
        }

        // Error banner
        if (app.error_msg[0] != '\0') {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            ImGui::TextWrapped("%s", app.error_msg);
            ImGui::PopStyleColor();
            ImGui::Separator();
        }

        // Layout: 2 columnas
        float panel_width = ImGui::GetContentRegionAvail().x * 0.42f;

        // === COLUMNA IZQUIERDA: Configuracion ===
        ImGui::BeginChild("config_panel", ImVec2(panel_width, -1), ImGuiChildFlags_Border);

        // Modo
        ImGui::Text("MODO DE SIMULACION");
        ImGui::RadioButton("Determinista", &app.mode, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Monte Carlo", &app.mode, 1);

        if (app.mode == 1) {
            ImGui::SetNextItemWidth(120);
            ImGui::InputInt("Replicas", &app.mc_replicas, 100, 1000);
            if (app.mc_replicas < 10) app.mc_replicas = 10;
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            ImGui::InputInt("Seed", &app.mc_seed);
        }

        ImGui::Separator();

        // Parametros globales
        ImGui::Text("PARAMETROS DEL ESCENARIO");
        ImGui::SetNextItemWidth(-1);
        ImGui::Combo("Terreno", &app.terrain_idx, TERRAIN_NAMES, IM_ARRAYSIZE(TERRAIN_NAMES));
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("Distancia (m)", &app.distance_m, 100.0f, 9000.0f, "%.0f m");
        ImGui::SetNextItemWidth(-1);
        ImGui::Combo("Agregacion", &app.aggregation_idx, AGG_NAMES, IM_ARRAYSIZE(AGG_NAMES));
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("T max (min)", &app.t_max_minutes, 5.0f, 120.0f, "%.0f min");

        ImGui::Separator();

        // Bando azul
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.3f, 0.6f, 0.7f));
        if (ImGui::CollapsingHeader("BANDO AZUL (Propio)", ImGuiTreeNodeFlags_DefaultOpen)) {
            render_side_config("blue", app.blue, app.blue_names, app.service->blueCatalog());
        }
        ImGui::PopStyleColor();

        ImGui::Spacing();

        // Bando rojo
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.6f, 0.1f, 0.1f, 0.7f));
        if (ImGui::CollapsingHeader("BANDO ROJO (Enemigo)", ImGuiTreeNodeFlags_DefaultOpen)) {
            render_side_config("red", app.red, app.red_names, app.service->redCatalog());
        }
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();

        // Boton de ejecucion
        if (app.running) {
            ImGui::BeginDisabled();
            ImGui::Button("Simulando...", ImVec2(-1, 40));
            ImGui::EndDisabled();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
            if (ImGui::Button("EJECUTAR SIMULACION", ImVec2(-1, 40))) {
                app.error_msg[0] = '\0';
                app.has_result = false;
                app.has_mc_result = false;
                try {
                    auto config = buildScenarioConfig(app);
                    if (app.mode == 0) {
                        app.running = true;
                        app.future_result = app.service->runScenarioAsync(
                            std::move(config));
                    } else {
                        app.running = true;
                        app.future_mc = app.service->runMonteCarloAsync(
                            std::move(config), app.mc_replicas,
                            static_cast<uint64_t>(app.mc_seed));
                    }
                } catch (const std::exception& e) {
                    std::snprintf(app.error_msg, sizeof(app.error_msg),
                        "Error: %s", e.what());
                }
            }
            ImGui::PopStyleColor(2);
        }

        ImGui::EndChild();

        ImGui::SameLine();

        // === COLUMNA DERECHA: Resultados ===
        ImGui::BeginChild("results_panel", ImVec2(0, -1), ImGuiChildFlags_Border);
        ImGui::Text("RESULTADOS");
        ImGui::Separator();
        render_results(app);
        ImGui::EndChild();

        ImGui::End();

        // Render
        ImGui::Render();
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
