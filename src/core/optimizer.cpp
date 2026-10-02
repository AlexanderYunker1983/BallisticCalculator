// SPDX-License-Identifier: GPL-3.0-or-later
#include "optimizer.h"
#include "integration.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace ballistic { namespace detail {
namespace {
struct Residual {
    double h, v;
    double norm(const Options &o) const {
        return std::hypot(h / std::max(1e-12, o.altitudeTolerance), v / std::max(1e-12, o.velocityTolerance));
    }
};
}
Parameters optimize(const Parameters &input, const Atmosphere &air, RunContext &ctx,
                    const Progress &progress, Diagnostics &diagnostics, Parameters &best) {
    const auto &o = ctx.o;
    Parameters p = input;
    p.engineCutoffTime = input.duration();
    const auto fuelTimes = input.separationTimes();
    const double minimumCutoff = std::max(fuelTimes[1] + 0.001, input.verticalTime + 0.002);
    auto variable = [](Parameters &value, int index) -> double & {
        if (index == 0) return value.turnDegrees;
        if (index == 1) return value.turnTime;
        return value.engineCutoffTime;
    };
    auto evaluate = [&](const Parameters &candidate) {
        bool ignored = false;
        const auto last = integrate(PreparedModel(candidate), air, ctx, nullptr, ignored);
        return Residual{last.radius - EarthRadius - o.targetAltitude,
                        last.velocity - orbitalSpeed(o.targetAltitude)};
    };

    Residual residual = evaluate(p);
    auto record = [&] {
        best = p;
        diagnostics.residualAvailable = true;
        diagnostics.altitudeError = residual.h;
        diagnostics.velocityError = residual.v;
    };
    record();
    int iteration = 0;
    while (std::abs(residual.h) > o.altitudeTolerance || std::abs(residual.v) > o.velocityTolerance) {
        ctx.check();
        if (iteration++ >= o.maxIterations)
            throw Stop{Status::NotConverged, "Подбор параметров не сошёлся за заданное число итераций.", StopReason::IterationLimit};
        diagnostics.iterations = iteration;
        if (progress) progress(iteration, residual.h, residual.v);
        auto sensitivity = [&](int index) {
            const double initialStep = index == 0 ? 0.05 : 0.2;
            const double origin = variable(p, index);
            for (int reduction = 0; reduction < 10; ++reduction) {
                for (double direction : {1.0, -1.0}) {
                    ctx.check();
                    Parameters candidate = p;
                    double &value = variable(candidate, index);
                    value += direction * std::ldexp(initialStep, -reduction);
                    const double delta = value - origin;
                    if (delta == 0 || !validate(candidate, o).empty()) continue;
                    try {
                        const auto probe = evaluate(candidate);
                        const Residual derivative{(probe.h - residual.h) / delta, (probe.v - residual.v) / delta};
                        if (std::isfinite(derivative.h) && std::isfinite(derivative.v)) return derivative;
                    } catch (const std::runtime_error &) {
                        // Only a failed numerical probe is recoverable. Stop propagates.
                    }
                    ++diagnostics.rejectedProbes;
                }
            }
            throw Stop{Status::NotConverged, "Не удалось оценить чувствительность в допустимой области.", StopReason::SensitivityUnavailable};
        };
        const std::array<Residual, 3> derivatives{{sensitivity(0), sensitivity(1), sensitivity(2)}};
        const double hScale = std::max(1e-12, o.altitudeTolerance), vScale = std::max(1e-12, o.velocityTolerance);
        // Minimum-norm correction for two residuals and three scaled parameters.
        const std::array<double, 3> scales{{5, 30, 10}};
        std::array<double, 3> hColumn{}, vColumn{}, change{};
        double matrixScale = 0;
        for (int i = 0; i < 3; ++i) {
            hColumn[i] = derivatives[i].h * scales[i] / hScale;
            vColumn[i] = derivatives[i].v * scales[i] / vScale;
            matrixScale = std::max({matrixScale, std::abs(hColumn[i]), std::abs(vColumn[i])});
        }
        if (!std::isfinite(matrixScale) || matrixScale == 0)
            throw Stop{Status::NotConverged, "Подбор остановлен: некорректная чувствительность параметров.", StopReason::IllConditioned};
        for (int i = 0; i < 3; ++i) { hColumn[i] /= matrixScale; vColumn[i] /= matrixScale; }
        const double h = residual.h / hScale / matrixScale, v = residual.v / vScale / matrixScale;
        auto correction = [&](int columns) {
            double a = 0, b = 0, d = 0, determinant = 0;
            for (int i = 0; i < columns; ++i) {
                a += hColumn[i] * hColumn[i]; b += hColumn[i] * vColumn[i]; d += vColumn[i] * vColumn[i];
                for (int j = 0; j < i; ++j) {
                    const double minor = hColumn[i] * vColumn[j] - hColumn[j] * vColumn[i];
                    determinant += minor * minor; // Avoid cancellation in a*d-b*b.
                }
            }
            if (!(determinant > 1e-20 * a * d))
                throw Stop{Status::NotConverged, "Подбор остановлен: вырожденная чувствительность параметров.", StopReason::IllConditioned};
            const double x = (-h * d + b * v) / determinant, y = (b * h - a * v) / determinant;
            change.fill(0);
            for (int i = 0; i < columns; ++i) change[i] = scales[i] * (hColumn[i] * x + vColumn[i] * y);
        };
        correction(3);
        // A fuel boundary is an active constraint, not permission to burn beyond exhaustion.
        if ((p.engineCutoffTime >= fuelTimes[2] && change[2] > 0) ||
            (p.engineCutoffTime <= minimumCutoff && change[2] < 0)) correction(2);
        double factor = 1;
        for (int i = 0; i < 3; ++i) {
            if (!std::isfinite(change[i]))
                throw Stop{Status::NotConverged, "Подбор остановлен: неконечное изменение параметров.", StopReason::IllConditioned};
            factor = std::min(factor, scales[i] / std::max(1e-30, std::abs(change[i])));
        }
        for (double &value : change) value *= factor;
        bool accepted = false;
        for (double scale = 1; scale >= 1.0 / 128; scale /= 2) {
            Parameters candidate = p;
            for (int i = 0; i < 3; ++i) variable(candidate, i) += scale * change[i];
            candidate.engineCutoffTime = std::max(minimumCutoff, std::min(fuelTimes[2], candidate.engineCutoffTime));
            candidate.turnDegrees = std::max(0.0, std::min(90.0, candidate.turnDegrees));
            if (!validate(candidate, o).empty()) continue;
            try {
                const auto trial = evaluate(candidate);
                if (trial.norm(o) < residual.norm(o)) { p = candidate; residual = trial; record(); accepted = true; break; }
            } catch (const std::runtime_error &) { /* Reject an invalid candidate, retain last valid state. */ }
        }
        if (!accepted) throw Stop{Status::NotConverged, "Подбор остановлен: дальнейшее уменьшение ошибки не найдено.", StopReason::NoImprovement};
    }
    return p;
}
} }
