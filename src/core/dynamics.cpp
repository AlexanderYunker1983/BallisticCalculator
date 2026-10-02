// SPDX-License-Identifier: GPL-3.0-or-later
#include "dynamics.h"
#include <cmath>
#include <stdexcept>

namespace ballistic {
int Dynamics::stageAt(double t) const {
    return model_.stageAt(t);
}
double Dynamics::mass(double t, int stage) const {
    return model_.mass(t, stage);
}
Sample Dynamics::evaluate(double t, const State &s, int stage, State *derivative) const {
    for (double value : s) if (!std::isfinite(value)) throw std::runtime_error("Неконечное состояние траектории.");
    if (s[2] <= 0 || s[0] < -0.01 || s[2] < EarthRadius - 1)
        throw std::runtime_error("Траектория вышла за область модели: падение или отрицательная скорость.");
    Sample q;
    q.time = t; q.velocity = s[0]; q.theta = s[1]; q.radius = s[2]; q.arc = s[3];
    q.phi = model_.angle(t);
    q.alpha = q.phi - q.theta + q.arc;
    q.mass = mass(t, stage);
    if (!std::isfinite(q.mass) || q.mass <= 0) throw std::runtime_error("Неположительная масса в расчёте.");
    const auto air = air_.at(q.radius - EarthRadius);
    q.density = air.density; q.mach = q.velocity / air.soundSpeed;
    const double m = q.mach;
    const double cx = m <= 0.8 ? 0.29 : m <= 1.068 ? m - 0.51 : 0.089 + 0.5 / m;
    const double cya = m <= 0.25 ? 2.8 : m <= 1.1 ? 2.8 + 0.447 * (m - 0.25) :
                       m <= 1.6 ? 3.18 - 0.660 * (m - 1.1) :
                       m <= 3.6 ? 2.85 + 0.350 * (m - 1.6) : 3.55;
    const double cy = (cya - cx) * q.alpha;
    const double thrust = stage < 3 ? model_.parameters().thrust[stage] : 0;
    const double pressureArea = air.density * q.velocity * q.velocity * ReferenceArea / 2;
    const double axial = thrust - pressureArea * cx;
    const double lift = pressureArea * cy;
    const double tangent = (axial * std::cos(q.alpha) - lift * std::sin(q.alpha)) / q.mass;
    const double normal = (axial * std::sin(q.alpha) + lift * std::cos(q.alpha)) / q.mass;
    const double gravity = G0 * EarthRadius * EarthRadius / (q.radius * q.radius);
    q.acceleration = tangent - gravity * std::sin(q.theta);
    q.overload = std::hypot(tangent, normal) / G0;
    for (double value : {q.phi, q.alpha, q.mass, q.acceleration, q.density, q.mach, q.overload})
        if (!std::isfinite(value)) throw std::runtime_error("Неконечные параметры точки траектории.");
    if (derivative) {
        (*derivative)[0] = q.acceleration;
        (*derivative)[1] = std::abs(q.velocity) <= 0.5 ? 0 : normal / q.velocity -
            std::cos(q.theta) * (gravity / q.velocity - q.velocity / q.radius);
        (*derivative)[2] = q.velocity * std::sin(q.theta);
        (*derivative)[3] = q.velocity / q.radius * std::cos(q.theta);
        for (double value : *derivative)
            if (!std::isfinite(value)) throw std::runtime_error("Неконечная производная.");
    }
    return q;
}
State Dynamics::derivative(double t, const State &s, int stage) const {
    State d{}; evaluate(t, s, stage, &d); return d;
}
Sample Dynamics::sample(double t, const State &s) const { return evaluate(t, s, stageAt(t), nullptr); }
}
