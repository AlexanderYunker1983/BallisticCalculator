# SPDX-License-Identifier: GPL-3.0-or-later
INCLUDEPATH += $$PWD
HEADERS += $$PWD/model.h $$PWD/rk4.h $$PWD/atmosphere.h $$PWD/dynamics.h $$PWD/solver.h
SOURCES += $$PWD/model.cpp $$PWD/validation.cpp $$PWD/atmosphere.cpp $$PWD/dynamics.cpp $$PWD/solver.cpp
DISTFILES += $$PWD/upper_atmosphere.inc
