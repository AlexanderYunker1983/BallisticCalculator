// SPDX-License-Identifier: GPL-3.0-or-later
#include "solver.h"
#include "integration.h"
#include "optimizer.h"
#include <cmath>
#include <stdexcept>
namespace ballistic {
Result Solver::run(const Parameters &input, const Options &o, CancelCheck cancelled, Progress progress) const {
    Result result; result.parameters = input; result.options = o;
    const auto start = detail::Clock::now();
    detail::RunContext ctx{o, cancelled, start, 0};
    try {
        const auto error = validate(input, o);
        if (!error.empty()) throw detail::Stop{Status::InvalidInput, error, StopReason::InvalidInput};
        ctx.check();
        Parameters p = input;
        if (o.optimize) p = detail::optimize(input, atmosphere_, ctx, progress, result.diagnostics, result.parameters);
        result.parameters = p; // Exactly the inputs used to create the returned trajectory.
        const auto last = detail::integrate(PreparedModel(p), atmosphere_, ctx, &result.trajectory, result.atmosphereClamped);
        if (o.optimize && (std::abs(last.radius - EarthRadius - o.targetAltitude) > o.altitudeTolerance ||
                           std::abs(last.velocity - orbitalSpeed(o.targetAltitude)) > o.velocityTolerance))
            throw std::runtime_error("Контрольный расчёт не подтвердил найденные параметры.");
        result.status = Status::Completed;
        result.message = o.optimize ? "Подбор завершён: достигнуты допуски высоты и скорости." : "Траектория рассчитана с заданными параметрами.";
    } catch (const detail::Stop &stop) { result.status = stop.status; result.message = stop.message; result.diagnostics.reason = stop.reason; result.trajectory.clear(); }
      catch (const std::exception &ex) { result.status = Status::NumericalFailure; result.message = ex.what(); result.diagnostics.reason = StopReason::NumericalFailure; result.trajectory.clear(); }
    result.evaluations = ctx.evaluations;
    result.elapsedSeconds = std::chrono::duration<double>(detail::Clock::now() - start).count();
    return result;
}
}
