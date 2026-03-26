// montecarlo_runner.cpp — Implementacion de MonteCarloRunner
#include "montecarlo_runner.h"
#include "combat_utils.h"

MonteCarloResult MonteCarloRunner::run(const IStochasticModel& model,
                                        const CombatInput& input,
                                        int n_replicas,
                                        std::mt19937& rng) {
    MonteCarloResult mc;
    mc.combat_id  = input.combat_id;
    mc.n_replicas = n_replicas;

    mc.deterministic = model.simulate(input);

    std::vector<double> blue_surv(n_replicas);
    std::vector<double> red_surv(n_replicas);
    std::vector<double> durations(n_replicas);

    for (int i = 0; i < n_replicas; ++i) {
        CombatResult r = model.simulateStochastic(input, rng);
        blue_surv[i] = r.blue_survivors;
        red_surv[i]  = r.red_survivors;
        durations[i] = r.duration_contact_minutes;

        switch (r.outcome) {
            case Outcome::BLUE_WINS:     ++mc.count_blue_wins; break;
            case Outcome::RED_WINS:      ++mc.count_red_wins; break;
            case Outcome::DRAW:          ++mc.count_draw; break;
            case Outcome::INDETERMINATE: ++mc.count_indeterminate; break;
        }
    }

    mc.blue_survivors = lanchester::computeStats(blue_surv);
    mc.red_survivors  = lanchester::computeStats(red_surv);
    mc.duration       = lanchester::computeStats(durations);
    return mc;
}
