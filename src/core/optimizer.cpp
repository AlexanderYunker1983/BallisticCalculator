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
        auto sensitivity = [&](bool angle) {
            const double initialStep = angle ? 0.05 : 0.2;
            const double origin = angle ? p.turnDegrees : p.turnTime;
            for (int reduction = 0; reduction < 10; ++reduction) {
                for (double direction : {1.0, -1.0}) {
                    ctx.check();
                    Parameters candidate = p;
                    double &value = angle ? candidate.turnDegrees : candidate.turnTime;
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
        const auto ra = sensitivity(true), rt = sensitivity(false);
        const double hScale = std::max(1e-12, o.altitudeTolerance), vScale = std::max(1e-12, o.velocityTolerance);
        // Columns use characteristic changes of 5 degrees and 30 seconds.
        double a = ra.h * 5 / hScale, b = rt.h * 30 / hScale;
        double c = ra.v * 5 / vScale, d = rt.v * 30 / vScale;
        const double matrixScale = std::max({std::abs(a), std::abs(b), std::abs(c), std::abs(d)});
        if (!std::isfinite(matrixScale) || matrixScale == 0)
            throw Stop{Status::NotConverged, "Подбор остановлен: некорректная чувствительность параметров.", StopReason::IllConditioned};
        a /= matrixScale; b /= matrixScale; c /= matrixScale; d /= matrixScale;
        const double determinant = a * d - b * c;
        if (std::abs(determinant) <= 1e-10 * std::hypot(a, c) * std::hypot(b, d))
            throw Stop{Status::NotConverged, "Подбор остановлен: вырожденная чувствительность параметров.", StopReason::IllConditioned};
        const double h = residual.h / hScale / matrixScale, v = residual.v / vScale / matrixScale;
        double angleChange = 5 * (-h * d + b * v) / determinant;
        double timeChange = 30 * (c * h - a * v) / determinant;
        if (!std::isfinite(angleChange) || !std::isfinite(timeChange))
            throw Stop{Status::NotConverged, "Подбор остановлен: неконечное изменение параметров.", StopReason::IllConditioned};
        const double factor = std::min(1.0, std::min(5.0 / std::max(1e-30, std::abs(angleChange)),
                                                  30.0 / std::max(1e-30, std::abs(timeChange))));
        angleChange *= factor; timeChange *= factor;
        bool accepted = false;
        for (double scale = 1; scale >= 1.0 / 128; scale /= 2) {
            Parameters candidate = p;
            candidate.turnDegrees += scale * angleChange;
            candidate.turnTime += scale * timeChange;
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
