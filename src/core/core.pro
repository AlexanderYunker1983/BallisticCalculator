# SPDX-License-Identifier: GPL-3.0-or-later
TEMPLATE = lib
CONFIG += staticlib c++14
CONFIG -= qt
TARGET = BallisticCore
win32-msvc*: QMAKE_CXXFLAGS += /utf-8

CONFIG(debug, debug|release) {
    TARGET = BallisticCored
    DESTDIR = $$OUT_PWD/debug
} else {
    DESTDIR = $$OUT_PWD/release
}

include(core.pri)
