// Modelo Lanchester-CIO — CLI
// CIO / ET — Herramienta de investigacion interna

#include "lanchester_io.h"

// Definicion de la instancia global de parametros del modelo
ModelParams g_model_params;

// ---------------------------------------------------------------------------
// main — CLI
// ---------------------------------------------------------------------------

void print_usage(const char* prog) {
    std::fprintf(stderr,
        "Uso:\n"
        "  %s <escenario.json> [--output <file>] [--aggregation pre|post]\n"
        "  %s --batch <directorio/> [--output <resultados.csv>]\n"
        "  %s --scenario <base.json> --sweep <path> <start> <end> <step> [--output <file>]\n"
        "  %s <escenario.json> --montecarlo <N> [--seed <S>] [--output <file>]\n"
        "  %s <escenario.json> --sensitivity [--output <file>]\n"
        "\n"
        "Opciones:\n"
        "  --output <file>         Escribir resultado a fichero (por defecto: stdout)\n"
        "  --aggregation pre|post  Modo de agregacion (por defecto: pre)\n"
        "  --batch <dir>           Procesar todos los JSON de un directorio (salida CSV)\n"
        "  --scenario <file>       Escenario base para --sweep\n"
        "  --sweep <path> <s> <e> <step>  Barrer parametro (salida CSV)\n"
        "  --montecarlo <N>        Ejecutar N replicas estocasticas (Poisson)\n"
        "  --seed <S>              Semilla para reproducibilidad (default: 42)\n"
        "  --sensitivity           Analisis de sensibilidad de parametros\n",
        prog, prog, prog, prog, prog);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string scenario_path;
    std::string output_path;
    std::string batch_dir;
    std::string sweep_scenario;
    std::string sweep_path;
    double sweep_start = 0, sweep_end = 0, sweep_step = 1;
    bool do_sweep = false;
    AggregationMode agg_mode = AggregationMode::PRE;
    int mc_replicas = 0;
    uint64_t mc_seed = 42;
    bool do_sensitivity = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--output" && i + 1 < argc) {
            output_path = argv[++i];
        } else if (arg == "--aggregation" && i + 1 < argc) {
            std::string mode = argv[++i];
            if (mode == "post") agg_mode = AggregationMode::POST;
            else                agg_mode = AggregationMode::PRE;
        } else if (arg == "--batch" && i + 1 < argc) {
            batch_dir = argv[++i];
        } else if (arg == "--scenario" && i + 1 < argc) {
            sweep_scenario = argv[++i];
        } else if (arg == "--sweep" && i + 4 < argc) {
            do_sweep    = true;
            sweep_path  = argv[++i];
            sweep_start = std::stod(argv[++i]);
            sweep_end   = std::stod(argv[++i]);
            sweep_step  = std::stod(argv[++i]);
        } else if (arg == "--montecarlo" && i + 1 < argc) {
            mc_replicas = std::stoi(argv[++i]);
        } else if (arg == "--seed" && i + 1 < argc) {
            mc_seed = static_cast<uint64_t>(std::stoull(argv[++i]));
        } else if (arg == "--sensitivity") {
            do_sensitivity = true;
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg[0] != '-') {
            scenario_path = arg;
        } else {
            std::fprintf(stderr, "Opcion desconocida: %s\n", arg.c_str());
            print_usage(argv[0]);
            return 1;
        }
    }

    // Cargar parametros del modelo y catalogos
    std::string exe_dir = exe_directory(argv[0]);
    g_model_params = load_model_params(exe_dir + "/model_params.json");
    VehicleCatalog blue_cat = load_catalog(exe_dir + "/vehicle_db.json");
    VehicleCatalog red_cat  = load_catalog(exe_dir + "/vehicle_db_en.json");

    if (blue_cat.empty() && red_cat.empty())
        std::fprintf(stderr, "Aviso: no se encontraron catalogos de vehiculos en '%s'.\n",
                     exe_dir.c_str());

    // Modo batch
    if (!batch_dir.empty()) {
        run_batch(batch_dir, output_path, blue_cat, red_cat, agg_mode);
        return 0;
    }

    // Modo sweep
    if (do_sweep) {
        std::string base = sweep_scenario.empty() ? scenario_path : sweep_scenario;
        if (base.empty()) {
            std::fprintf(stderr, "Error: --sweep requiere --scenario <file> o un escenario posicional.\n");
            return 1;
        }
        run_sweep(base, sweep_path, sweep_start, sweep_end, sweep_step,
                  output_path, blue_cat, red_cat, agg_mode);
        return 0;
    }

    // A partir de aqui se necesita un escenario
    if (scenario_path.empty()) {
        std::fprintf(stderr, "Error: se requiere un fichero de escenario.\n");
        print_usage(argv[0]);
        return 1;
    }

    std::ifstream ifs(scenario_path);
    if (!ifs.is_open()) {
        std::fprintf(stderr, "Error: no se pudo abrir '%s'.\n", scenario_path.c_str());
        return 1;
    }

    json scenario;
    try {
        scenario = json::parse(ifs);
    } catch (const json::parse_error& e) {
        std::fprintf(stderr, "Error JSON en '%s': %s\n",
                     scenario_path.c_str(), e.what());
        return 1;
    }

    // Modo sensibilidad
    if (do_sensitivity) {
        run_sensitivity(scenario, output_path, blue_cat, red_cat, agg_mode);
        return 0;
    }

    // Modo Monte Carlo
    if (mc_replicas > 0) {
        MonteCarloScenarioOutput mc_result =
            run_scenario_montecarlo(scenario, blue_cat, red_cat,
                                    agg_mode, mc_replicas, mc_seed);
        json output_json = mc_scenario_to_json(mc_result);
        std::string output_str = output_json.dump(2);

        if (output_path.empty()) {
            std::cout << output_str << std::endl;
        } else {
            std::ofstream ofs(output_path);
            if (!ofs.is_open()) {
                std::fprintf(stderr, "Error: no se pudo escribir '%s'.\n",
                             output_path.c_str());
                return 1;
            }
            ofs << output_str << std::endl;
            std::fprintf(stderr, "Monte Carlo (%d replicas) escrito en '%s'.\n",
                         mc_replicas, output_path.c_str());
        }
        return 0;
    }

    // Modo escenario unico (determinista)
    ScenarioOutput result = run_scenario(scenario, blue_cat, red_cat, agg_mode);

    json output_json = scenario_to_json(result);
    std::string output_str = output_json.dump(2);

    if (output_path.empty()) {
        std::cout << output_str << std::endl;
    } else {
        std::ofstream ofs(output_path);
        if (!ofs.is_open()) {
            std::fprintf(stderr, "Error: no se pudo escribir '%s'.\n",
                         output_path.c_str());
            return 1;
        }
        ofs << output_str << std::endl;
        std::fprintf(stderr, "Resultado escrito en '%s'.\n", output_path.c_str());
    }

    return 0;
}
