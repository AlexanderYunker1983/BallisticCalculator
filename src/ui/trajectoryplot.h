// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>
#include <QPointF>
#include <QVector>
#include <array>
#include <QPixmap>
#include "plotseries.h"

class TrajectoryPlot : public QWidget {
    Q_OBJECT
public:
    TrajectoryPlot(QString title, QString xLabel, QString yLabel, QWidget *parent = nullptr);
    void setData(QVector<QPointF> points, const QVector<double> &separations = {});
    void setSeries(PlotSeries series, const QVector<double> &separations = {});
    int pointCount() const { return series_.points().size(); }
    quint64 geometryBuildCount() const { return geometryBuilds_; }
    int geometryPointCount() const { return geometry_.size(); }
    QRectF viewRange() const { return view_; }
    void resetView();
protected:
    void changeEvent(QEvent *) override;
    void paintEvent(QPaintEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;
private:
    QString title_, xLabel_, yLabel_;
    PlotSeries series_;
    QVector<double> separations_;
    QRectF full_, view_;
    QPoint lastMouse_, hover_;
    bool dragging_ = false, hovering_ = false, geometryDirty_ = true;
    QVector<QPointF> geometry_;
    QRectF cachedView_;
    QSize cachedSize_;
    quint64 geometryBuilds_ = 0;
    void rebuildGeometry();
    void paintStatic(QPainter &painter);
    QPixmap scene_;
    QRectF sceneView_;
    QSize sceneSize_;
    bool sceneDirty_ = true;
    QRectF plotRect() const;
    QPointF screenPoint(const QPointF &p) const;
};
