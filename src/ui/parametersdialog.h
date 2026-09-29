// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDialog>
#include "model.h"
class NumberEdit;
class QLabel;
class ParametersDialog : public QDialog {
    Q_OBJECT
public:
    explicit ParametersDialog(const ballistic::Parameters &p, QWidget *parent = nullptr);
    ballistic::Parameters parameters() const;
private:
    ballistic::Parameters original_;
    NumberEdit *payload_;
    NumberEdit *mass_[3], *fuel_[3], *thrust_[3], *exhaust_[3];
    QLabel *error_;
};
