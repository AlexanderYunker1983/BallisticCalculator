// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "runcontext.h"
#include "preparedmodel.h"
#include "atmosphere.h"
namespace ballistic { namespace detail {
Sample integrate(const PreparedModel &model, const Atmosphere &air, RunContext &context,
                 std::vector<Sample> *output, bool &clamped);
} }
