// SPDX-License-Identifier: GPL-3.0-or-later
#include "preparedmodel.h"
#include <cmath>
#include <stdexcept>

namespace ballistic {
PreparedModel::PreparedModel(const Parameters &p) : parameters_(p) {
    auto error = validateVehicle(p);
    if (error.empty()) error = validateProgram(p);
    if (!error.empty()) throw std::invalid_argument(error.message);
    times_ = p.separationTimes();
    endTime_ = p.duration();
    for (int i = 0; i < 3; ++i) {
        startingMass_[i] = p.payload;
        for (int j = i; j < 3; ++j) startingMass_[i] += p.mass[j];
    }
    phi_ = p.turnDegrees * Pi / 180;
    slope_ = -phi_ / (duration() - p.turnTime);
    const double interval = p.verticalTime - p.turnTime;
    quadratic_ = (Pi / 2 - phi_ - slope_ * interval) / (interval * interval);
}
int PreparedModel::stageAt(double time) const {
    if (time >= endTime_) return 3;
    for (int i = 0; i < 3; ++i) if (time < times_[i]) return i;
    return 3;
}
double PreparedModel::mass(double time, int stage) const {
    if (stage < 0 || stage > 3 || !std::isfinite(time)) throw std::invalid_argument("Недопустимая ступень или время.");
    if (stage == 3) {
        // An early cutoff stops consumption; it does not jettison the last stage.
        if (endTime_ < times_[2]) return mass(endTime_, 2);
        return parameters_.payload;
    }
    const double start = stage == 0 ? 0 : times_[stage - 1];
    // Retain arithmetic order for reproducibility at stage boundaries.
    return startingMass_[stage] - (time - start) * parameters_.thrust[stage] / parameters_.exhaustVelocity[stage];
}
double PreparedModel::angle(double time) const {
    if (time <= parameters_.verticalTime) return Pi / 2;
    if (time >= duration()) return 0;
    if (time >= parameters_.turnTime) return phi_ * (duration() - time) / (duration() - parameters_.turnTime);
    const double x = time - parameters_.turnTime;
    return phi_ + slope_ * x + quadratic_ * x * x;
}
}
