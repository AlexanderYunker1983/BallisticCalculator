// SPDX-License-Identifier: GPL-3.0-or-later
#include "parametersdialog.h"
#include "numberedit.h"
#include "thememanager.h"
#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QVBoxLayout>
#include <cmath>

ParametersDialog::ParametersDialog(const ballistic::Parameters &p, QWidget *parent)
    : QDialog(parent), original_(p) {
    setWindowTitle(QString::fromUtf8("Начальные условия")); setObjectName("parametersDialog");
    setWindowFlag(Qt::WindowContextHelpButtonHint, false); setSizeGripEnabled(true); setMinimumSize(640, 420);
    auto *layout = new QVBoxLayout(this); layout->setContentsMargins(24, 20, 24, 18); layout->setSpacing(12);
    auto *heading = new QLabel(QString::fromUtf8("Параметры ракеты-носителя"));
    heading->setProperty("uiRole", "heading"); layout->addWidget(heading);
    auto *note = new QLabel(QString::fromUtf8("Полезная нагрузка и характеристики трёх ступеней"));
    note->setProperty("uiRole", "muted"); note->setWordWrap(true); layout->addWidget(note);
    auto *scroll = new QScrollArea; scroll->setFrameShape(QFrame::NoFrame); scroll->setWidgetResizable(true);
    auto *body = new QWidget; scroll->setWidget(body); layout->addWidget(scroll, 1);
    auto *bodyLayout = new QVBoxLayout(body); bodyLayout->setContentsMargins(0, 0, 0, 0); bodyLayout->setSpacing(12);
    auto *payloadCard = new QFrame; payloadCard->setProperty("uiRole", "panel");
    auto *payloadRow = new QHBoxLayout(payloadCard); payloadRow->setContentsMargins(16, 12, 16, 12);
    auto *payloadLabel = new QLabel(QString::fromUtf8("Полезная нагрузка, кг")); payloadLabel->setProperty("uiRole", "section");
    payload_ = new NumberEdit; payload_->setObjectName("payload"); payload_->setValue(p.payload); payload_->setSingleStep(100);
    payload_->setMaximumWidth(200); payloadLabel->setBuddy(payload_); payload_->setAccessibleName(payloadLabel->text());
    payloadRow->addWidget(payloadLabel); payloadRow->addStretch(); payloadRow->addWidget(payload_); bodyLayout->addWidget(payloadCard);
    auto *stages = new QFrame; stages->setProperty("uiRole", "panel");
    auto *grid = new QGridLayout(stages); grid->setContentsMargins(16, 14, 16, 16); grid->setHorizontalSpacing(14); grid->setVerticalSpacing(12);
    const QStringList labels{QString::fromUtf8("Полная масса, кг"), QString::fromUtf8("Топливо, кг"),
                             QString::fromUtf8("Тяга, Н"), QString::fromUtf8("Удельный импульс, м/с")};
    for (int r = 0; r < labels.size(); ++r) {
        auto *label = new QLabel(labels[r]); label->setProperty("uiRole", "muted");
        if (r == 3) label->setObjectName("specificImpulseLabel");
        grid->addWidget(label, r + 1, 0);
    }
    for (int i = 0; i < 3; ++i) {
        auto *label = new QLabel(QString::fromUtf8("Ступень %1").arg(i + 1));
        label->setProperty("uiRole", "section"); label->setAlignment(Qt::AlignCenter); grid->addWidget(label, 0, i + 1);
        grid->setColumnStretch(i + 1, 1);
        NumberEdit **rows[] = {mass_, fuel_, thrust_, exhaust_};
        const double values[] = {p.mass[i], p.fuel[i], p.thrust[i], p.exhaustVelocity[i]};
        const double steps[] = {100, 100, 1000, 1};
        const char *names[] = {"mass", "fuel", "thrust", "exhaust"};
        for (int r = 0; r < 4; ++r) {
            auto *field = new NumberEdit; rows[r][i] = field;
            field->setObjectName(QString::fromLatin1(names[r]) + QString::number(i + 1));
            field->setValue(values[r]); field->setSingleStep(steps[r]); field->setMinimumWidth(120);
            field->setAccessibleName(QString::fromUtf8("Ступень %1. %2").arg(i + 1).arg(labels[r]));
            field->setToolTip(r == 3
                ? QString::fromUtf8("Удельный импульс в м/с (Н·с/кг). Для значения в секундах используйте Isp × 9,80665.")
                : labels[r]);
            grid->addWidget(field, r + 1, i + 1);
        }
    }
    bodyLayout->addWidget(stages);
    auto *massNote = new QLabel(QString::fromUtf8("Полная масса каждой ступени включает её топливо."));
    massNote->setProperty("uiRole", "muted"); massNote->setWordWrap(true); bodyLayout->addWidget(massNote);
    auto *summary = new QLabel; summary->setObjectName("vehicleSummary"); summary->setProperty("uiRole", "section");
    summary->setWordWrap(true); bodyLayout->addWidget(summary); bodyLayout->addStretch();
    auto updateSummary = [this, summary] {
        const auto vehicle = parameters();
        const double fuel = vehicle.fuel[0] + vehicle.fuel[1] + vehicle.fuel[2];
        summary->setText(QString::fromUtf8("Стартовая масса: %1 кг   ·   Топливо: %2 кг")
            .arg(QLocale().toString(vehicle.totalMass(), 'f', 0), QLocale().toString(fuel, 'f', 0)));
    };
    error_ = new QLabel; error_->setObjectName("validationError"); error_->setWordWrap(true);
    error_->setProperty("tone", "error"); error_->setMinimumHeight(fontMetrics().height()); layout->addWidget(error_);
    const auto fields = body->findChildren<NumberEdit *>();
    for (auto *field : fields) connect(field, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, field, updateSummary] {
        field->setProperty("invalid", false); ThemeManager::repolish(field); error_->clear(); updateSummary();
    });
    updateSummary();
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(QString::fromUtf8("Применить"));
    buttons->button(QDialogButtonBox::Ok)->setProperty("uiRole", "primary");
    buttons->button(QDialogButtonBox::Ok)->setDefault(true);
    buttons->button(QDialogButtonBox::Cancel)->setText(QString::fromUtf8("Отмена")); layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [this, fields, scroll] {
        for (auto *field : fields) if (!field->commitInput()) {
            field->setProperty("invalid", true); ThemeManager::repolish(field);
            error_->setText(QString::fromUtf8("Завершите ввод числа в поле «%1».").arg(field->accessibleName()));
            field->setFocus(); scroll->ensureWidgetVisible(field); return;
        }
        NumberEdit *firstInvalid = nullptr;
        for (auto *field : fields) {
            const bool invalid = !(field->value() > 0) || !std::isfinite(field->value());
            field->setProperty("invalid", invalid); ThemeManager::repolish(field);
            if (invalid && !firstInvalid) firstInvalid = field;
        }
        for (int i = 0; i < 3; ++i) if (fuel_[i]->value() >= mass_[i]->value()) {
            fuel_[i]->setProperty("invalid", true); ThemeManager::repolish(fuel_[i]);
            if (!firstInvalid) firstInvalid = fuel_[i];
        }
        // Only vehicle data is edited here. Program times are checked at launch.
        auto vehicle = parameters(); vehicle.verticalTime = 0; vehicle.turnTime = vehicle.duration() / 2;
        const auto error = ballistic::validate(vehicle, ballistic::Options{});
        error_->setText(QString::fromUtf8(error.c_str()));
        if (firstInvalid) { firstInvalid->setFocus(); firstInvalid->selectAll(); scroll->ensureWidgetVisible(firstInvalid); }
        if (error.empty()) accept();
    });
    QWidget *previous = payload_;
    for (int i = 0; i < 3; ++i) for (auto *field : {mass_[i], fuel_[i], thrust_[i], exhaust_[i]}) {
        QWidget::setTabOrder(previous, field); previous = field;
    }
    QWidget::setTabOrder(previous, buttons->button(QDialogButtonBox::Ok));
    QWidget::setTabOrder(buttons->button(QDialogButtonBox::Ok), buttons->button(QDialogButtonBox::Cancel));
    payload_->setFocus();
    const auto available = QGuiApplication::primaryScreen()->availableGeometry();
    resize(qMin(900, available.width() - 48), qMin(610, available.height() - 64));
}
ballistic::Parameters ParametersDialog::parameters() const {
    auto p = original_; p.payload = payload_->value();
    for (int i = 0; i < 3; ++i) {
        p.mass[i] = mass_[i]->value(); p.fuel[i] = fuel_[i]->value();
        p.thrust[i] = thrust_[i]->value(); p.exhaustVelocity[i] = exhaust_[i]->value();
    }
    return p;
}
