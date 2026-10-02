// SPDX-License-Identifier: GPL-3.0-or-later
#include "integration.h"
#include "dynamics.h"
#include "rk4.h"
#include <algorithm>
#include <stdexcept>
namespace ballistic { namespace detail {
Sample integrate(const PreparedModel &model, const Atmosphere &air, RunContext &ctx,
                 std::vector<Sample> *output, bool &clamped) {
    ctx.check();
    if (ctx.evaluations >= ctx.o.maxEvaluations)
        throw Stop{Status::NotConverged, "Достигнут предел вычислений траектории.", StopReason::EvaluationLimit};
    ++ctx.evaluations;
    Dynamics dynamics(model, air);
    State state{{0, Pi / 2, EarthRadius, 0}};
    double time = 0;
    const auto &p = model.parameters();
    const auto &times = model.separationTimes();
    const double end = model.duration();
    std::vector<double> events{p.verticalTime, p.turnTime, times[0], times[1], end};
    std::sort(events.begin(), events.end());
    events.erase(std::unique(events.begin(), events.end()), events.end());
    std::size_t nextEvent = 0, steps = 0;
    Sample sample = dynamics.sample(time, state);
    if (output) { output->clear(); output->reserve(std::min<std::size_t>(200000, ctx.o.maxSteps)); output->push_back(sample); }
    while (time < end) {
        if ((steps & 127) == 0) ctx.check();
        if (++steps > ctx.o.maxSteps) throw Stop{Status::NotConverged, "Достигнут предел шагов интегрирования.", StopReason::StepLimit};
        while (nextEvent < events.size() && events[nextEvent] <= time) ++nextEvent;
        const double boundary = nextEvent < events.size() ? events[nextEvent] : end;
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
} }
