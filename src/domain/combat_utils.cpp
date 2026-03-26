// combat_utils.cpp — Implementacion de utilidades de combate
#include "combat_utils.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace lanchester {

AggregatedParams aggregate(const std::vector<CompositionEntry>& composition) {
    AggregatedParams agg{};
    int n_total = 0, n_cc = 0;
    double sum_D = 0, sum_P = 0, sum_U = 0, sum_c = 0, sum_Amax = 0, sum_f = 0;
    double sum_Pcc = 0, sum_Dcc = 0, sum_ccc = 0, sum_Acc = 0, sum_M = 0, sum_fcc = 0;

    for (const auto& entry : composition) {
        int n = entry.count;
        const auto& v = entry.vehicle;
        n_total += n;
        sum_D    += n * v.D;
        sum_P    += n * v.P;
        sum_U    += n * v.U;
        sum_c    += n * v.c;
        sum_Amax += n * v.A_max;
        sum_f    += n * v.f;
        if (v.CC) {
            n_cc    += n;
            sum_Pcc += n * v.P_cc;
            sum_Dcc += n * v.D_cc;
            sum_ccc += n * v.c_cc;
            sum_Acc += n * v.A_cc;
            sum_M   += n * v.M;
            sum_fcc += n * v.f_cc;
        }
    }

    agg.n_total = n_total;
    agg.n_cc    = n_cc;
    if (n_total > 0) {
        agg.D     = sum_D    / n_total;
        agg.P     = sum_P    / n_total;
        agg.U     = sum_U    / n_total;
        agg.c     = sum_c    / n_total;
        agg.A_max = sum_Amax / n_total;
        agg.f     = sum_f    / n_total;
    }
    if (n_cc > 0) {
        agg.has_cc = true;
        agg.P_cc = sum_Pcc / n_cc;  agg.D_cc = sum_Dcc / n_cc;
        agg.c_cc = sum_ccc / n_cc;  agg.A_cc = sum_Acc / n_cc;
        agg.M    = sum_M   / n_cc;  agg.f_cc = sum_fcc / n_cc;
    }
    return agg;
}

double initialForces(int n_total, double aft_pct, double eng_frac, double cnt_fac) {
    double n = static_cast<double>(n_total);
    return (n - n * aft_pct) * eng_frac * cnt_fac;
}

int totalCount(const std::vector<CompositionEntry>& comp) {
    int n = 0;
    for (const auto& e : comp) n += e.count;
    return n;
}

void distributeCasualtiesByVulnerability(
    std::vector<CompositionEntry>& comp, double total_casualties)
{
    if (comp.empty() || total_casualties <= 0.0) return;
    int n_total = totalCount(comp);
    if (n_total <= 0) return;

    std::vector<double> vulnerability(comp.size());
    double vuln_sum = 0;
    for (size_t i = 0; i < comp.size(); ++i) {
        vulnerability[i] = comp[i].count / (comp[i].vehicle.D_cc + 1.0);
        vuln_sum += vulnerability[i];
    }
    if (vuln_sum <= 0.0) {
        double frac = std::max(0.0, 1.0 - total_casualties / n_total);
        for (auto& e : comp)
            e.count = std::max(0, static_cast<int>(std::round(e.count * frac)));
        return;
    }

    double remaining_casualties = total_casualties;
    for (size_t i = 0; i < comp.size(); ++i) {
        double share = total_casualties * (vulnerability[i] / vuln_sum);
        int losses = std::min(comp[i].count,
                              static_cast<int>(std::round(share)));
        comp[i].count -= losses;
        remaining_casualties -= losses;
    }
    while (remaining_casualties >= 1.0) {
        for (size_t i = 0; i < comp.size() && remaining_casualties >= 1.0; ++i) {
            if (comp[i].count > 0) {
                comp[i].count--;
                remaining_casualties -= 1.0;
            }
        }
    }
}

PercentileStats computeStats(std::vector<double>& data) {
    PercentileStats ps;
    if (data.empty()) return ps;
    std::sort(data.begin(), data.end());
    int n = static_cast<int>(data.size());

    double sum = std::accumulate(data.begin(), data.end(), 0.0);
    ps.mean = sum / n;

    double sq_sum = 0;
    for (double d : data) sq_sum += (d - ps.mean) * (d - ps.mean);
    ps.std = (n > 1) ? std::sqrt(sq_sum / (n - 1)) : 0.0;

    auto pct = [&](double p) -> double {
        double idx = p * (n - 1);
        int lo = static_cast<int>(std::floor(idx));
        int hi = std::min(lo + 1, n - 1);
        double frac = idx - lo;
        return data[lo] * (1.0 - frac) + data[hi] * frac;
    };

    ps.p05    = pct(0.05);
    ps.p25    = pct(0.25);
    ps.median = pct(0.50);
    ps.p75    = pct(0.75);
    ps.p95    = pct(0.95);
    return ps;
}

} // namespace lanchester
