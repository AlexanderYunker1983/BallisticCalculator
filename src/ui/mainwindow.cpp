// SPDX-License-Identifier: GPL-3.0-or-later
#include "mainwindow.h"
#include "numberedit.h"
#include "thememanager.h"
#include <QFrame>
#include <QScrollArea>
#include <QScreen>
#include <QShortcut>
#include "parametersdialog.h"
#include "calculationtask.h"
#include "trajectoryplot.h"
#include <QCheckBox>
#include <QCloseEvent>
#include <QDir>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QApplication>
#include <cmath>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(QString::fromUtf8("Баллистический калькулятор"));
    setMinimumSize(720, 440);
    const auto available = QGuiApplication::primaryScreen()->availableGeometry();
    resize(qMin(1360, available.width() - 48), qMin(900, available.height() - 64));
    auto *scroll = new QScrollArea; scroll->setFrameShape(QFrame::NoFrame); scroll->setWidgetResizable(true);
    setCentralWidget(scroll);
    auto *central = new QWidget; central->setObjectName("pageContent"); scroll->setWidget(central);
    auto *layout = new QVBoxLayout(central); layout->setContentsMargins(24, 20, 24, 16); layout->setSpacing(14);
    auto *title = new QLabel(QString::fromUtf8("Баллистический калькулятор"));
    title->setProperty("uiRole", "heading"); layout->addWidget(title);
    auto *subtitle = new QLabel(QString::fromUtf8("Траектория трёхступенчатой ракеты-носителя"));
    subtitle->setProperty("uiRole", "muted"); layout->addWidget(subtitle);

    auto *program = new QFrame; program->setProperty("uiRole", "panel");
    auto *programLayout = new QVBoxLayout(program); programLayout->setContentsMargins(18, 14, 18, 14); programLayout->setSpacing(12);
    auto *section = new QHBoxLayout;
    auto *heading = new QLabel(QString::fromUtf8("Программа полёта")); heading->setProperty("uiRole", "section");
    edit_ = new QPushButton(QString::fromUtf8("Параметры ракеты…")); edit_->setObjectName("parametersButton");
    edit_->setToolTip(QString::fromUtf8("Полезная нагрузка, массы ступеней, тяга и удельный импульс"));
    section->addWidget(heading); section->addStretch(); section->addWidget(edit_); programLayout->addLayout(section);
    inputs_ = new QWidget; auto *form = new QGridLayout(inputs_); form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(14); form->setVerticalSpacing(6);
    auto addInput = [&](const QString &text, const char *name, double max, double value, int column) {
        auto *edit = new NumberEdit; edit->setObjectName(name); edit->setMaximum(max); edit->setValue(value); edit->setMinimumWidth(115);
        auto *label = new QLabel(text); label->setProperty("uiRole", "muted"); label->setWordWrap(true); label->setBuddy(edit);
        edit->setAccessibleName(text);
        form->addWidget(label, 0, column); form->addWidget(edit, 1, column); form->setColumnStretch(column, 1); return edit;
    };
    vertical_ = addInput(QString::fromUtf8("Вертикальный участок, с"), "verticalTime", 86400, parameters_.verticalTime, 0);
    turnTime_ = addInput(QString::fromUtf8("Время поворота, с"), "turnTime", 86400, parameters_.turnTime, 1);
    angle_ = addInput(QString::fromUtf8("Угол поворота, °"), "turnAngle", 90, parameters_.turnDegrees, 2);
    target_ = addInput(QString::fromUtf8("Целевая высота, км"), "targetAltitude", 300, 250, 3);
    step_ = addInput(QString::fromUtf8("Шаг расчёта, с"), "maxStep", 1, 0.01, 4);
    vertical_->setToolTip(QString::fromUtf8("t₀ — длительность вертикального участка от старта"));
    turnTime_->setToolTip(QString::fromUtf8("t₁ — время выхода на заданный угол; должно быть больше t₀"));
    angle_->setToolTip(QString::fromUtf8("φ₁ — угол программы полёта в момент t₁"));
    step_->setToolTip(QString::fromUtf8("Максимальный шаг интегрирования: от 0,000001 до 1 с"));
    step_->setSingleStep(0.001); angle_->setSingleStep(0.1); programLayout->addWidget(inputs_);
    auto *actions = new QHBoxLayout;
    optimize_ = new QCheckBox(QString::fromUtf8("Подбирать угол и время поворота")); optimize_->setObjectName("optimize"); optimize_->setChecked(true);
    optimize_->setToolTip(QString::fromUtf8("Подобрать φ₁ и t₁ по конечной высоте и модулю скорости"));
    start_ = new QPushButton(QString::fromUtf8("Рассчитать")); start_->setObjectName("startButton"); start_->setProperty("uiRole", "primary");
    start_->setToolTip(QString::fromUtf8("Начать расчёт · Ctrl+Enter"));
    cancel_ = new QPushButton(QString::fromUtf8("Остановить")); cancel_->setObjectName("cancelButton");
    cancel_->setToolTip(QString::fromUtf8("Отменить текущий расчёт · Esc"));
    actions->addWidget(optimize_); actions->addStretch(); actions->addWidget(cancel_); actions->addWidget(start_);
    programLayout->addLayout(actions); layout->addWidget(program);

    auto *summary = new QHBoxLayout; summary->setSpacing(12);
    auto card = [&](const QString &caption, const char *name, int stretch) {
        auto *box = new QFrame; box->setProperty("uiRole", "panel");
        auto *v = new QVBoxLayout(box); v->setContentsMargins(16, 12, 16, 12); v->setSpacing(6);
        auto *label = new QLabel(caption); label->setProperty("uiRole", "muted"); v->addWidget(label);
        auto *value = new QLabel(QString::fromUtf8("—")); value->setObjectName(name); value->setProperty("uiRole", "metric");
        value->setTextInteractionFlags(Qt::TextSelectableByMouse); v->addWidget(value); summary->addWidget(box, stretch); return value;
    };
    altitude_ = card(QString::fromUtf8("Конечная высота"), "altitudeResult", 1);
    velocity_ = card(QString::fromUtf8("Скорость / целевая"), "velocityResult", 2);
    details_ = card(QString::fromUtf8("Показанный расчёт"), "calculationDetails", 1);
    layout->addLayout(summary);

    auto *chartTools = new QHBoxLayout;
    auto *chartHeading = new QLabel(QString::fromUtf8("Результаты полёта")); chartHeading->setProperty("uiRole", "section");
    auto *reset = new QPushButton(QString::fromUtf8("Сбросить масштаб")); reset->setObjectName("resetZoomButton");
    chartTools->addWidget(chartHeading); chartTools->addStretch(); chartTools->addWidget(reset); layout->addLayout(chartTools);
    tabs_ = new QTabWidget; tabs_->setObjectName("plotTabs"); tabs_->setUsesScrollButtons(true); layout->addWidget(tabs_, 1);
    const QStringList titles{QString::fromUtf8("Скорость"), QString::fromUtf8("Высота"), QString::fromUtf8("Угол атаки"),
        QString::fromUtf8("Программа полёта"), QString::fromUtf8("Перегрузка"), QString::fromUtf8("Ортодромная дальность"), QString::fromUtf8("Профиль высоты")};
    const QStringList units{QString::fromUtf8("V, м/с"), QString::fromUtf8("H, км"), QString::fromUtf8("α, °"), QString::fromUtf8("φ, °"),
        QString::fromUtf8("n, g"), QString::fromUtf8("S, км"), QString::fromUtf8("H, км")};
    for (int i = 0; i < 7; ++i) {
        plots_[i] = new TrajectoryPlot(titles[i], i == 6 ? QString::fromUtf8("S, км") : QString::fromUtf8("t, с"), units[i]);
        plots_[i]->setObjectName(QString::fromUtf8("plot%1").arg(i));
    }
    auto addTab = [&](const QString &name, int first, int second) {
        auto *page = new QWidget; auto *row = new QHBoxLayout(page); row->setContentsMargins(1, 1, 1, 1);
        row->setSpacing(1); row->addWidget(plots_[first]); if (second >= 0) row->addWidget(plots_[second]); tabs_->addTab(page, name);
    };
    addTab(QString::fromUtf8("Скорость и высота"), 0, 1); addTab(QString::fromUtf8("Углы"), 2, 3);
    addTab(QString::fromUtf8("Перегрузка"), 4, -1); addTab(QString::fromUtf8("Дальность"), 5, -1); addTab(QString::fromUtf8("Профиль высоты"), 6, -1);
    progress_ = new QProgressBar; progress_->setRange(0, 0); progress_->setTextVisible(false); progress_->setFixedHeight(3);
    auto progressPolicy = progress_->sizePolicy(); progressPolicy.setRetainSizeWhenHidden(true); progress_->setSizePolicy(progressPolicy); layout->addWidget(progress_);
    status_ = new QLabel(QString::fromUtf8("Готов к расчёту. Задайте параметры и нажмите «Рассчитать»."));
    status_->setObjectName("statusMessage"); status_->setProperty("tone", "normal"); status_->setWordWrap(true); layout->addWidget(status_);
    auto *hint = new QLabel(QString::fromUtf8("Колесо — масштаб графика  ·  Перетаскивание — сдвиг  ·  Двойной щелчок — весь график"));
    hint->setProperty("uiRole", "muted"); hint->setWordWrap(true); layout->addWidget(hint);
    connect(start_, &QPushButton::clicked, this, &MainWindow::startCalculation);
    connect(cancel_, &QPushButton::clicked, this, &MainWindow::cancelCalculation);
    connect(new QShortcut(QKeySequence("Ctrl+Return"), this), &QShortcut::activated, this, &MainWindow::startCalculation);
    connect(new QShortcut(QKeySequence("Ctrl+Enter"), this), &QShortcut::activated, this, &MainWindow::startCalculation);
    connect(new QShortcut(QKeySequence(Qt::Key_Escape), this), &QShortcut::activated, this, &MainWindow::cancelCalculation);
    connect(reset, &QPushButton::clicked, this, [this] { for (auto *plot : plots_) plot->resetView(); });
    connect(edit_, &QPushButton::clicked, this, [this] {
        ParametersDialog dialog(inputParameters(), this);
        if (dialog.exec() == QDialog::Accepted) parameters_ = dialog.parameters();
    });
    QWidget::setTabOrder(edit_, vertical_); QWidget::setTabOrder(vertical_, turnTime_);
    QWidget::setTabOrder(turnTime_, angle_); QWidget::setTabOrder(angle_, target_);
    QWidget::setTabOrder(target_, step_); QWidget::setTabOrder(step_, optimize_);
    QWidget::setTabOrder(optimize_, start_); QWidget::setTabOrder(start_, cancel_);
    setBusy(false);
}
MainWindow::~MainWindow() {
    if (task_) { task_->cancel(); task_->wait(); }
}
ballistic::Parameters MainWindow::inputParameters() const {
    auto p = parameters_; p.verticalTime = vertical_->value(); p.turnTime = turnTime_->value(); p.turnDegrees = angle_->value(); return p;
}
void MainWindow::setBusy(bool value) {
    start_->setEnabled(!value); cancel_->setEnabled(value); edit_->setEnabled(!value);
    optimize_->setEnabled(!value); inputs_->setEnabled(!value); progress_->setVisible(value);
}
void MainWindow::startCalculation() {
    if (task_) return;
    const auto p = inputParameters();
    ballistic::Options o; o.maxStep = step_->value(); o.targetAltitude = target_->value() * 1000; o.optimize = optimize_->isChecked();
    const auto error = ballistic::validate(p, o);
    status_->setProperty("tone", error.empty() ? "normal" : "error"); ThemeManager::repolish(status_);
    if (!error.empty()) { status_->setText(QString::fromUtf8(error.c_str())); emit calculationFinished(false); return; }
    status_->setText(QString::fromUtf8("Выполняется расчёт…")); setBusy(true);
    task_ = new CalculationTask(p, o, this);
    connect(task_, &CalculationTask::progress, this, [this](int iteration, double h, double v) {
        if (!cancel_->isEnabled()) return;
        status_->setText(QString::fromUtf8("Подбор: итерация %1 · ошибка высоты %2 м · ошибка скорости %3 м/с")
            .arg(iteration).arg(QLocale().toString(h, 'f', 2)).arg(QLocale().toString(v, 'f', 3)));
    });
    connect(task_, &QThread::finished, this, &MainWindow::finishCalculation);
    task_->start();
}
void MainWindow::cancelCalculation() {
    if (!task_) return;
    task_->cancel(); cancel_->setEnabled(false); status_->setText(QString::fromUtf8("Отмена расчёта…"));
}
void MainWindow::finishCalculation() {
    auto *completed = task_;
    if (!completed) return;
    task_ = nullptr;
    const bool success = completed->result.status == ballistic::Status::Completed;
    const bool cancelled = completed->result.status == ballistic::Status::Cancelled;
    status_->setText(QString::fromUtf8(completed->result.message.c_str()));
    status_->setProperty("tone", success || cancelled ? "normal" : "error"); ThemeManager::repolish(status_);
    if (success) { result_ = std::move(completed->result); applyResult(); }
    completed->deleteLater(); setBusy(false);
    emit calculationFinished(success);
    if (closing_) QTimer::singleShot(0, this, &QWidget::close);
}
void MainWindow::applyResult() {
    parameters_ = result_.parameters;
    turnTime_->setValue(parameters_.turnTime); angle_->setValue(parameters_.turnDegrees);
    const auto &last = result_.trajectory.back();
    const auto locale = QLocale();
    altitude_->setText(locale.toString((last.radius - ballistic::EarthRadius) / 1000, 'f', 3) + QString::fromUtf8(" км"));
    velocity_->setText(QString::fromUtf8("%1 / %2 м/с").arg(locale.toString(last.velocity, 'f', 3),
        locale.toString(ballistic::orbitalSpeed(result_.options.targetAltitude), 'f', 3)));
    details_->setText(QString::fromUtf8("%1 точек · %2 с").arg(locale.toString(static_cast<qulonglong>(result_.trajectory.size())),
        locale.toString(result_.elapsedSeconds, 'f', 2)));
    QVector<QPointF> data[7]; for (auto &series : data) series.reserve(int(result_.trajectory.size()));
    for (const auto &s : result_.trajectory) {
        const double h = (s.radius - ballistic::EarthRadius) / 1000, range = ballistic::EarthRadius * s.arc / 1000;
        data[0].append({s.time, s.velocity}); data[1].append({s.time, h});
        data[2].append({s.time, s.alpha * 180 / ballistic::Pi}); data[3].append({s.time, s.phi * 180 / ballistic::Pi});
        data[4].append({s.time, s.overload}); data[5].append({s.time, range}); data[6].append({range, h});
    }
    QVector<double> events; for (double t : parameters_.separationTimes()) events.append(t);
    for (int i = 0; i < 7; ++i) plots_[i]->setData(std::move(data[i]), i == 6 ? QVector<double>{} : events);
    if (result_.atmosphereClamped)
        status_->setText(status_->text() + QString::fromUtf8(" На участке выше 300 км использована граница атмосферной таблицы."));
}
void MainWindow::closeEvent(QCloseEvent *event) {
    if (task_) { closing_ = true; cancelCalculation(); event->ignore(); }
    else QMainWindow::closeEvent(event);
}
bool MainWindow::savePlotScreenshots(const QString &directory) {
    if (result_.trajectory.empty() || !QDir().mkpath(directory)) return false;
    const int old = tabs_->currentIndex(); bool ok = true;
    for (int i = 0; i < tabs_->count(); ++i) {
        tabs_->setCurrentIndex(i); QApplication::processEvents();
        ok = grab().save(QDir(directory).filePath(QString::fromUtf8("tab-%1.png").arg(i + 1))) && ok;
    }
    tabs_->setCurrentIndex(old); return ok;
}
