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
