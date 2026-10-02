// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "model.h"

namespace ballistic {
// Validated immutable snapshot. Derived vehicle/program values are computed once.
class PreparedModel {
public:
    explicit PreparedModel(const Parameters &parameters);
    const Parameters &parameters() const { return parameters_; }
    const std::array<double, 3> &separationTimes() const { return times_; }
    double duration() const { return times_[2]; }
    int stageAt(double time) const;
    double mass(double time, int stage) const;
    double angle(double time) const;
private:
    Parameters parameters_;
    std::array<double, 3> times_, startingMass_;
    double phi_, slope_, quadratic_;
};
}
