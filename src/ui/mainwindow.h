// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QMainWindow>
#include "model.h"
class NumberEdit;
class QPushButton;
class QLabel;
class QCheckBox;
class QProgressBar;
class QTabWidget;
class CalculationTask;
class TrajectoryPlot;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    bool busy() const { return task_ != nullptr; }
    const ballistic::Result &lastResult() const { return result_; }
    ballistic::Parameters inputParameters() const;
    bool savePlotScreenshots(const QString &directory);
public slots:
    void startCalculation();
    void cancelCalculation();
signals:
    void calculationFinished(bool success);
protected:
    void closeEvent(QCloseEvent *) override;
private:
    ballistic::Parameters parameters_;
    ballistic::Result result_;
    CalculationTask *task_ = nullptr;
    bool closing_ = false;
    NumberEdit *vertical_, *turnTime_, *angle_, *target_, *step_;
    QPushButton *start_, *cancel_, *edit_;
    QCheckBox *optimize_;
    QProgressBar *progress_;
    QLabel *status_, *altitude_, *velocity_, *details_;
    QWidget *inputs_;
    QTabWidget *tabs_;
    TrajectoryPlot *plots_[7];
    void setBusy(bool busy);
    void applyResult();
    void finishCalculation();
};
