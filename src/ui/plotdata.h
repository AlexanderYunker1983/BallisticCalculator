// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "plotseries.h"
#include "model.h"
#include <array>
struct PlotData {
    std::array<PlotSeries, 7> series;
    QVector<double> events;
};
PlotData preparePlotData(const ballistic::Result &result, const ballistic::CancelCheck &cancelled = {});
