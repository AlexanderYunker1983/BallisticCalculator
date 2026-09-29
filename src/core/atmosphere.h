// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <vector>
namespace ballistic {
struct Air { double temperature, pressure, density, soundSpeed; };
class Atmosphere {
public:
    Atmosphere();
    Air at(double altitude) const;
    static Air lowerAtmosphere(double altitude);
private:
    std::vector<Air> table_; // Heap storage; immutable after construction.
};
}
