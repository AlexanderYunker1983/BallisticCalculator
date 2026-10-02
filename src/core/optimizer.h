// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "runcontext.h"
#include "atmosphere.h"
namespace ballistic { namespace detail {
Parameters optimize(const Parameters &input, const Atmosphere &air, RunContext &context,
                    const Progress &progress, Diagnostics &diagnostics, Parameters &best);
} }
