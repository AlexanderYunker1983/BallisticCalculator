// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "atmosphere.h"
#include "model.h"
namespace ballistic {
class Dynamics {
public:
    Dynamics(const Parameters &p, const Atmosphere &air) : p_(p), air_(air), times_(p.separationTimes()) {}
    int stageAt(double time) const;
    double mass(double time, int stage) const;
    State derivative(double time, const State &state, int stage) const;
    Sample sample(double time, const State &state) const;
private:
    Parameters p_;
    const Atmosphere &air_;
    std::array<double, 3> times_;
    Sample evaluate(double time, const State &state, int stage, State *derivative) const;
};
}
