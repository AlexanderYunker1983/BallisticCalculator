// SPDX-License-Identifier: GPL-3.0-or-later
#include "solver.h"
#include "dynamics.h"
#include "rk4.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>

namespace ballistic {
namespace {
struct Stop { Status status; std::string message; StopReason reason; };
using Clock = std::chrono::steady_clock;
struct RunContext {
    const Options &o;
    const CancelCheck &cancelled;
    Clock::time_point start;
    int evaluations = 0;
    void check() const {
        if (cancelled && cancelled()) throw Stop{Status::Cancelled, "Расчёт отменён.", StopReason::Cancelled};
        if (std::chrono::duration<double>(Clock::now() - start).count() > o.timeLimitSeconds)
            throw Stop{Status::NotConverged, "Достигнуто ограничение времени расчёта.", StopReason::TimeLimit};
    }
};
Sample integrate(const Parameters &p, const Atmosphere &air, RunContext &ctx,
                 std::vector<Sample> *output, bool &clamped) {
    ctx.check();
    if (ctx.evaluations >= ctx.o.maxEvaluations)
        throw Stop{Status::NotConverged, "Достигнут предел вычислений траектории.", StopReason::EvaluationLimit};
    ++ctx.evaluations;
    Dynamics dynamics(p, air);
    State state{{0, Pi / 2, EarthRadius, 0}};
    double time = 0;
    const auto times = p.separationTimes();
    std::vector<double> events{p.verticalTime, p.turnTime, times[0], times[1], times[2]};
    std::sort(events.begin(), events.end());
    events.erase(std::unique(events.begin(), events.end()), events.end());
    std::size_t nextEvent = 0, steps = 0;
    Sample sample = dynamics.sample(time, state);
    if (output) { output->clear(); output->reserve(std::min<std::size_t>(200000, ctx.o.maxSteps)); output->push_back(sample); }
    while (time < times[2]) {
        if ((steps & 127) == 0) ctx.check();
        if (++steps > ctx.o.maxSteps) throw Stop{Status::NotConverged, "Достигнут предел шагов интегрирования.", StopReason::StepLimit};
        while (nextEvent < events.size() && events[nextEvent] <= time) ++nextEvent;
        const double boundary = nextEvent < events.size() ? events[nextEvent] : times[2];
        double dt = std::min(ctx.o.maxStep, 10 / std::max(1.0, state[0]));
        const bool reachesEvent = time + dt >= boundary;
        if (reachesEvent) dt = boundary - time;
        if (!(dt > 0) || time + dt == time) throw std::runtime_error("Шаг интегрирования не продвигает время.");
        // Keep the left-hand stage for all RK evaluations in this interval.
        // The saved boundary sample and next interval use the right-hand stage.
        const int stage = dynamics.stageAt(time);
        state = rk4(state, time, dt, [&](double t, const State &s) { return dynamics.derivative(t, s, stage); });
        time = reachesEvent ? boundary : time + dt;
        sample = dynamics.sample(time, state);
        clamped = clamped || sample.radius - EarthRadius > 300000;
        if (output) output->push_back(sample);
    }
    ctx.check();
    return sample;
}
struct Residual {
    double h, v;
    double norm(const Options &o) const {
        return std::hypot(h / std::max(1e-12, o.altitudeTolerance), v / std::max(1e-12, o.velocityTolerance));
    }
};
}

Result Solver::run(const Parameters &input, const Options &o, CancelCheck cancelled, Progress progress) const {
    Result result; result.parameters = input; result.options = o;
    const auto start = Clock::now();
    RunContext ctx{o, cancelled, start, 0};
    try {
        const auto error = validate(input, o);
        if (!error.empty()) throw Stop{Status::InvalidInput, error, StopReason::InvalidInput};
        ctx.check();
        Parameters p = input;
        auto evaluate = [&](const Parameters &candidate) {
            bool ignored = false;
            const auto last = integrate(candidate, atmosphere_, ctx, nullptr, ignored);
            return Residual{last.radius - EarthRadius - o.targetAltitude,
                            last.velocity - orbitalSpeed(o.targetAltitude)};
        };
        if (o.optimize) {
            Residual residual = evaluate(p);
            auto record = [&] {
                result.parameters = p;
                result.diagnostics.residualAvailable = true;
                result.diagnostics.altitudeError = residual.h;
                result.diagnostics.velocityError = residual.v;
            };
            record();
            int iteration = 0;
            while (std::abs(residual.h) > o.altitudeTolerance || std::abs(residual.v) > o.velocityTolerance) {
                ctx.check();
                if (iteration++ >= o.maxIterations)
                    throw Stop{Status::NotConverged, "Подбор параметров не сошёлся за заданное число итераций.", StopReason::IterationLimit};
                result.diagnostics.iterations = iteration;
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
                            ++result.diagnostics.rejectedProbes;
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
        }
        result.parameters = p; // Exactly the inputs used to create the returned trajectory.
        const auto last = integrate(p, atmosphere_, ctx, &result.trajectory, result.atmosphereClamped);
        if (o.optimize && (std::abs(last.radius - EarthRadius - o.targetAltitude) > o.altitudeTolerance ||
                           std::abs(last.velocity - orbitalSpeed(o.targetAltitude)) > o.velocityTolerance))
            throw std::runtime_error("Контрольный расчёт не подтвердил найденные параметры.");
        result.status = Status::Completed;
        result.message = o.optimize ? "Подбор завершён: достигнуты допуски высоты и скорости." : "Траектория рассчитана с заданными параметрами.";
    } catch (const Stop &stop) { result.status = stop.status; result.message = stop.message; result.diagnostics.reason = stop.reason; result.trajectory.clear(); }
      catch (const std::exception &ex) { result.status = Status::NumericalFailure; result.message = ex.what(); result.diagnostics.reason = StopReason::NumericalFailure; result.trajectory.clear(); }
    result.evaluations = ctx.evaluations;
    result.elapsedSeconds = std::chrono::duration<double>(Clock::now() - start).count();
    return result;
}
}
