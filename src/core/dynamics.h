// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "atmosphere.h"
#include "model.h"
#include "preparedmodel.h"
namespace ballistic {
class Dynamics {
public:
    Dynamics(const Parameters &p, const Atmosphere &air) : model_(p), air_(air) {}
    Dynamics(const PreparedModel &model, const Atmosphere &air) : model_(model), air_(air) {}
    int stageAt(double time) const;
    double mass(double time, int stage) const;
    State derivative(double time, const State &state, int stage) const;
    Sample sample(double time, const State &state) const;
private:
    PreparedModel model_;
    const Atmosphere &air_;
    Sample evaluate(double time, const State &state, int stage, State *derivative) const;
};
}
