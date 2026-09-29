// SPDX-License-Identifier: GPL-3.0-or-later
#include "thememanager.h"
#include <QApplication>
#include <QEvent>
#include <QPointer>
#include <QSettings>
#include <QStyle>
#include <QStyleFactory>
#include <QWidget>
#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#endif

namespace {
QPalette makePalette(bool dark) {
    QPalette p;
    const QColor text(dark ? "#e7edf7" : "#172638");
    const QColor surface(dark ? "#172130" : "#ffffff");
    const QColor window(dark ? "#101722" : "#f3f6fa");
    const QColor muted(dark ? "#a6b5cb" : "#607086");
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::Base, surface);
    p.setColor(QPalette::AlternateBase, QColor(dark ? "#233044" : "#eaf0f7"));
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, surface);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::Highlight, QColor(dark ? "#79adff" : "#245fca"));
    p.setColor(QPalette::HighlightedText, QColor(dark ? "#0c203c" : "#ffffff"));
    p.setColor(QPalette::BrightText, QColor(dark ? "#ffaba8" : "#b42332"));
    p.setColor(QPalette::Link, p.color(QPalette::Highlight));
    p.setColor(QPalette::LinkVisited, p.color(QPalette::Highlight));
    p.setColor(QPalette::Mid, QColor(dark ? "#35465e" : "#cbd5e3"));
    p.setColor(QPalette::Light, QColor(dark ? "#42536b" : "#ffffff"));
    p.setColor(QPalette::Midlight, p.color(QPalette::AlternateBase));
    p.setColor(QPalette::Dark, QColor(dark ? "#0a1019" : "#a0aec0"));
    p.setColor(QPalette::Shadow, QColor(dark ? "#000000" : "#697586"));
    p.setColor(QPalette::ToolTipBase, surface);
    p.setColor(QPalette::ToolTipText, text);
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, role, muted);
    p.setColor(QPalette::Disabled, QPalette::Highlight, p.color(QPalette::AlternateBase));
    p.setColor(QPalette::Disabled, QPalette::HighlightedText, muted);
    return p;
}
QString styleSheet(const QPalette &p) {
    QString css = QString::fromUtf8(R"(
QMainWindow, QDialog, QScrollArea, QWidget#pageContent { background: @window; }
QWidget { color: @text; }
QWidget:disabled { color: @muted; }
QLabel { background: transparent; border: none; }
QFrame[uiRole="panel"] { background: @surface; border: 1px solid @border; border-radius: 10px; }
QLabel[uiRole="heading"] { font-size: 23px; font-weight: 600; }
QLabel[uiRole="section"] { font-size: 14px; font-weight: 600; }
QLabel[uiRole="muted"] { color: @muted; }
QLabel[uiRole="metric"] { color: @accent; font-size: 20px; font-weight: 600; }
QLabel[tone="error"] { color: @danger; }
QLabel[tone="normal"] { color: @muted; }
QPushButton { background: @surface; border: 1px solid @border; border-radius: 6px; padding: 8px 16px; min-height: 18px; }
QPushButton:hover { background: @raised; border-color: @accent; }
QPushButton:pressed { background: @raised; }
QPushButton:focus { border: 2px solid @accent; padding: 7px 15px; }
QPushButton[uiRole="primary"] { background: @accent; color: @onAccent; border-color: @accent; font-weight: 600; }
QPushButton[uiRole="primary"]:hover { background: @accentHover; border-color: @accentHover; }
QPushButton[uiRole="primary"]:focus { border: 2px solid @text; }
QPushButton:disabled, QPushButton[uiRole="primary"]:disabled { background: @raised; color: @muted; border-color: @border; }
QDoubleSpinBox { background: @surface; color: @text; border: 1px solid @border; border-radius: 6px; padding: 8px 10px; min-height: 18px; selection-background-color: @accent; selection-color: @onAccent; }
QDoubleSpinBox:hover { border-color: @muted; }
QDoubleSpinBox:focus { border: 2px solid @accent; padding: 7px 9px; }
QDoubleSpinBox[invalid="true"] { border-color: @danger; }
QDoubleSpinBox:disabled { color: @muted; background: @raised; }
QCheckBox { spacing: 8px; background: transparent; }
QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid @muted; border-radius: 4px; background: @surface; }
QCheckBox::indicator:checked { background: @accent; border-color: @accent; image: url(@checkIcon); }
QCheckBox::indicator:disabled { border-color: @border; background: @raised; }
QCheckBox:focus { color: @accent; }
QTabWidget::pane { background: @surface; border: 1px solid @border; border-radius: 8px; top: -1px; }
QTabBar { font-weight: 600; }
QTabBar::tab { background: @window; color: @muted; border-bottom: 3px solid transparent; padding: 10px 16px; margin-right: 4px; }
QTabBar::tab:hover { background: @raised; color: @text; }
QTabBar::tab:selected { color: @accent; border-bottom-color: @accent; }
QProgressBar { border: none; background: @raised; height: 3px; }
QProgressBar::chunk { background: @accent; }
QToolTip { background: @surface; color: @text; border: 1px solid @border; padding: 6px; }
)");
    css.replace("@window", p.color(QPalette::Window).name());
    css.replace("@surface", p.color(QPalette::Base).name());
    css.replace("@raised", p.color(QPalette::AlternateBase).name());
    css.replace("@text", p.color(QPalette::WindowText).name());
    css.replace("@muted", p.color(QPalette::Disabled, QPalette::WindowText).name());
    css.replace("@border", p.color(QPalette::Mid).name());
    css.replace("@danger", p.color(QPalette::BrightText).name());
    css.replace("@onAccent", p.color(QPalette::HighlightedText).name());
    css.replace("@checkIcon", p.color(QPalette::HighlightedText).lightness() < 128
        ? ":/ui/check-dark.svg" : ":/ui/check-light.svg");
    // Replace the longer token first.
    css.replace("@accentHover", p.color(QPalette::Highlight).lighter(110).name());
    css.replace("@accent", p.color(QPalette::Highlight).name());
    return css;
}
}

ThemeManager::ThemeManager(QApplication &app, SystemReader reader)
    : QObject(&app), application_(app), reader_(reader ? std::move(reader) : readSystemTheme) {
    application_.setStyle(QStyleFactory::create("Fusion"));
    application_.installEventFilter(this);
    application_.installNativeEventFilter(this);
    connect(&poll_, &QTimer::timeout, this, &ThemeManager::refresh);
    poll_.setInterval(1000); poll_.setTimerType(Qt::CoarseTimer); poll_.start();
    refresh();
}
ThemeManager::~ThemeManager() {
    application_.removeNativeEventFilter(this);
    application_.removeEventFilter(this);
}
ThemeManager::SystemTheme ThemeManager::readSystemTheme() {
    SystemTheme result;
#ifdef Q_OS_WIN
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", QSettings::NativeFormat);
    result.dark = settings.value("AppsUseLightTheme", 1).toInt() == 0;
    HIGHCONTRASTW contrast{}; contrast.cbSize = sizeof(contrast);
    result.highContrast = SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0)
        && (contrast.dwFlags & HCF_HIGHCONTRASTON);
    if (result.highContrast) {
        auto color = [](int role) { const auto c = GetSysColor(role); return QColor(GetRValue(c), GetGValue(c), GetBValue(c)); };
        auto &p = result.palette;
        p.setColor(QPalette::Window, color(COLOR_WINDOW)); p.setColor(QPalette::Base, color(COLOR_WINDOW));
        p.setColor(QPalette::AlternateBase, color(COLOR_BTNFACE)); p.setColor(QPalette::Button, color(COLOR_BTNFACE));
        p.setColor(QPalette::Text, color(COLOR_WINDOWTEXT)); p.setColor(QPalette::WindowText, color(COLOR_WINDOWTEXT));
        p.setColor(QPalette::ButtonText, color(COLOR_BTNTEXT)); p.setColor(QPalette::Mid, color(COLOR_WINDOWTEXT));
        p.setColor(QPalette::BrightText, color(COLOR_WINDOWTEXT)); p.setColor(QPalette::Highlight, color(COLOR_HIGHLIGHT));
        p.setColor(QPalette::HighlightedText, color(COLOR_HIGHLIGHTTEXT));
        p.setColor(QPalette::ToolTipBase, color(COLOR_WINDOW)); p.setColor(QPalette::ToolTipText, color(COLOR_WINDOWTEXT));
        p.setColor(QPalette::Disabled, QPalette::WindowText, color(COLOR_GRAYTEXT));
        result.dark = p.color(QPalette::Window).lightness() < 128;
    }
#endif
    return result;
}
void ThemeManager::setMode(Mode mode) { mode_ = mode; refresh(); }
void ThemeManager::refresh() {
    const auto system = reader_();
    const bool dark = system.highContrast ? system.dark : mode_ == Mode::System ? system.dark : mode_ == Mode::Dark;
    const auto p = system.highContrast ? system.palette : makePalette(dark);
    if (initialized_ && p == appliedPalette_ && system.highContrast == highContrast_) return;
    initialized_ = true; dark_ = dark; highContrast_ = system.highContrast; appliedPalette_ = p;
    application_.setPalette(p);
    application_.setStyleSheet(styleSheet(p));
    for (auto *widget : application_.allWidgets()) {
        widget->update();
        if (widget->isWindow()) updateCaption(widget);
    }
    emit themeChanged(dark_);
}
void ThemeManager::scheduleRefresh() {
    if (refreshPending_) return;
    refreshPending_ = true;
    QTimer::singleShot(0, this, [this] { refreshPending_ = false; refresh(); });
}
bool ThemeManager::nativeEventFilter(const QByteArray &eventType, void *message, long *) {
#ifdef Q_OS_WIN
    if (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG") return false;
    const auto *event = static_cast<MSG *>(message);
    if (event && (event->message == WM_SETTINGCHANGE || event->message == WM_THEMECHANGED || event->message == WM_SYSCOLORCHANGE))
        scheduleRefresh();
#else
    Q_UNUSED(eventType)
    Q_UNUSED(message)
#endif
    return false;
}
bool ThemeManager::eventFilter(QObject *object, QEvent *event) {
    if (event->type() == QEvent::ApplicationActivate) scheduleRefresh();
    if (event->type() == QEvent::Show || event->type() == QEvent::WinIdChange) {
        auto *widget = qobject_cast<QWidget *>(object);
        if (widget && widget->isWindow()) {
            const QPointer<QWidget> window(widget);
            QTimer::singleShot(0, this, [this, window] { if (window) updateCaption(window); });
        }
    }
    return false;
}
void ThemeManager::updateCaption(QWidget *window) {
#ifdef Q_OS_WIN
    if (!window->testAttribute(Qt::WA_WState_Created)) return;
    const BOOL enabled = dark_ && !highContrast_;
    // Attribute 20 is documented by Microsoft; older Windows simply rejects it.
    DwmSetWindowAttribute(reinterpret_cast<HWND>(window->winId()), 20, &enabled, sizeof(enabled));
#else
    Q_UNUSED(window)
#endif
}
void ThemeManager::repolish(QWidget *widget) {
    widget->style()->unpolish(widget); widget->style()->polish(widget); widget->update();
}
