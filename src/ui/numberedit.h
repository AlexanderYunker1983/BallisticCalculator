// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDoubleSpinBox>
#include <QWheelEvent>
#include <cmath>

// Numeric model values never depend on a formatted string. Both decimal marks
// are accepted, without interpreting either as a thousands separator.
class NumberEdit : public QDoubleSpinBox {
public:
    explicit NumberEdit(QWidget *parent = nullptr) : QDoubleSpinBox(parent) {
        auto numericLocale = QLocale::system();
        numericLocale.setNumberOptions(QLocale::RejectGroupSeparator | QLocale::OmitGroupSeparator);
        setLocale(numericLocale); setDecimals(6); setKeyboardTracking(false);
        setRange(0, 1e12); setGroupSeparatorShown(false);
        setButtonSymbols(QAbstractSpinBox::NoButtons);
        setAlignment(Qt::AlignRight); setFocusPolicy(Qt::StrongFocus);
        setMinimumHeight(36);
    }
    bool commitInput() {
        if (!hasAcceptableInput()) return false;
        interpretText();
        return true;
    }
protected:
    void wheelEvent(QWheelEvent *event) override {
        if (hasFocus()) QDoubleSpinBox::wheelEvent(event);
        else event->ignore();
    }
    QString textFromValue(double value) const override {
        QString text = locale().toString(value, 'f', decimals());
        if (text.contains(locale().decimalPoint())) {
            while (text.endsWith('0')) text.chop(1);
            if (text.endsWith(locale().decimalPoint())) text.chop(1);
        }
        return text;
    }
    QValidator::State validate(QString &text, int &pos) const override {
        if (text.contains('.') && text.contains(',')) return QValidator::Invalid;
        QString normalized = normalize(text);
        return QDoubleSpinBox::validate(normalized, pos);
    }
    double valueFromText(const QString &text) const override {
        bool ok = false;
        const double value = locale().toDouble(normalize(text), &ok);
        return ok && std::isfinite(value) ? value : QDoubleSpinBox::valueFromText(text);
    }
private:
    QString normalize(QString text) const {
        text.replace('.', locale().decimalPoint());
        text.replace(',', locale().decimalPoint());
        return text;
    }
};
