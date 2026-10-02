// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QThread>
#include <atomic>
#include "solver.h"
#include "plotdata.h"

class CalculationTask : public QThread {
    Q_OBJECT
public:
    CalculationTask(ballistic::Parameters parameters, ballistic::Options options, QObject *parent = nullptr)
        : QThread(parent), parameters_(parameters), options_(options) {}
    void cancel() { cancelled_.store(true); }
    // Read only after QThread::finished; worker owns this data while running.
    ballistic::Result result;
    PlotData plots;
signals:
    void progress(int iteration, double altitudeError, double velocityError);
protected:
    void run() override {
        try {
            ballistic::Solver solver;
            result = solver.run(parameters_, options_, [this] { return cancelled_.load(); },
                [this](int i, double h, double v) { emit progress(i, h, v); });
            if (result.status == ballistic::Status::Completed) {
                plots = preparePlotData(result, [this] { return cancelled_.load(); });
                if (cancelled_.load()) {
                    result.status = ballistic::Status::Cancelled; result.message = "Расчёт отменён.";
                    result.diagnostics.reason = ballistic::StopReason::Cancelled; result.trajectory.clear(); plots = {};
                }
            }
        } catch (const std::exception &e) {
            result.status = ballistic::Status::NumericalFailure; result.message = e.what();
        } catch (...) {
            result.status = ballistic::Status::NumericalFailure; result.message = "Неизвестная ошибка расчёта.";
        }
        if (result.status == ballistic::Status::NumericalFailure) result.diagnostics.reason = ballistic::StopReason::NumericalFailure;
        if (result.status != ballistic::Status::Completed) { result.trajectory.clear(); plots = {}; }
    }
private:
    ballistic::Parameters parameters_;
    ballistic::Options options_;
    std::atomic<bool> cancelled_{false};
};
