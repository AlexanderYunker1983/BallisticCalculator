// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include "plotdata.h"
class CalculationTask;
class CalculationController : public QObject {
    Q_OBJECT
public:
    explicit CalculationController(QObject *parent = nullptr) : QObject(parent) {}
    ~CalculationController() override;
    bool busy() const { return task_ != nullptr; }
    bool start(const ballistic::Parameters &p, const ballistic::Options &o);
    void cancel();
    ballistic::Result takeResult() { return std::move(result_); }
    PlotData takePlots() { return std::move(plots_); }
signals:
    void progress(int iteration, double altitudeError, double velocityError);
    void finished();
private:
    CalculationTask *task_ = nullptr;
    ballistic::Result result_;
    PlotData plots_;
};
