// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QThread>
#include <atomic>
#include "solver.h"

class CalculationTask : public QThread {
    Q_OBJECT
public:
    CalculationTask(ballistic::Parameters parameters, ballistic::Options options, QObject *parent = nullptr)
        : QThread(parent), parameters_(parameters), options_(options) {}
    void cancel() { cancelled_.store(true); }
    // Read only after QThread::finished; worker owns this data while running.
    ballistic::Result result;
signals:
    void progress(int iteration, double altitudeError, double velocityError);
protected:
    void run() override {
        try {
            ballistic::Solver solver;
            result = solver.run(parameters_, options_, [this] { return cancelled_.load(); },
                [this](int i, double h, double v) { emit progress(i, h, v); });
        } catch (const std::exception &e) {
            result.status = ballistic::Status::NumericalFailure; result.message = e.what();
        } catch (...) {
            result.status = ballistic::Status::NumericalFailure; result.message = "Неизвестная ошибка расчёта.";
        }
    }
private:
    ballistic::Parameters parameters_;
    ballistic::Options options_;
    std::atomic<bool> cancelled_{false};
};
