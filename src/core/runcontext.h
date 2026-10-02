// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "model.h"
#include <chrono>
namespace ballistic { namespace detail {
struct Stop { Status status; std::string message; StopReason reason; };
using Clock = std::chrono::steady_clock;
struct RunContext {
    const Options &o;
    const CancelCheck &cancelled;
    Clock::time_point start;
    int evaluations = 0;
    void check() const {
        if (cancelled && cancelled()) throw Stop{Status::Cancelled, "Расчёт отменён.", StopReason::Cancelled};
        if (std::chrono::duration<double>(Clock::now() - start).count() > o.timeLimitSeconds)
            throw Stop{Status::NotConverged, "Достигнуто ограничение времени расчёта.", StopReason::TimeLimit};
    }
};
} }
