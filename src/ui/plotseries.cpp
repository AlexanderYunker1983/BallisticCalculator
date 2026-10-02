// SPDX-License-Identifier: GPL-3.0-or-later
#include "plotseries.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace { constexpr int BlockSize = 64; }
PlotSeries::Extrema PlotSeries::merge(const Extrema &a, const Extrema &b) const {
    if (a.first < 0) return b;
    if (b.first < 0) return a;
    return {a.first, b.last, points_[a.min].y() <= points_[b.min].y() ? a.min : b.min,
            points_[a.max].y() >= points_[b.max].y() ? a.max : b.max};
}
PlotSeries::PlotSeries(QVector<QPointF> points) : points_(std::move(points)) {
    double minX = 0, maxX = 0, minY = 0, maxY = 0;
    if (!points_.isEmpty()) { minX = maxX = points_[0].x(); minY = maxY = points_[0].y(); }
    for (int i = 0; i < points_.size(); ++i) {
        if (!std::isfinite(points_[i].x()) || !std::isfinite(points_[i].y()))
            throw std::invalid_argument("Неконечная координата графика.");
        if (i && points_[i].x() < points_[i - 1].x()) sorted_ = false;
        minX = std::min(minX, points_[i].x()); maxX = std::max(maxX, points_[i].x());
        minY = std::min(minY, points_[i].y()); maxY = std::max(maxY, points_[i].y());
    }
    if (points_.isEmpty()) return;
    bounds_ = QRectF(minX, minY, maxX - minX, maxY - minY);
    if (!sorted_) {
        xOrder_.resize(points_.size()); std::iota(xOrder_.begin(), xOrder_.end(), 0);
        std::stable_sort(xOrder_.begin(), xOrder_.end(), [&](int a, int b) { return points_[a].x() < points_[b].x(); });
        return;
    }
    const int blocks = (points_.size() - 1) / BlockSize + 1;
    leaves_ = 1; while (leaves_ < blocks) leaves_ *= 2;
    tree_.resize(2 * leaves_);
    for (int i = 0; i < points_.size(); ++i) {
        auto &leaf = tree_[leaves_ + i / BlockSize];
        leaf = merge(leaf, {i, i, i, i});
    }
    for (int node = leaves_ - 1; node > 0; --node) tree_[node] = merge(tree_[2 * node], tree_[2 * node + 1]);
}
void PlotSeries::visit(int node, int first, int end, const std::function<int(int)> &column,
                       const std::function<void(const Extrema &)> &append) const {
    const auto &range = tree_[node];
    if (range.first < 0 || range.last < first || range.first >= end) return;
    if (range.first >= first && range.last < end && column(range.first) == column(range.last)) { append(range); return; }
    if (node >= leaves_) {
        for (int i = std::max(first, range.first); i < std::min(end, range.last + 1); ++i) append({i, i, i, i});
    } else { visit(node * 2, first, end, column, append); visit(node * 2 + 1, first, end, column, append); }
}
QVector<int> PlotSeries::visibleIndices(double left, double right, int columns) const {
    QVector<int> indices;
    if (points_.isEmpty() || !(right > left) || columns <= 0) return indices;
    auto column = [&](int i) {
        const double x = (points_[i].x() - left) / (right - left) * columns;
        return int(std::floor(std::max(-1e6, std::min(1e6, x))));
    };
    Extrema pending;
    auto flush = [&] {
        if (pending.first < 0) return;
        std::array<int, 4> selected{{pending.first, pending.min, pending.max, pending.last}};
        std::sort(selected.begin(), selected.end());
        for (int i : selected) if (indices.isEmpty() || indices.back() != i) indices.append(i);
    };
    auto append = [&](const Extrema &range) {
        if (pending.first >= 0 && column(pending.first) != column(range.first)) { flush(); pending = {}; }
        pending = merge(pending, range);
    };
    if (sorted_) {
        auto lower = std::lower_bound(points_.cbegin(), points_.cend(), left, [](const QPointF &p, double x) { return p.x() < x; });
        auto upper = std::upper_bound(points_.cbegin(), points_.cend(), right, [](double x, const QPointF &p) { return x < p.x(); });
        const int first = std::max(0, int(lower - points_.cbegin()) - 1);
        const int end = std::min(points_.size(), int(upper - points_.cbegin()) + 1);
        visit(1, first, end, column, append);
    } else {
        for (int i = 0; i < points_.size(); ++i) append({i, i, i, i});
    }
    flush(); return indices;
}
int PlotSeries::nearestIndex(double x) const {
    if (points_.isEmpty()) return -1;
    auto index = [&](int position) { return sorted_ ? position : xOrder_[position]; };
    int begin = 0, end = points_.size();
    while (begin < end) { const int mid = begin + (end - begin) / 2; if (points_[index(mid)].x() < x) begin = mid + 1; else end = mid; }
    int nearest = std::min(begin, points_.size() - 1);
    if (nearest > 0 && std::abs(points_[index(nearest - 1)].x() - x) < std::abs(points_[index(nearest)].x() - x)) --nearest;
    return index(nearest);
}
qint64 PlotSeries::storageBytes() const {
    return qint64(points_.capacity()) * sizeof(QPointF) + qint64(tree_.capacity()) * sizeof(Extrema) + qint64(xOrder_.capacity()) * sizeof(int);
}
