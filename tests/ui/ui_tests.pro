# SPDX-License-Identifier: GPL-3.0-or-later
QT += core gui widgets testlib
CONFIG += console c++14 testcase
CONFIG -= app_bundle
TEMPLATE = app
TARGET = ui_tests
win32-msvc*: QMAKE_CXXFLAGS += /utf-8
isEmpty(BALLISTIC_CORE_BUILD): BALLISTIC_CORE_BUILD = $$clean_path($$OUT_PWD/../../src/core)
include(../../src/core/link.pri)
include(../../src/ui/ui.pri)
SOURCES += $$PWD/ui_tests.cpp
