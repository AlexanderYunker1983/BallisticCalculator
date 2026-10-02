// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QTabWidget>
#include <QTimer>
#include <QScrollArea>
#include <QScrollBar>
#include <QWheelEvent>
#include <QFont>
#include <QCheckBox>
#include <QElapsedTimer>
#include "thememanager.h"
#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#endif
#include "mainwindow.h"
#include "numberedit.h"
#include "parametersdialog.h"
#include "trajectoryplot.h"

class UiTests : public QObject {
    Q_OBJECT
    bool systemDark_ = false;
    ThemeManager *themes_ = nullptr;
private slots:
    void initTestCase() {
        qApp->setFont(QFont("Segoe UI", 10));
        themes_ = new ThemeManager(*qApp, [this] {
            ThemeManager::SystemTheme state; state.dark = systemDark_; return state;
        });
    }
    void cleanup() { systemDark_ = false; themes_->setMode(ThemeManager::Mode::System); }
    void cleanupTestCase() { delete themes_; themes_ = nullptr; }
    void russianLabelsAreUnicode() {
        MainWindow window;
        QCOMPARE(window.windowTitle().at(0).unicode(), ushort(0x0411));
        QCOMPARE(window.windowTitle().at(1).unicode(), ushort(0x0430));
        const auto text = window.findChild<QPushButton *>("startButton")->text();
        QCOMPARE(text, QString::fromUtf8("Рассчитать")); QCOMPARE(text.at(0).unicode(), ushort(0x0420));
    }
    void specificImpulseRetainsUnitsAndValues() {
        ParametersDialog dialog(ballistic::Parameters{});
        QCOMPARE(dialog.findChild<QLabel *>("specificImpulseLabel")->text(), QString::fromUtf8("Удельный импульс, м/с"));
        auto *impulse = dialog.findChild<NumberEdit *>("exhaust1");
        QCOMPARE(impulse->value(), 3297.5);
        impulse->setValue(3400.25);
        dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(dialog.parameters().exhaustVelocity[0], 3400.25);
    }
    void automaticThemeUpdatesOpenWindowsAndPlots() {
        MainWindow window; window.setAttribute(Qt::WA_DontShowOnScreen); window.show();
        QSignalSpy finished(&window, &MainWindow::calculationFinished);
        window.startCalculation(); QTRY_COMPARE(finished.count(), 1);
        QVERIFY(finished[0][0].toBool());
        ParametersDialog dialog(window.inputParameters(), &window);
        dialog.setAttribute(Qt::WA_DontShowOnScreen); dialog.show();
        auto *payload = dialog.findChild<NumberEdit *>("payload"); payload->setValue(4321);
        auto *plot = window.findChild<TrajectoryPlot *>("plot0");
        const auto originalRange = plot->viewRange();
        const QPointF cursor(plot->width() / 2.0, plot->height() / 2.0);
        QWheelEvent zoom(cursor, cursor, QPoint(), QPoint(0, 120), 120, Qt::Vertical, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(plot, &zoom);
        QVERIFY(plot->viewRange().width() < originalRange.width());
        const int count = plot->pointCount(); const auto range = plot->viewRange();
        const double velocity = window.lastResult().trajectory.back().velocity;
        const auto lightPixel = plot->grab().toImage().pixelColor(3, 3);
        QVERIFY(lightPixel.lightness() > 200);
        systemDark_ = true;
#ifdef Q_OS_WIN
        MSG event{}; event.message = WM_SETTINGCHANGE; long result = 0;
        QVERIFY(!themes_->nativeEventFilter("windows_generic_MSG", &event, &result));
#else
        themes_->refresh();
#endif
        QTRY_VERIFY(themes_->isDark());
        QVERIFY(window.palette().color(QPalette::Window).lightness() < 70);
        QVERIFY(dialog.palette().color(QPalette::Window).lightness() < 70);
        QVERIFY(payload->palette().color(QPalette::Text).lightness() > 200);
        const auto darkPixel = plot->grab().toImage().pixelColor(3, 3);
        QVERIFY(darkPixel.lightness() < 70); QVERIFY(lightPixel != darkPixel);
        QCOMPARE(payload->value(), 4321.0); QCOMPARE(plot->pointCount(), count); QCOMPARE(plot->viewRange(), range);
        QCOMPARE(window.lastResult().trajectory.back().velocity, velocity);
        // Registry edits without WM_SETTINGCHANGE are picked up by the fallback timer.
        systemDark_ = false;
        QTRY_VERIFY(!themes_->isDark());
        QCOMPARE(plot->grab().toImage().pixelColor(3, 3), lightPixel);
        QCOMPARE(payload->value(), 4321.0);
    }
    void themeChangesWhileCalculationIsRunning() {
        MainWindow window; QSignalSpy finished(&window, &MainWindow::calculationFinished);
        window.startCalculation(); QVERIFY(window.busy());
        systemDark_ = true; themes_->refresh();
        QVERIFY(themes_->isDark()); QVERIFY(!window.findChild<QPushButton *>("startButton")->isEnabled());
        window.cancelCalculation(); QTRY_COMPARE(finished.count(), 1);
        QVERIFY(!window.busy()); QVERIFY(!finished[0][0].toBool());
    }
    void themeTextContrast() {
        auto luminance = [](const QColor &color) {
            auto linear = [](double c) { return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); };
            return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
        };
        auto contrast = [&](const QColor &a, const QColor &b) {
            const double x = luminance(a), y = luminance(b);
            return (qMax(x, y) + 0.05) / (qMin(x, y) + 0.05);
        };
        for (auto mode : {ThemeManager::Mode::Light, ThemeManager::Mode::Dark}) {
            themes_->setMode(mode);
            const auto p = qApp->palette();
            QVERIFY(contrast(p.color(QPalette::Text), p.color(QPalette::Base)) >= 4.5);
            QVERIFY(contrast(p.color(QPalette::Disabled, QPalette::WindowText), p.color(QPalette::Base)) >= 4.5);
            QVERIFY(contrast(p.color(QPalette::BrightText), p.color(QPalette::Window)) >= 4.5);
            QVERIFY(contrast(p.color(QPalette::HighlightedText), p.color(QPalette::Highlight)) >= 4.5);
        }
    }
    void unfocusedNumberIgnoresWheel() {
        NumberEdit edit; edit.setValue(42); QVERIFY(!edit.hasFocus());
        QWheelEvent event(QPointF(10, 10), QPointF(10, 10), QPoint(), QPoint(0, 120), 120, Qt::Vertical, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&edit, &event); QCOMPARE(edit.value(), 42.0);
    }
    void smallWindowsAllowScrolling() {
        MainWindow window; window.setAttribute(Qt::WA_DontShowOnScreen); window.resize(760, 460); window.show();
        QApplication::processEvents();
        auto *scroll = window.findChild<QScrollArea *>(); QVERIFY(scroll);
        QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
        ParametersDialog dialog(ballistic::Parameters{});
        dialog.setAttribute(Qt::WA_DontShowOnScreen); dialog.resize(660, 430); dialog.show(); QApplication::processEvents();
        auto *form = dialog.findChild<QScrollArea *>(); QVERIFY(form); QVERIFY(form->verticalScrollBar()->maximum() > 0);
        auto *ok = dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
        QVERIFY(dialog.rect().contains(ok->mapTo(&dialog, ok->rect().center())));
    }
    void decimalSeparators_data() {
        QTest::addColumn<QString>("locale"); QTest::addColumn<QString>("text");
        QTest::newRow("ru-comma") << QString("ru_RU") << QString("14,97");
        QTest::newRow("ru-dot") << QString("ru_RU") << QString("14.97");
        QTest::newRow("en-comma") << QString("en_US") << QString("14,97");
        QTest::newRow("en-dot") << QString("en_US") << QString("14.97");
    }
    void decimalSeparators() {
        QFETCH(QString, locale); QFETCH(QString, text);
        NumberEdit edit; auto loc = QLocale(locale);
        loc.setNumberOptions(QLocale::RejectGroupSeparator | QLocale::OmitGroupSeparator);
        edit.setLocale(loc); edit.show(); edit.selectAll(); QTest::keyClicks(&edit, text); edit.interpretText();
        QVERIFY(std::abs(edit.value() - 14.97) < 1e-8);
    }
    void cancelDialogKeepsInput() {
        ballistic::Parameters original;
        ParametersDialog dialog(original); dialog.show();
        auto *payload = dialog.findChild<NumberEdit *>("payload"); QVERIFY(payload);
        payload->setValue(5000);
        QTest::mouseClick(dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Cancel), Qt::LeftButton);
        QCOMPARE(dialog.result(), int(QDialog::Rejected)); QCOMPARE(original.payload, 3000.0);
    }
    void acceptDialogValidates() {
        ParametersDialog dialog(ballistic::Parameters{}); dialog.show();
        QVERIFY(dialog.grab().save("parameters-dialog.png"));
        auto *payload = dialog.findChild<NumberEdit *>("payload"); payload->setValue(0);
        auto *ok = dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
        QTest::mouseClick(ok, Qt::LeftButton);
        QVERIFY(dialog.isVisible()); QVERIFY(!dialog.findChild<QLabel *>("validationError")->text().isEmpty());
        payload->setValue(4000); QTest::mouseClick(ok, Qt::LeftButton);
        QCOMPARE(dialog.result(), int(QDialog::Accepted)); QCOMPARE(dialog.parameters().payload, 4000.0);
    }
    void mainDialogCancelDoesNotCommit() {
        MainWindow window; window.show();
        const auto before = window.inputParameters().payload;
        QTimer::singleShot(0, [&] {
            auto *dialog = window.findChild<ParametersDialog *>();
            if (!dialog) return;
            dialog->findChild<NumberEdit *>("payload")->setValue(5000);
            dialog->reject();
        });
        window.findChild<QPushButton *>("parametersButton")->click();
        QCOMPARE(window.inputParameters().payload, before);
    }
    void calculateCancelAndRestart() {
        MainWindow window; window.setAttribute(Qt::WA_DontShowOnScreen); window.show();
        QSignalSpy finished(&window, &MainWindow::calculationFinished);
        window.startCalculation(); QVERIFY(window.busy());
        QVERIFY(!window.findChild<QPushButton *>("startButton")->isEnabled());
        window.startCalculation(); // Must not create another task.
        window.cancelCalculation();
        QTRY_COMPARE(finished.count(), 1); QVERIFY(!finished[0][0].toBool()); QVERIFY(!window.busy());
        QVERIFY(window.lastResult().trajectory.empty());
        window.startCalculation(); QTRY_COMPARE(finished.count(), 2);
        QVERIFY2(finished[1][0].toBool(), qPrintable(window.findChild<QLabel *>("statusMessage")->text()));
        QVERIFY(!window.busy()); QCOMPARE(window.lastResult().status, ballistic::Status::Completed);
        const auto plots = window.findChildren<TrajectoryPlot *>(); QCOMPARE(plots.size(), 7);
        for (auto *plot : plots) QCOMPARE(plot->pointCount(), int(window.lastResult().trajectory.size()));
        auto *tabs = window.findChild<QTabWidget *>("plotTabs"); QCOMPARE(tabs->count(), 5);
        for (int i = 0; i < tabs->count(); ++i) { tabs->setCurrentIndex(i); QVERIFY(!window.grab().isNull()); }
        const auto before = window.lastResult().trajectory.back().velocity;
        window.findChild<NumberEdit *>("maxStep")->setValue(0);
        window.startCalculation(); QCOMPARE(finished.count(), 3); QVERIFY(!finished[2][0].toBool());
        QCOMPARE(window.lastResult().trajectory.back().velocity, before);
    }
    void pendingInputIsUsed_data() {
        QTest::addColumn<bool>("shortcut");
        QTest::newRow("button") << false;
        QTest::newRow("shortcut") << true;
    }
    void pendingInputIsUsed() {
        QFETCH(bool, shortcut);
        MainWindow window; window.setAttribute(Qt::WA_DontShowOnScreen); window.show();
        QApplication::setActiveWindow(&window); QApplication::processEvents();
        window.findChild<QCheckBox *>("optimize")->setChecked(false);
        auto *field = window.findChild<NumberEdit *>("targetAltitude");
        field->setFocus(); field->selectAll(); QTest::keyClicks(field, "200");
        QCOMPARE(field->value(), 250.0); QVERIFY(field->hasFocus());
        QSignalSpy finished(&window, &MainWindow::calculationFinished);
        if (shortcut) QTest::keyClick(field, Qt::Key_Return, Qt::ControlModifier);
        else QTest::mouseClick(window.findChild<QPushButton *>("startButton"), Qt::LeftButton);
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY(finished[0][0].toBool());
        QCOMPARE(window.lastResult().options.targetAltitude, 200000.0);
    }
    void pendingInvalidInputIsRejected_data() {
        QTest::addColumn<QString>("text");
        QTest::newRow("zero-step") << QString("0");
        QTest::newRow("unfinished") << QString();
    }
    void pendingInvalidInputIsRejected() {
        QFETCH(QString, text);
        MainWindow window; window.setAttribute(Qt::WA_DontShowOnScreen); window.show();
        QApplication::setActiveWindow(&window); QApplication::processEvents();
        auto *field = window.findChild<NumberEdit *>("maxStep");
        field->setFocus(); field->selectAll();
        QTest::keyClick(field, Qt::Key_Backspace);
        if (!text.isEmpty()) QTest::keyClicks(field, text);
        QSignalSpy finished(&window, &MainWindow::calculationFinished);
        QTest::keyClick(field, Qt::Key_Return, Qt::ControlModifier);
        QCOMPARE(finished.count(), 1); QVERIFY(!finished[0][0].toBool());
        QVERIFY(!window.busy()); QVERIFY(window.lastResult().trajectory.empty());
        QVERIFY(!window.findChild<QLabel *>("statusMessage")->text().isEmpty());
    }
    void closeWhileRunning() {
        MainWindow window; window.setAttribute(Qt::WA_DontShowOnScreen); window.show();
        window.startCalculation(); QVERIFY(window.busy()); window.close();
        QTRY_VERIFY(!window.busy()); QTRY_VERIFY(!window.isVisible());
    }
    void plotInteractionAndEmptyData() {
        TrajectoryPlot plot("Test", "x", "y"); plot.resize(600, 400); plot.show();
        QVERIFY(!plot.grab().isNull());
        plot.setData({{0, 0}, {1, 20}, {2, -10}, {3, 0}}, {1, 2});
        QTest::mouseMove(&plot, QPoint(250, 180));
        QTest::mousePress(&plot, Qt::LeftButton, Qt::NoModifier, QPoint(250, 180));
        QTest::mouseMove(&plot, QPoint(270, 195)); QTest::mouseRelease(&plot, Qt::LeftButton);
        QTest::mouseDClick(&plot, Qt::LeftButton); QVERIFY(!plot.grab().isNull());
        plot.setData({{1, 1}, {1, 1}}); QVERIFY(!plot.grab().isNull());
        plot.setData({}); QVERIFY(!plot.grab().isNull());
    }
    void plotAggregationPreservesExtrema() {
        QVector<QPointF> points;
        for (int i = 0; i < 10000; ++i) points.append({double(i), 0});
        points[1234].setY(100); points[1240].setY(-100);
        PlotSeries series(points);
        const auto selected = series.visibleIndices(0, 9999, 100);
        QVERIFY(selected.contains(0)); QVERIFY(selected.contains(9999));
        QVERIFY(selected.contains(1234)); QVERIFY(selected.contains(1240));
        QVERIFY(std::is_sorted(selected.cbegin(), selected.cend()));
        QVERIFY(selected.size() <= 408);
        const auto zoom = series.visibleIndices(1230, 1250, 400);
        QVERIFY(zoom.contains(1234)); QVERIFY(zoom.contains(1240));
        QCOMPARE(series.nearestIndex(1234.1), 1234);
        PlotSeries loop({{2, 0}, {0, 1}, {1, -1}, {0, 2}});
        QCOMPARE(loop.nearestIndex(0), 1); QCOMPARE(loop.nearestIndex(1.1), 2);
        const auto loopIndices = loop.visibleIndices(0, 2, 100);
        QVERIFY(std::is_sorted(loopIndices.cbegin(), loopIndices.cend()));
    }
    void largePlotCachesGeometry() {
        QVector<QPointF> points; points.reserve(2000000);
        for (int i = 0; i < 2000000; ++i) points.append({double(i), std::sin(i * 0.001)});
        QElapsedTimer timer; timer.start(); PlotSeries series(std::move(points));
        const auto preparation = timer.nsecsElapsed(); const auto bytes = series.storageBytes();
        TrajectoryPlot plot("Large", "x", "y"); plot.setAttribute(Qt::WA_DontShowOnScreen); plot.resize(800, 500); plot.show();
        plot.setSeries(std::move(series), {12345}); timer.restart(); plot.grab();
        const auto firstPaint = timer.nsecsElapsed(); const auto builds = plot.geometryBuildCount();
        QVERIFY(plot.geometryPointCount() > 1); QVERIFY(plot.geometryPointCount() < 4000);
        timer.restart();
        for (int i = 0; i < 20; ++i) {
            QMouseEvent hover(QEvent::MouseMove, QPointF(200 + i, 150), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(&plot, &hover); plot.grab();
        }
        qInfo("large plot: prepare=%.3f ms, first paint=%.3f ms, hover paint=%.3f ms, series=%lld bytes",
              preparation / 1e6, firstPaint / 1e6, timer.nsecsElapsed() / 20e6, static_cast<long long>(bytes));
        QCOMPARE(plot.geometryBuildCount(), builds);
        plot.resize(900, 500); plot.grab(); QVERIFY(plot.geometryBuildCount() > builds);
        const auto resized = plot.geometryBuildCount(); themes_->setMode(ThemeManager::Mode::Dark); plot.grab();
        QCOMPARE(plot.geometryBuildCount(), resized);
        QVERIFY(plot.grab().toImage().pixelColor(3, 3).lightness() < 70);
        plot.setData({{0, 0}, {1, 1}}); plot.grab(); QVERIFY(plot.geometryBuildCount() > resized);
    }
};
QTEST_MAIN(UiTests)
#include "ui_tests.moc"
