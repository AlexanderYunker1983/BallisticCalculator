// SPDX-License-Identifier: GPL-3.0-or-later
#include "atmosphere.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ballistic {
namespace {
constexpr double GasConstant = 287.05287;
Air makeAir(double temperature, double pressure, double density) {
    return {temperature, pressure, density, std::sqrt(1.4 * GasConstant * temperature)};
}
Air blend(const Air &a, const Air &b, double x) {
    return makeAir(a.temperature + x * (b.temperature - a.temperature),
                   a.pressure + x * (b.pressure - a.pressure),
                   a.density + x * (b.density - a.density));
}
struct UpperRow { double km, temperature, torr, density; };
const UpperRow upper[] = {
#include "upper_atmosphere.inc"
};
}
Air Atmosphere::lowerAtmosphere(double altitude) {
    // COESA 1976 lower-atmosphere equations; PDAS geometric-height convention.
    // https://www.pdas.com/atmos.html and /programs/atmos.py
    // The high-altitude legacy table below is NOT a full COESA 1976 model.
    if (!std::isfinite(altitude) || altitude < 0 || altitude > 86000)
        throw std::invalid_argument("Нижняя модель атмосферы определена от 0 до 86 км.");
    const double h = altitude * 6369000.0 / (altitude + 6369000.0);
    const double baseH[] = {0, 11000, 20000, 32000, 47000, 51000, 71000, 84852};
    const double baseT[] = {288.15, 216.65, 216.65, 228.65, 270.65, 270.65, 214.65, 186.946};
    const double baseP[] = {1, 2.2336110e-1, 5.4032950e-2, 8.5666784e-3, 1.0945601e-3,
                            6.6063531e-4, 3.9046834e-5, 3.68501e-6};
    const double lapse[] = {-0.0065, 0, 0.001, 0.0028, 0, -0.0028, -0.002, 0};
    int layer = 0;
    while (layer < 7 && h >= baseH[layer + 1]) ++layer;
    const double deltaH = h - baseH[layer];
    const double t = baseT[layer] + lapse[layer] * deltaH;
    const double ratio = lapse[layer] == 0 ? baseP[layer] * std::exp(-0.034163195 * deltaH / baseT[layer]) :
        baseP[layer] * std::pow(baseT[layer] / t, 0.034163195 / lapse[layer]);
    return makeAir(t, 101325 * ratio, 1.225 * ratio * 288.15 / t);
}
Atmosphere::Atmosphere() {
    table_.reserve(300001);
    const Air anchor = lowerAtmosphere(86000);
    std::size_t index = 0;
    for (int h = 0; h <= 300000; ++h) {
        if (h <= 86000) { table_.push_back(lowerAtmosphere(h)); continue; }
        while (index + 1 < sizeof(upper) / sizeof(upper[0]) && upper[index].km * 1000 < h) ++index;
        const auto &b = upper[index];
        const double lowerH = index == 0 ? 86000 : upper[index - 1].km * 1000;
        const auto a = index == 0 ? anchor : makeAir(upper[index - 1].temperature,
            upper[index - 1].torr * 133.322, upper[index - 1].density);
        table_.push_back(blend(a, makeAir(b.temperature, b.torr * 133.322, b.density),
                              (h - lowerH) / (b.km * 1000 - lowerH)));
    }
}
Air Atmosphere::at(double altitude) const {
    if (!std::isfinite(altitude)) throw std::invalid_argument("Неконечная высота при запросе атмосферы.");
    if (altitude <= 0) return table_.front();
    if (altitude >= 300000) return table_.back();
    const auto index = static_cast<std::size_t>(altitude);
    return blend(table_[index], table_[index + 1], altitude - index);
}
}
