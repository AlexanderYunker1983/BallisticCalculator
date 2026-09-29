// SPDX-License-Identifier: GPL-3.0-or-later
#include "model.h"
#include <cmath>
#include <numeric>

namespace ballistic {
std::array<double, 3> Parameters::separationTimes() const {
    std::array<double, 3> result{};
    double sum = 0;
    for (std::size_t i = 0; i < 3; ++i) {
        sum += fuel[i] * exhaustVelocity[i] / thrust[i];
        result[i] = sum;
    }
    return result;
}
double Parameters::totalMass() const {
    return std::accumulate(mass.begin(), mass.end(), payload);
}
std::string validate(const Parameters &p, const Options &o) {
    auto positive = [](double x) { return std::isfinite(x) && x > 0; };
    if (!positive(p.payload)) return "Масса полезной нагрузки должна быть положительной и конечной.";
    for (std::size_t i = 0; i < 3; ++i) {
        if (!positive(p.mass[i]) || !positive(p.fuel[i]) || !positive(p.thrust[i]) ||
            !positive(p.exhaustVelocity[i]))
            return "Массы, топливо, тяга и удельный импульс всех ступеней должны быть положительными и конечными.";
        if (p.fuel[i] >= p.mass[i]) return "Масса топлива должна быть меньше полной массы своей ступени.";
    }
    const auto times = p.separationTimes();
    if (!positive(p.totalMass()) || !positive(times[2]) || times[2] > 86400 ||
        !(times[0] < times[1] && times[1] < times[2]))
        return "Недопустимая длительность работы ступеней (максимум 24 часа).";
    if (!std::isfinite(p.verticalTime) || !std::isfinite(p.turnTime) ||
        p.verticalTime < 0 || p.turnTime - p.verticalTime < 0.001 || times[2] - p.turnTime < 0.001)
        return "Времена должны удовлетворять условию: 0 ≤ t₀ < t₁ < время окончания работы; интервалы не менее 0,001 с.";
    if (!std::isfinite(p.turnDegrees) || p.turnDegrees < 0 || p.turnDegrees > 90)
        return "Угол φ₁ должен находиться в диапазоне 0–90°.";
    if (!positive(o.maxStep) || o.maxStep < 0.000001 || o.maxStep > 1)
        return "Максимальный шаг должен находиться в диапазоне 0,000001–1 с.";
    if (!positive(o.targetAltitude) || o.targetAltitude > 300000)
        return "Целевая высота должна находиться в диапазоне 0–300 км (исключая ноль).";
    if (!positive(o.altitudeTolerance) || !positive(o.velocityTolerance) ||
        !positive(o.timeLimitSeconds) || o.maxIterations <= 0 || o.maxEvaluations <= 0 || o.maxSteps == 0)
        return "Некорректные допуски или ограничения расчёта.";
    if (p.thrust[0] / p.totalMass() <= G0)
        return "Тяги первой ступени недостаточно для старта с заданной массой.";
    return {};
}
double programAngle(const Parameters &p, double t) {
    if (t <= p.verticalTime) return Pi / 2;
    const double end = p.duration();
    if (t >= end) return 0;
    const double phi = p.turnDegrees * Pi / 180;
    if (t >= p.turnTime) return phi * (end - t) / (end - p.turnTime);
    // The same quadratic as the original, evaluated about t1 to avoid cancellation.
    const double interval = p.verticalTime - p.turnTime;
    const double slope = -phi / (end - p.turnTime);
    const double a = (Pi / 2 - phi - slope * interval) / (interval * interval);
    const double x = t - p.turnTime;
    return phi + slope * x + a * x * x;
}
double orbitalSpeed(double altitude) {
    return std::sqrt(G0 * EarthRadius * EarthRadius / (EarthRadius + altitude));
}
}
