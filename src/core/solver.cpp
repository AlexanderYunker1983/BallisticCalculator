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
struct Stop { Status status; std::string message; };
using Clock = std::chrono::steady_clock;
struct RunContext {
    const Options &o;
    const CancelCheck &cancelled;
    Clock::time_point start;
    int evaluations = 0;
    void check() const {
        if (cancelled && cancelled()) throw Stop{Status::Cancelled, "Расчёт отменён."};
        if (std::chrono::duration<double>(Clock::now() - start).count() > o.timeLimitSeconds)
            throw Stop{Status::NotConverged, "Достигнуто ограничение времени расчёта."};
    }
};
Sample integrate(const Parameters &p, const Atmosphere &air, RunContext &ctx,
                 std::vector<Sample> *output, bool &clamped) {
    ctx.check();
    if (ctx.evaluations >= ctx.o.maxEvaluations)
        throw Stop{Status::NotConverged, "Достигнут предел вычислений траектории."};
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
        if (++steps > ctx.o.maxSteps) throw Stop{Status::NotConverged, "Достигнут предел шагов интегрирования."};
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
struct Residual { double h, v; double norm() const { return std::hypot(h / 1000, v / 100); } };
}

Result Solver::run(const Parameters &input, const Options &o, CancelCheck cancelled, Progress progress) const {
    Result result; result.parameters = input; result.options = o;
    const auto start = Clock::now();
    RunContext ctx{o, cancelled, start, 0};
    try {
        const auto error = validate(input, o);
        if (!error.empty()) throw Stop{Status::InvalidInput, error};
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
            int iteration = 0;
            while (std::abs(residual.h) > o.altitudeTolerance || std::abs(residual.v) > o.velocityTolerance) {
                ctx.check();
                if (iteration++ >= o.maxIterations)
                    throw Stop{Status::NotConverged, "Подбор параметров не сошёлся за заданное число итераций."};
                if (progress) progress(iteration, residual.h, residual.v);
                double da = p.turnDegrees < 89.9 ? 0.05 : -0.05;
                double dt = p.turnTime + 0.2 < p.duration() - 0.001 ? 0.2 : -0.2;
                Parameters pa = p, pt = p;
                pa.turnDegrees += da; pt.turnTime += dt;
                if (!validate(pa, o).empty() || !validate(pt, o).empty())
                    throw Stop{Status::NotConverged, "Недостаточный интервал для подбора параметров."};
                const auto ra = evaluate(pa), rt = evaluate(pt);
                const double a = (ra.h - residual.h) / da, b = (rt.h - residual.h) / dt;
                const double c = (ra.v - residual.v) / da, d = (rt.v - residual.v) / dt;
                const double determinant = a * d - b * c;
                if (!std::isfinite(determinant) || std::abs(determinant) < 1e-12)
                    throw Stop{Status::NotConverged, "Подбор остановлен: вырожденная чувствительность параметров."};
                double angleChange = (-residual.h * d + b * residual.v) / determinant;
                double timeChange = (c * residual.h - a * residual.v) / determinant;
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
                        if (trial.norm() < residual.norm()) { p = candidate; residual = trial; accepted = true; break; }
                    } catch (const std::runtime_error &) { /* Reject an invalid candidate, retain last valid state. */ }
                }
                if (!accepted) throw Stop{Status::NotConverged, "Подбор остановлен: дальнейшее уменьшение ошибки не найдено."};
            }
        }
        result.parameters = p; // Exactly the inputs used to create the returned trajectory.
        const auto last = integrate(p, atmosphere_, ctx, &result.trajectory, result.atmosphereClamped);
        if (o.optimize && (std::abs(last.radius - EarthRadius - o.targetAltitude) > o.altitudeTolerance ||
                           std::abs(last.velocity - orbitalSpeed(o.targetAltitude)) > o.velocityTolerance))
            throw std::runtime_error("Контрольный расчёт не подтвердил найденные параметры.");
        result.status = Status::Completed;
        result.message = o.optimize ? "Подбор завершён: достигнуты допуски высоты и скорости." : "Траектория рассчитана с заданными параметрами.";
    } catch (const Stop &stop) { result.status = stop.status; result.message = stop.message; result.trajectory.clear(); }
      catch (const std::exception &ex) { result.status = Status::NumericalFailure; result.message = ex.what(); result.trajectory.clear(); }
    result.evaluations = ctx.evaluations;
    result.elapsedSeconds = std::chrono::duration<double>(Clock::now() - start).count();
    return result;
}
}
