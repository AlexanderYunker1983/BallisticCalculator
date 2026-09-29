# SPDX-License-Identifier: GPL-3.0-or-later
QT += core gui widgets
CONFIG += c++14
TEMPLATE = app
TARGET = BallisticCalculator
win32-msvc*: QMAKE_CXXFLAGS += /utf-8
isEmpty(BALLISTIC_CORE_BUILD): BALLISTIC_CORE_BUILD = $$clean_path($$OUT_PWD/../core)
include(../core/link.pri)
include(../ui/ui.pri)
SOURCES += $$PWD/main.cpp
DISTFILES += $$PWD/qt.conf
