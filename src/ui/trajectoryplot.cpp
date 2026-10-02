// SPDX-License-Identifier: GPL-3.0-or-later
#include "trajectoryplot.h"
#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QLocale>
#include <QEvent>
#include <algorithm>
#include <cmath>

namespace {
double tickStep(double span, int count) {
    const double raw = span / std::max(2, count), magnitude = std::pow(10, std::floor(std::log10(raw)));
    const double fraction = raw / magnitude;
    return (fraction <= 1 ? 1 : fraction <= 2 ? 2 : fraction <= 5 ? 5 : 10) * magnitude;
}
QString tickLabel(double n) { return QLocale().toString(std::abs(n) < 1e-10 ? 0 : n, 'g', 6); }
}
TrajectoryPlot::TrajectoryPlot(QString title, QString xLabel, QString yLabel, QWidget *parent)
    : QWidget(parent), title_(std::move(title)), xLabel_(std::move(xLabel)), yLabel_(std::move(yLabel)) {
    setMinimumSize(330, 240); setMouseTracking(true); setAutoFillBackground(false);
    setAccessibleName(title_);
    setToolTip(QString::fromUtf8("Колесо — масштаб; перетаскивание — сдвиг; двойной щелчок — весь график."));
}
void TrajectoryPlot::setData(QVector<QPointF> points, const QVector<double> &separations) {
    setSeries(PlotSeries(std::move(points)), separations);
}
void TrajectoryPlot::setLowerThreshold(double level) {
    lowerThreshold_ = level; hasLowerThreshold_ = std::isfinite(level);
    sceneDirty_ = true; update();
}
void TrajectoryPlot::setSeries(PlotSeries series, const QVector<double> &separations) {
    series_ = std::move(series); separations_ = separations; geometryDirty_ = true; sceneDirty_ = true;
    if (series_.points().isEmpty()) { full_ = {}; view_ = {}; geometry_.clear(); update(); return; }
    const auto bounds = series_.bounds();
    const double dx = std::max(1e-6, bounds.width()), dy = std::max(1e-6, bounds.height());
    full_ = QRectF(bounds.left() - dx * 0.025, bounds.top() - dy * 0.08, dx * 1.05, dy * 1.16);
    resetView();
}
void TrajectoryPlot::rebuildGeometry() {
    if (!geometryDirty_ && cachedView_ == view_ && cachedSize_ == size()) return;
    geometry_.clear(); auto indices = series_.visibleIndices(view_.left(), view_.right(), int(plotRect().width()));
    const auto &points = series_.points();
    for (double event : separations_) {
        if (event < view_.left() || event > view_.right()) continue;
        const int index = series_.nearestIndex(event);
        if (index >= 0) indices.append(index);
    }
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    for (int index : indices) {
        const auto point = screenPoint(points[index]);
        geometry_.append(point);
    }
    geometryDirty_ = false; cachedView_ = view_; cachedSize_ = size(); ++geometryBuilds_;
}
void TrajectoryPlot::resetView() { view_ = full_; update(); }
void TrajectoryPlot::changeEvent(QEvent *event) {
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange || event->type() == QEvent::FontChange) {
        sceneDirty_ = true; update();
    }
    QWidget::changeEvent(event);
}
QRectF TrajectoryPlot::plotRect() const { return QRectF(86, 65, std::max(20, width() - 112), std::max(20, height() - 127)); }
QPointF TrajectoryPlot::screenPoint(const QPointF &p) const {
    const auto r = plotRect();
    return {r.left() + (p.x() - view_.left()) / view_.width() * r.width(),
            r.bottom() - (p.y() - view_.top()) / view_.height() * r.height()};
}
void TrajectoryPlot::paintStatic(QPainter &painter) {
    const auto &points_ = series_.points();
    painter.setRenderHint(QPainter::Antialiasing);
    const auto colors = palette();
    const auto muted = colors.color(QPalette::Disabled, QPalette::WindowText);
    painter.fillRect(rect(), colors.color(QPalette::Base));
    auto titleFont = font(); titleFont.setPointSize(12); titleFont.setBold(true); painter.setFont(titleFont);
    painter.setPen(colors.color(QPalette::Text)); painter.drawText(QRectF(20, 16, width() - 40, 28), Qt::AlignLeft | Qt::AlignVCenter, title_);
    painter.setFont(font());
    if (points_.isEmpty()) {
        painter.setPen(muted); painter.drawText(rect().adjusted(24, 65, -24, -24), Qt::AlignCenter | Qt::TextWordWrap,
            QString::fromUtf8("Нажмите «Рассчитать», чтобы построить траекторию")); return;
    }
    const auto area = plotRect();
    const QColor thresholdColor(220, 38, 38);
    const double thresholdY = screenPoint({view_.left(), lowerThreshold_}).y();
    if (hasLowerThreshold_) {
        // Clamp to the viewport: below the threshold may cover all or none of it.
        const double fillTop = std::max(area.top(), std::min(area.bottom(), thresholdY));
        QColor fill = thresholdColor; fill.setAlpha(48);
        painter.fillRect(QRectF(area.left(), fillTop, area.width(), area.bottom() - fillTop), fill);
    }
    const double xStep = tickStep(view_.width(), int(area.width() / 105));
    const double yStep = tickStep(view_.height(), int(area.height() / 65));
    const double firstX = std::ceil(view_.left() / xStep) * xStep;
    for (int tick = 0; tick < 100; ++tick) {
        const double x = firstX + tick * xStep; if (x > view_.right()) break;
        const double sx = screenPoint({x, view_.top()}).x();
        painter.setPen(QPen(colors.color(QPalette::AlternateBase), 1)); painter.drawLine(QPointF(sx, area.top()), QPointF(sx, area.bottom()));
        painter.setPen(muted); painter.drawText(QRectF(sx - 50, area.bottom() + 8, 100, 22), Qt::AlignHCenter, tickLabel(x));
    }
    const double firstY = std::ceil(view_.top() / yStep) * yStep;
    for (int tick = 0; tick < 100; ++tick) {
        const double y = firstY + tick * yStep; if (y > view_.bottom()) break;
        const double sy = screenPoint({view_.left(), y}).y();
        painter.setPen(QPen(colors.color(QPalette::AlternateBase), 1)); painter.drawLine(QPointF(area.left(), sy), QPointF(area.right(), sy));
        painter.setPen(muted); painter.drawText(QRectF(24, sy - 10, 54, 22), Qt::AlignRight | Qt::AlignVCenter, tickLabel(y));
    }
    painter.setPen(colors.color(QPalette::Mid)); painter.drawRect(area);
    painter.setPen(muted); painter.drawText(QRectF(area.left(), height() - 30, area.width(), 24), Qt::AlignCenter, xLabel_);
    painter.save(); painter.translate(15, area.center().y()); painter.rotate(-90);
    painter.drawText(QRectF(-area.height() / 2, -12, area.height(), 24), Qt::AlignCenter, yLabel_); painter.restore();
    painter.save(); painter.setClipRect(area.adjusted(-1, -1, 1, 1));
    if (hasLowerThreshold_ && thresholdY >= area.top() && thresholdY <= area.bottom()) {
        painter.setPen(QPen(thresholdColor, 1.5));
        painter.drawLine(QPointF(area.left(), thresholdY), QPointF(area.right(), thresholdY));
    }
    painter.setPen(QPen(colors.color(QPalette::BrightText), 1, Qt::DashLine));
    for (int i = 0; i < separations_.size(); ++i) {
        if (separations_[i] < view_.left() || separations_[i] > view_.right()) continue;
        const double x = screenPoint({separations_[i], 0}).x();
        painter.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
        const double labelX = std::max(area.left() + 4, std::min(x + 4, area.right() - 58));
        const auto label = i == 2 ? QString::fromUtf8("Выкл.") : QString::fromUtf8("%1-я ст.").arg(i + 1);
        painter.drawText(QRectF(labelX, area.top() + 4, 54, 20), label);
    }
    rebuildGeometry();
    painter.setPen(QPen(colors.color(QPalette::Highlight), 1.8));
    // Bound the rasterizer's stroke complexity for dense, oscillating curves.
    for (int first = 0; first + 1 < geometry_.size(); first += 15)
        painter.drawPolyline(geometry_.constData() + first, std::min(16, geometry_.size() - first));
    painter.restore();
}
void TrajectoryPlot::paintEvent(QPaintEvent *) {
    const qreal dpr = devicePixelRatioF();
    if (sceneDirty_ || scene_.isNull() || sceneSize_ != size() || sceneView_ != view_ || scene_.devicePixelRatioF() != dpr) {
        scene_ = QPixmap(qRound(width() * dpr), qRound(height() * dpr)); scene_.setDevicePixelRatio(dpr);
        QPainter background(&scene_); paintStatic(background);
        sceneSize_ = size(); sceneView_ = view_; sceneDirty_ = false;
    }
    QPainter painter(this); painter.drawPixmap(0, 0, scene_);
    const auto &points = series_.points(); const auto area = plotRect();
    if (points.isEmpty() || !hovering_ || dragging_ || !area.contains(hover_)) return;
    const double x = view_.left() + (hover_.x() - area.left()) / area.width() * view_.width();
    const int nearest = series_.nearestIndex(x); const auto point = screenPoint(points[nearest]);
    const auto muted = palette().color(QPalette::Disabled, QPalette::WindowText);
    painter.save(); painter.setClipRect(area); painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(muted, 1, Qt::DashLine));
    painter.drawLine(QPointF(point.x(), area.top()), QPointF(point.x(), area.bottom()));
    painter.setBrush(palette().color(QPalette::Highlight)); painter.drawEllipse(point, 3, 3); painter.restore();
    const QString text = QString::fromUtf8("x: %1   y: %2").arg(tickLabel(points[nearest].x()), tickLabel(points[nearest].y()));
    painter.setPen(palette().color(QPalette::Text)); painter.drawText(QRectF(20, 43, width() - 45, 20), Qt::AlignRight, text);
}
void TrajectoryPlot::wheelEvent(QWheelEvent *e) {
    if (series_.points().isEmpty() || !plotRect().contains(e->pos())) return;
    const double scale = std::pow(1.2, -e->angleDelta().y() / 120.0);
    const auto area = plotRect();
    const double fx = (e->pos().x() - area.left()) / area.width();
    const double fy = (area.bottom() - e->pos().y()) / area.height();
    const double w = view_.width() * scale, h = view_.height() * scale;
    if (w < full_.width() * 1e-6 || h < full_.height() * 1e-6 || w > full_.width() * 100) return;
    view_ = QRectF(view_.left() + fx * (view_.width() - w), view_.top() + fy * (view_.height() - h), w, h);
    update(); e->accept();
}
void TrajectoryPlot::mousePressEvent(QMouseEvent *e) {
    if (!series_.points().isEmpty() && e->button() == Qt::LeftButton && plotRect().contains(e->pos())) {
        dragging_ = true; lastMouse_ = e->pos(); setCursor(Qt::ClosedHandCursor);
    }
}
void TrajectoryPlot::mouseMoveEvent(QMouseEvent *e) {
    if (dragging_) {
        const auto delta = e->pos() - lastMouse_; const auto area = plotRect();
        view_.translate(-delta.x() * view_.width() / area.width(), delta.y() * view_.height() / area.height());
        lastMouse_ = e->pos();
    }
    hover_ = e->pos(); hovering_ = true; update();
}
void TrajectoryPlot::mouseReleaseEvent(QMouseEvent *) { dragging_ = false; unsetCursor(); update(); }
void TrajectoryPlot::mouseDoubleClickEvent(QMouseEvent *) { resetView(); }
void TrajectoryPlot::leaveEvent(QEvent *) { hovering_ = false; update(); }
