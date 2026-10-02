// SPDX-License-Identifier: GPL-3.0-or-later
#include "model.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ballistic {
namespace {
bool positive(double x) { return std::isfinite(x) && x > 0; }
ValidationIssue issue(ValidationCode code, InputField field, const char *message, int stage = -1) {
    return {code, field, stage, message};
}
bool sufficientInterval(double begin, double end) {
    // A few ulps of the operands, not a physical relaxation of the 1 ms limit.
    const double rounding = 8 * std::numeric_limits<double>::epsilon() *
        std::max(1.0, std::max(std::abs(begin), std::abs(end)));
    return end > begin && end - begin + rounding >= 0.001;
}
}
ValidationIssue validateVehicle(const Parameters &p) {
    if (!positive(p.payload))
        return issue(ValidationCode::PositiveFinite, InputField::Payload, "Масса полезной нагрузки должна быть положительной и конечной.");
    double end = 0;
    for (int i = 0; i < 3; ++i) {
        const double values[] = {p.mass[i], p.fuel[i], p.thrust[i], p.exhaustVelocity[i]};
        const InputField fields[] = {InputField::Mass, InputField::Fuel, InputField::Thrust, InputField::ExhaustVelocity};
        for (int j = 0; j < 4; ++j) if (!positive(values[j]))
            return issue(ValidationCode::PositiveFinite, fields[j], "Параметр ступени должен быть положительным и конечным.", i);
        if (p.fuel[i] >= p.mass[i])
            return issue(ValidationCode::FuelMass, InputField::Fuel, "Масса топлива должна быть меньше полной массы своей ступени.", i);
        const double duration = p.fuel[i] * p.exhaustVelocity[i] / p.thrust[i];
        const double flow = p.thrust[i] / p.exhaustVelocity[i];
        if (!positive(duration) || !positive(flow) || !positive(p.mass[i] - p.fuel[i]) ||
            !std::isfinite(end + duration) || end + duration <= end || end + duration > 86400)
            return issue(ValidationCode::StageDuration, InputField::Thrust,
                         "Недопустимые расход топлива или длительность ступени; общая длительность — не более 24 часов.", i);
        end += duration;
    }
    if (!positive(p.totalMass()))
        return issue(ValidationCode::TotalMass, InputField::Payload, "Суммарная масса должна быть конечной и положительной.");
    const double startAcceleration = p.thrust[0] / p.totalMass();
    if (!std::isfinite(startAcceleration) || startAcceleration <= G0)
        return issue(ValidationCode::StartThrust, InputField::Thrust, "Тяги первой ступени недостаточно для старта или стартовое ускорение неконечно.", 0);
    return {};
}
ValidationIssue validateProgram(const Parameters &p) {
    const auto times = p.separationTimes();
    if (!std::isfinite(p.engineCutoffTime) || p.engineCutoffTime < 0 ||
        (p.engineCutoffTime > 0 && (!sufficientInterval(times[1], p.engineCutoffTime) || p.engineCutoffTime > times[2])))
        return issue(ValidationCode::CutoffTime, InputField::EngineCutoffTime,
                     "Выключение двигателя: 0 — выработка топлива; иначе время должно быть после начала третьей ступени (не менее 0,001 с) и не позже выработки топлива.");
    if (!std::isfinite(p.verticalTime) || p.verticalTime < 0)
        return issue(ValidationCode::TimeOrder, InputField::VerticalTime, "Длительность вертикального участка должна быть конечной и неотрицательной.");
    const double end = p.duration();
    if (!positive(end) || !std::isfinite(p.turnTime) ||
        !sufficientInterval(p.verticalTime, p.turnTime) || !sufficientInterval(p.turnTime, end))
        return issue(ValidationCode::TimeOrder, InputField::TurnTime,
                     "Времена должны удовлетворять условию: 0 ≤ t₀ < t₁ < время окончания работы; интервалы не менее 0,001 с.");
    if (!std::isfinite(p.turnDegrees) || p.turnDegrees < 0 || p.turnDegrees > 90)
        return issue(ValidationCode::AngleRange, InputField::TurnDegrees, "Угол φ₁ должен находиться в диапазоне 0–90°.");
    if (p.programDomain != ProgramDomain::Bounded && p.programDomain != ProgramDomain::LegacyUnbounded)
        return issue(ValidationCode::ProgramRange, InputField::TurnDegrees, "Неизвестная область программы угла.");
    const auto range = programEnvelope(p);
    if (p.programDomain == ProgramDomain::Bounded && (range.minimum < -1e-12 || range.maximum > Pi / 2 + 1e-12))
        return issue(ValidationCode::ProgramRange, InputField::TurnDegrees,
                     "Программа угла выходит за диапазон 0–90°. Уменьшите угол или время поворота.");
    return {};
}
ValidationIssue validateOptions(const Options &o) {
    if (!positive(o.maxStep) || o.maxStep < 0.000001 || o.maxStep > 1)
        return issue(ValidationCode::StepRange, InputField::MaxStep, "Максимальный шаг должен находиться в диапазоне 0,000001–1 с.");
    if (!positive(o.targetAltitude) || o.targetAltitude > 300000)
        return issue(ValidationCode::TargetRange, InputField::TargetAltitude, "Целевая высота должна находиться в диапазоне 0–300 км (исключая ноль).");
    const double values[] = {o.altitudeTolerance, o.velocityTolerance, o.timeLimitSeconds};
    const InputField fields[] = {InputField::AltitudeTolerance, InputField::VelocityTolerance, InputField::TimeLimit};
    for (int i = 0; i < 3; ++i) if (!positive(values[i]))
        return issue(ValidationCode::Limit, fields[i], "Допуски и ограничение времени должны быть положительными и конечными.");
    if (o.maxIterations <= 0) return issue(ValidationCode::Limit, InputField::MaxIterations, "Лимит итераций должен быть положительным.");
    if (o.maxEvaluations <= 0) return issue(ValidationCode::Limit, InputField::MaxEvaluations, "Лимит прогонов должен быть положительным.");
    if (o.maxSteps == 0) return issue(ValidationCode::Limit, InputField::MaxSteps, "Лимит шагов должен быть положительным.");
    return {};
}
ValidationIssue validateDetailed(const Parameters &p, const Options &o) {
    auto result = validateVehicle(p);
    if (result.empty()) result = validateProgram(p);
    if (result.empty()) result = validateOptions(o);
    return result;
}
std::string validate(const Parameters &p, const Options &o) { return validateDetailed(p, o).message; }
}
