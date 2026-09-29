// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "model.h"
#include "atmosphere.h"
namespace ballistic {
class Solver {
public:
    Result run(const Parameters &parameters, const Options &options = Options{},
               CancelCheck cancelled = {}, Progress progress = {}) const;
private:
    Atmosphere atmosphere_;
};
}
