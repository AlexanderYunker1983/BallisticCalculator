// SPDX-License-Identifier: GPL-3.0-or-later
#include "trajectoryplot.h"
#include <QPainter>
#include <QPainterPath>
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
int columnOf(double x) { return int(std::floor(std::max(-1000000.0, std::min(1000000.0, x)))); }
}
TrajectoryPlot::TrajectoryPlot(QString title, QString xLabel, QString yLabel, QWidget *parent)
    : QWidget(parent), title_(std::move(title)), xLabel_(std::move(xLabel)), yLabel_(std::move(yLabel)) {
    setMinimumSize(330, 240); setMouseTracking(true); setAutoFillBackground(false);
    setAccessibleName(title_);
    setToolTip(QString::fromUtf8("Колесо — масштаб; перетаскивание — сдвиг; двойной щелчок — весь график."));
}
void TrajectoryPlot::setData(QVector<QPointF> points, const QVector<double> &separations) {
    points_ = std::move(points); separations_ = separations;
    if (points_.isEmpty()) { update(); return; }
    double minX = points_[0].x(), maxX = minX, minY = points_[0].y(), maxY = minY;
    sorted_ = true;
    for (int i = 0; i < points_.size(); ++i) {
        const auto &p = points_[i]; minX = std::min(minX, p.x()); maxX = std::max(maxX, p.x());
        minY = std::min(minY, p.y()); maxY = std::max(maxY, p.y());
        if (i && points_[i - 1].x() > p.x()) sorted_ = false;
    }
    const double dx = std::max(1e-6, maxX - minX), dy = std::max(1e-6, maxY - minY);
    full_ = QRectF(minX - dx * 0.025, minY - dy * 0.08, dx * 1.05, dy * 1.16);
    resetView();
}
void TrajectoryPlot::resetView() { view_ = full_; update(); }
void TrajectoryPlot::changeEvent(QEvent *event) {
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) update();
    QWidget::changeEvent(event);
}
QRectF TrajectoryPlot::plotRect() const { return QRectF(86, 65, std::max(20, width() - 112), std::max(20, height() - 127)); }
QPointF TrajectoryPlot::screenPoint(const QPointF &p) const {
    const auto r = plotRect();
    return {r.left() + (p.x() - view_.left()) / view_.width() * r.width(),
            r.bottom() - (p.y() - view_.top()) / view_.height() * r.height()};
}
void TrajectoryPlot::paintEvent(QPaintEvent *) {
    QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
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
    painter.setPen(QPen(colors.color(QPalette::BrightText), 1, Qt::DashLine));
    for (int i = 0; i < separations_.size(); ++i) {
        const double x = screenPoint({separations_[i], 0}).x();
        painter.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
        const double labelX = std::max(area.left() + 4, std::min(x + 4, area.right() - 58));
        painter.drawText(QRectF(labelX, area.top() + 4, 54, 20), QString::fromUtf8("%1-я ст.").arg(i + 1));
    }
    // Keep first/min/max/last in each consecutive screen column, preserving
    // extrema and their temporal order while limiting path complexity.
    QPainterPath path; bool started = false;
    int first = 0, end = points_.size();
    if (sorted_) {
        auto lower = std::lower_bound(points_.cbegin(), points_.cend(), view_.left(), [](const QPointF &p, double x) { return p.x() < x; });
        auto upper = std::upper_bound(points_.cbegin(), points_.cend(), view_.right(), [](double x, const QPointF &p) { return x < p.x(); });
        first = std::max(0, int(lower - points_.cbegin()) - 1); end = std::min(points_.size(), int(upper - points_.cbegin()) + 1);
    }
    for (int i = first; i < end;) {
        const int column = columnOf(screenPoint(points_[i]).x());
        int next = i + 1, minIndex = i, maxIndex = i;
        while (next < end && columnOf(screenPoint(points_[next]).x()) == column) {
            if (points_[next].y() < points_[minIndex].y()) minIndex = next;
            if (points_[next].y() > points_[maxIndex].y()) maxIndex = next;
            ++next;
        }
        std::array<int, 4> indices{{i, minIndex, maxIndex, next - 1}}; std::sort(indices.begin(), indices.end());
        int previous = -1;
        for (int index : indices) if (index != previous) {
            const auto point = screenPoint(points_[index]);
            if (!started) { path.moveTo(point); started = true; } else path.lineTo(point);
            previous = index;
        }
        i = next;
    }
    painter.setPen(QPen(colors.color(QPalette::Highlight), 1.8)); painter.drawPath(path);
    if (hovering_ && !dragging_ && area.contains(hover_)) {
        const double x = view_.left() + (hover_.x() - area.left()) / area.width() * view_.width();
        int nearest = 0;
        if (sorted_) {
            auto it = std::lower_bound(points_.cbegin(), points_.cend(), x, [](const QPointF &p, double value) { return p.x() < value; });
            nearest = std::min(points_.size() - 1, int(it - points_.cbegin()));
            if (nearest > 0 && std::abs(points_[nearest - 1].x() - x) < std::abs(points_[nearest].x() - x)) --nearest;
        } else for (int i = 1; i < points_.size(); ++i)
            if (std::abs(points_[i].x() - x) < std::abs(points_[nearest].x() - x)) nearest = i;
        const auto point = screenPoint(points_[nearest]);
        painter.setPen(QPen(muted, 1, Qt::DashLine));
        painter.drawLine(QPointF(point.x(), area.top()), QPointF(point.x(), area.bottom()));
        painter.setBrush(colors.color(QPalette::Highlight)); painter.drawEllipse(point, 3, 3);
        painter.restore(); painter.save();
        const QString text = QString::fromUtf8("x: %1   y: %2").arg(tickLabel(points_[nearest].x()), tickLabel(points_[nearest].y()));
        painter.setPen(colors.color(QPalette::Text)); painter.drawText(QRectF(20, 43, width() - 45, 20), Qt::AlignRight, text);
    }
    painter.restore();
}
void TrajectoryPlot::wheelEvent(QWheelEvent *e) {
    if (points_.isEmpty() || !plotRect().contains(e->pos())) return;
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
    if (!points_.isEmpty() && e->button() == Qt::LeftButton && plotRect().contains(e->pos())) {
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
