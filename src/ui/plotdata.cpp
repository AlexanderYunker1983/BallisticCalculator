// SPDX-License-Identifier: GPL-3.0-or-later
#include "plotdata.h"
#include <limits>
#include <stdexcept>
PlotData preparePlotData(const ballistic::Result &result, const ballistic::CancelCheck &cancelled) {
    PlotData plots;
    if (result.trajectory.size() > std::size_t(std::numeric_limits<int>::max())) throw std::length_error("Слишком много точек графика.");
    QVector<QPointF> data[7]; for (auto &series : data) series.reserve(int(result.trajectory.size()));
    std::size_t count = 0;
    for (const auto &s : result.trajectory) {
        if ((count++ & 4095) == 0 && cancelled && cancelled()) return {};
        const double h = (s.radius - ballistic::EarthRadius) / 1000, range = ballistic::EarthRadius * s.arc / 1000;
        data[0].append({s.time, s.velocity}); data[1].append({s.time, h});
        data[2].append({s.time, s.alpha * 180 / ballistic::Pi}); data[3].append({s.time, s.phi * 180 / ballistic::Pi});
        data[4].append({s.time, s.overload}); data[5].append({s.time, range}); data[6].append({range, h});
    }
    for (int i = 0; i < 7; ++i) {
        if (cancelled && cancelled()) return {};
        plots.series[i] = PlotSeries(std::move(data[i]));
    }
    for (double time : result.parameters.separationTimes())
        if (time < result.parameters.duration()) plots.events.append(time);
    plots.events.append(result.parameters.duration());
    return plots;
}
