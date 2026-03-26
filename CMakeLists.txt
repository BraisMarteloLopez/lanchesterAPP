cmake_minimum_required(VERSION 3.20)
project(LanchesterCIO VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(LANCHESTER_BUILD_GUI   "Build the GUI application (requires SDL2, ImGui)" OFF)
option(LANCHESTER_BUILD_TESTS "Build the test suite (fetches Catch2)"            ON)

# ---------------------------------------------------------------------------
# Core library (static — domain + application layer)
# ---------------------------------------------------------------------------
add_library(lanchester_core STATIC
    src/domain/model_params.cpp
    src/domain/vehicle_catalog.cpp
    src/domain/combat_utils.cpp
    src/domain/square_law_model.cpp
    src/domain/montecarlo_runner.cpp
    src/domain/model_factory.cpp
    src/application/scenario_config.cpp
    src/application/simulation_service.cpp
)
target_include_directories(lanchester_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include          # nlohmann/json.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/domain
    ${CMAKE_CURRENT_SOURCE_DIR}/src/application
)

# ---------------------------------------------------------------------------
# GUI application (optional — only with SDL2/ImGui/implot)
# ---------------------------------------------------------------------------
if(LANCHESTER_BUILD_GUI)
    # Vendor directories (downloaded by setup_gui_deps.sh)
    set(VENDOR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor)
    set(IMGUI_DIR  ${VENDOR_DIR}/imgui-1.91.7)
    set(IMPLOT_DIR ${VENDOR_DIR}/implot-0.16)

    if(MINGW OR WIN32)
        set(SDL2_DIR ${VENDOR_DIR}/SDL2-2.30.10/x86_64-w64-mingw32)
        set(SDL2_INCLUDE_DIRS ${SDL2_DIR}/include/SDL2)
        set(SDL2_LIBRARIES    ${SDL2_DIR}/lib/libSDL2main.a
                              ${SDL2_DIR}/lib/libSDL2.dll.a)
    else()
        find_package(SDL2 REQUIRED)
    endif()

    # ImGui sources
    set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    )

    # implot sources
    set(IMPLOT_SOURCES
        ${IMPLOT_DIR}/implot.cpp
        ${IMPLOT_DIR}/implot_items.cpp
    )

    add_executable(lanchester_gui
        src/ui/gui_main.cpp
        src/ui/gui_config.cpp
        ${IMGUI_SOURCES}
        ${IMPLOT_SOURCES}
    )

    target_include_directories(lanchester_gui PRIVATE
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
        ${IMPLOT_DIR}
        ${SDL2_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    )

    target_link_libraries(lanchester_gui PRIVATE
        lanchester_core
        ${SDL2_LIBRARIES}
    )

    if(MINGW)
        target_link_libraries(lanchester_gui PRIVATE
            mingw32 opengl32
            -static-libgcc -static-libstdc++ -static -lpthread
        )
        target_link_options(lanchester_gui PRIVATE -mwindows)

        # Copy SDL2.dll and data files to output directory
        add_custom_command(TARGET lanchester_gui POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${SDL2_DIR}/bin/SDL2.dll $<TARGET_FILE_DIR:lanchester_gui>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_CURRENT_SOURCE_DIR}/data/model_params.json $<TARGET_FILE_DIR:lanchester_gui>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_CURRENT_SOURCE_DIR}/data/vehicle_db.json $<TARGET_FILE_DIR:lanchester_gui>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_CURRENT_SOURCE_DIR}/data/vehicle_db_en.json $<TARGET_FILE_DIR:lanchester_gui>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_CURRENT_SOURCE_DIR}/data/gui_config.json $<TARGET_FILE_DIR:lanchester_gui>
        )
    elseif(UNIX)
        find_package(OpenGL REQUIRED)
        target_link_libraries(lanchester_gui PRIVATE OpenGL::GL)
    endif()

    set_target_properties(lanchester_gui PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/release
    )
endif()

# ---------------------------------------------------------------------------
# Tests (Catch2 v3 via FetchContent)
# ---------------------------------------------------------------------------
if(LANCHESTER_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.5.2
    )
    FetchContent_MakeAvailable(Catch2)

    add_executable(lanchester_tests
        src/tests/test_main.cpp
        src/tests/test_model_params.cpp
        src/tests/test_vehicle_catalog.cpp
        src/tests/test_square_law.cpp
        src/tests/test_simulation_service.cpp
    )

    target_link_libraries(lanchester_tests PRIVATE
        lanchester_core
        Catch2::Catch2WithMain
    )

    # Make test data path available to tests
    target_compile_definitions(lanchester_tests PRIVATE
        TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/src/tests/data"
    )

    include(CTest)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    include(Catch)
    catch_discover_tests(lanchester_tests)
endif()
