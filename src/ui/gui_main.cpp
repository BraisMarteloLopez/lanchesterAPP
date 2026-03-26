// Lanchester-CIO — Interfaz Grafica (Dear ImGui + SDL2 + OpenGL3)
// Wizard de simulacion de combate terrestre

#ifdef _WIN32
#include <windows.h>
#endif

#include "../domain/model_factory.h"
#include "../domain/model_params.h"
#include "../domain/vehicle_catalog.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"

#include <SDL.h>
#include <SDL_opengl.h>

// GUI modules (must be after ImGui includes)
#include "gui_state.h"
#include "gui_nav_bar.h"
#include "gui_step_scenario.h"
#include "gui_step_side.h"
#include "gui_step_simulation.h"

#include <filesystem>
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
// Estilo profesional
// ---------------------------------------------------------------------------

static void apply_style() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding   = 0.0f;
    s.ChildRounding    = 6.0f;
    s.FrameRounding    = 4.0f;
    s.GrabRounding     = 4.0f;
    s.PopupRounding    = 4.0f;
    s.ScrollbarRounding= 4.0f;
    s.TabRounding      = 4.0f;
    s.FramePadding     = ImVec2(10, 6);
    s.ItemSpacing      = ImVec2(10, 8);
    s.WindowPadding    = ImVec2(16, 12);
    s.ChildBorderSize  = 1.0f;
    s.FrameBorderSize  = 0.0f;
    s.SeparatorTextBorderSize = 1.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]        = colors::bg_dark;
    c[ImGuiCol_ChildBg]         = {0, 0, 0, 0};
    c[ImGuiCol_PopupBg]         = colors::panel;
    c[ImGuiCol_Border]          = {0.25f, 0.25f, 0.28f, 0.6f};
    c[ImGuiCol_FrameBg]         = {0.18f, 0.18f, 0.20f, 1.0f};
    c[ImGuiCol_FrameBgHovered]  = {0.22f, 0.22f, 0.25f, 1.0f};
    c[ImGuiCol_FrameBgActive]   = {0.26f, 0.26f, 0.30f, 1.0f};
    c[ImGuiCol_TitleBg]         = colors::nav_bar;
    c[ImGuiCol_TitleBgActive]   = colors::nav_bar;
    c[ImGuiCol_MenuBarBg]       = colors::nav_bar;
    c[ImGuiCol_Header]          = {0.22f, 0.22f, 0.25f, 1.0f};
    c[ImGuiCol_HeaderHovered]   = {0.28f, 0.28f, 0.32f, 1.0f};
    c[ImGuiCol_HeaderActive]    = {0.32f, 0.32f, 0.36f, 1.0f};
    c[ImGuiCol_Button]          = {0.22f, 0.22f, 0.26f, 1.0f};
    c[ImGuiCol_ButtonHovered]   = {0.28f, 0.28f, 0.33f, 1.0f};
    c[ImGuiCol_ButtonActive]    = {0.34f, 0.34f, 0.40f, 1.0f};
    c[ImGuiCol_SliderGrab]      = colors::step_active;
    c[ImGuiCol_SliderGrabActive]= {0.35f, 0.62f, 0.90f, 1.0f};
    c[ImGuiCol_CheckMark]       = colors::step_active;
    c[ImGuiCol_Text]            = colors::text_primary;
    c[ImGuiCol_TextDisabled]    = colors::text_secondary;
    c[ImGuiCol_Separator]       = {0.25f, 0.25f, 0.28f, 0.6f};
    c[ImGuiCol_Tab]             = {0.18f, 0.18f, 0.20f, 1.0f};
    c[ImGuiCol_TabHovered]      = colors::step_active;
    c[ImGuiCol_TabSelected]     = {0.24f, 0.48f, 0.72f, 1.0f};
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
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    apply_style();

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 130");

    // --- Cargar datos ---
    AppState app;
    app.exe_dir = exe_directory(argv0);
    app.gui_config = GuiConfig::load(app.exe_dir + "/gui_config.json");

    auto model_params = std::make_shared<ModelParamsClass>(
        ModelParamsClass::load(app.exe_dir + "/model_params.json"));
    auto blue_catalog = std::make_shared<VehicleCatalogClass>(
        VehicleCatalogClass::load(app.exe_dir + "/vehicle_db.json"));
    auto red_catalog = std::make_shared<VehicleCatalogClass>(
        VehicleCatalogClass::load(app.exe_dir + "/vehicle_db_en.json"));

    app.model_names = ModelFactory::instance().availableModels();
    auto model = ModelFactory::instance().create(
        app.model_names.empty() ? "" : app.model_names[0], model_params);

    app.service = std::make_shared<SimulationService>(
        model, model_params, blue_catalog, red_catalog);

    app.blue_names = app.service->blueCatalog().names();
    app.red_names  = app.service->redCatalog().names();
    app.tactical_state_names = model_params->tacticalStateNames();

    if (app.blue_names.empty() || app.red_names.empty()) {
        std::snprintf(app.error_msg, sizeof(app.error_msg),
            "No se encontraron catalogos de vehiculos en '%s'", app.exe_dir.c_str());
    }

    // --- Bucle principal ---
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

        // Comprobar simulacion async
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

        // --- Frame ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Ventana principal fullscreen
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("##main", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        // Barra de navegacion
        render_nav_bar(app);

        ImGui::Spacing();

        // Error banner
        if (app.error_msg[0] != '\0') {
            ImGui::PushStyleColor(ImGuiCol_Text, colors::error_text);
            ImGui::TextWrapped("%s", app.error_msg);
            ImGui::PopStyleColor();
            ImGui::Separator();
            ImGui::Spacing();
        }

        // Contenido del paso actual (scrollable)
        ImGui::BeginChild("##content", ImVec2(-1, -1), ImGuiChildFlags_None);

        switch (app.current_step) {
            case WizardStep::SCENARIO:
                render_step_scenario(app);
                break;
            case WizardStep::BLUE_SIDE:
                render_step_side(app, true);
                break;
            case WizardStep::RED_SIDE:
                render_step_side(app, false);
                break;
            case WizardStep::SIMULATION:
                render_step_simulation(app);
                break;
        }

        ImGui::EndChild();
        ImGui::End();

        // Render
        ImGui::Render();
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(colors::bg_dark.x, colors::bg_dark.y, colors::bg_dark.z, 1.0f);
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
