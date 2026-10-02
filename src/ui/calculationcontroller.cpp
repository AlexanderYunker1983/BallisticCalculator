// SPDX-License-Identifier: GPL-3.0-or-later
#include "calculationcontroller.h"
#include "calculationtask.h"
CalculationController::~CalculationController() {
    if (task_) { task_->cancel(); task_->wait(); }
}
bool CalculationController::start(const ballistic::Parameters &p, const ballistic::Options &o) {
    if (task_) return false;
    task_ = new CalculationTask(p, o, this);
    connect(task_, &CalculationTask::progress, this, &CalculationController::progress);
    connect(task_, &QThread::finished, this, [this] {
        auto *completed = task_;
        result_ = std::move(completed->result); plots_ = std::move(completed->plots);
        task_ = nullptr; completed->deleteLater(); emit finished();
    });
    task_->start(); return true;
}
void CalculationController::cancel() { if (task_) task_->cancel(); }
