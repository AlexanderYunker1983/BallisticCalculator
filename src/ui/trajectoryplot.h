// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>
#include <QPointF>
#include <QVector>
#include <array>

class TrajectoryPlot : public QWidget {
    Q_OBJECT
public:
    TrajectoryPlot(QString title, QString xLabel, QString yLabel, QWidget *parent = nullptr);
    void setData(QVector<QPointF> points, const QVector<double> &separations = {});
    int pointCount() const { return points_.size(); }
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
    QVector<QPointF> points_;
    QVector<double> separations_;
    QRectF full_, view_;
    QPoint lastMouse_, hover_;
    bool dragging_ = false, hovering_ = false, sorted_ = true;
    QRectF plotRect() const;
    QPointF screenPoint(const QPointF &p) const;
};
