// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QCommandLineParser>
#include <QFont>
#include <QTimer>
#include <cstdio>
#include "mainwindow.h"
#include "parametersdialog.h"
#include "thememanager.h"
#include <QDir>

int main(int argc, char *argv[]) {
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication app(argc, argv);
    app.setApplicationName(QString::fromUtf8("BallisticCalculator")); app.setApplicationVersion(QString::fromUtf8("2.0.0"));
    app.setFont(QFont(QString::fromUtf8("Segoe UI"), 10));
    QCommandLineParser parser; parser.addHelpOption(); parser.addVersionOption();
    QCommandLineOption smoke(QString::fromUtf8("smoke-test"), QString::fromUtf8("Calculate, save all tab screenshots and exit."), QString::fromUtf8("directory"));
    QCommandLineOption theme(QString::fromUtf8("theme"), QString::fromUtf8("Color theme: system, light or dark (default: system)."), "mode", "system");
    parser.addOption(smoke); parser.addOption(theme); parser.process(app);
    ThemeManager themes(app);
    const auto mode = parser.value(theme).toLower();
    if (mode == "light") themes.setMode(ThemeManager::Mode::Light);
    else if (mode == "dark") themes.setMode(ThemeManager::Mode::Dark);
    else if (mode != "system") { std::fprintf(stderr, "Unknown theme. Use system, light or dark.\n"); return 1; }
    MainWindow window;
    if (parser.isSet(smoke)) window.setAttribute(Qt::WA_DontShowOnScreen);
    window.show();
    if (parser.isSet(smoke)) {
        QObject::connect(&window, &MainWindow::calculationFinished, &app, [&](bool success) {
            bool saved = success && window.savePlotScreenshots(parser.value(smoke));
            if (saved) {
                ParametersDialog dialog(window.inputParameters(), &window);
                dialog.setAttribute(Qt::WA_DontShowOnScreen); dialog.show(); QApplication::processEvents();
                saved = dialog.grab().save(QDir(parser.value(smoke)).filePath("parameters-dialog.png"));
            }
            std::printf("Qt %s: calculation=%s, screenshots=%s\n", qVersion(), success ? "OK" : "FAILED", saved ? "OK" : "FAILED");
            app.exit(saved ? 0 : 1);
        });
        QTimer::singleShot(0, &window, &MainWindow::startCalculation);
        QTimer::singleShot(90000, &app, [&] { app.exit(2); });
    }
    return app.exec();
}
