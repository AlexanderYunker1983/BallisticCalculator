// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstddef>

namespace ballistic {
// Time is an independent argument, never a component of the integrated state.
template<std::size_t N, typename Rhs>
std::array<double, N> rk4(const std::array<double, N> &y, double t, double dt, Rhs rhs) {
    const auto k1 = rhs(t, y);
    std::array<double, N> temp{};
    for (std::size_t i = 0; i < N; ++i) temp[i] = y[i] + dt * k1[i] / 2;
    const auto k2 = rhs(t + dt / 2, temp);
    for (std::size_t i = 0; i < N; ++i) temp[i] = y[i] + dt * k2[i] / 2;
    const auto k3 = rhs(t + dt / 2, temp);
    for (std::size_t i = 0; i < N; ++i) temp[i] = y[i] + dt * k3[i];
    const auto k4 = rhs(t + dt, temp);
    for (std::size_t i = 0; i < N; ++i)
        temp[i] = y[i] + dt * (k1[i] + 2 * k2[i] + 2 * k3[i] + k4[i]) / 6;
    return temp;
}
}
