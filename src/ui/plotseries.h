// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QPointF>
#include <QVector>
#include <QRectF>
#include <functional>

// Immutable after construction; safe to prepare in the calculation thread.
class PlotSeries {
public:
    explicit PlotSeries(QVector<QPointF> points = {});
    const QVector<QPointF> &points() const { return points_; }
    QVector<int> visibleIndices(double left, double right, int columns) const;
    int nearestIndex(double x) const;
    qint64 storageBytes() const;
    QRectF bounds() const { return bounds_; }
private:
    struct Extrema { int first = -1, last = -1, min = -1, max = -1; };
    QVector<QPointF> points_;
    QVector<Extrema> tree_;
    QVector<int> xOrder_;
    int leaves_ = 0;
    bool sorted_ = true;
    QRectF bounds_;
    Extrema merge(const Extrema &a, const Extrema &b) const;
    void visit(int node, int first, int end, const std::function<int(int)> &column,
               const std::function<void(const Extrema &)> &append) const;
};
