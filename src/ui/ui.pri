# SPDX-License-Identifier: GPL-3.0-or-later
INCLUDEPATH += $$PWD
HEADERS += $$PWD/validationfield.h
HEADERS += $$PWD/plotseries.h $$PWD/plotdata.h $$PWD/calculationcontroller.h
SOURCES += $$PWD/plotseries.cpp $$PWD/plotdata.cpp $$PWD/calculationcontroller.cpp
HEADERS += $$PWD/numberedit.h $$PWD/parametersdialog.h $$PWD/trajectoryplot.h $$PWD/calculationtask.h $$PWD/mainwindow.h $$PWD/thememanager.h
SOURCES += $$PWD/parametersdialog.cpp $$PWD/trajectoryplot.cpp $$PWD/mainwindow.cpp $$PWD/thememanager.cpp
win32: LIBS += -ldwmapi -luser32
RESOURCES += $$PWD/ui.qrc
