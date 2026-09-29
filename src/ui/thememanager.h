// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QPalette>
#include <QTimer>
#include <functional>

class QApplication;
class QWidget;

class ThemeManager : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    enum class Mode { System, Light, Dark };
    struct SystemTheme {
        bool dark = false;
        bool highContrast = false;
        QPalette palette;
    };
    using SystemReader = std::function<SystemTheme()>;
    explicit ThemeManager(QApplication &application, SystemReader reader = {});
    ~ThemeManager() override;
    void setMode(Mode mode);
    bool isDark() const { return dark_; }
    bool nativeEventFilter(const QByteArray &, void *message, long *) override;
    static SystemTheme readSystemTheme();
    static void repolish(QWidget *widget);
public slots:
    void refresh();
signals:
    void themeChanged(bool dark);
protected:
    bool eventFilter(QObject *object, QEvent *event) override;
private:
    QApplication &application_;
    SystemReader reader_;
    QTimer poll_;
    Mode mode_ = Mode::System;
    QPalette appliedPalette_;
    bool initialized_ = false, dark_ = false, highContrast_ = false, refreshPending_ = false;
    void scheduleRefresh();
    void updateCaption(QWidget *window);
};
