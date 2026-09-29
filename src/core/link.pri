# SPDX-License-Identifier: GPL-3.0-or-later
# Consumer configuration. BALLISTIC_CORE_BUILD is the shadow-build directory
# containing core.pro's Makefile, not the source directory.
isEmpty(BALLISTIC_CORE_BUILD): error(Set BALLISTIC_CORE_BUILD to the core build directory)
INCLUDEPATH += $$PWD

CONFIG(debug, debug|release) {
    BALLISTIC_CORE_NAME = BallisticCored
    BALLISTIC_CORE_DIR = $$BALLISTIC_CORE_BUILD/debug
} else {
    BALLISTIC_CORE_NAME = BallisticCore
    BALLISTIC_CORE_DIR = $$BALLISTIC_CORE_BUILD/release
}
win32-msvc* {
    BALLISTIC_CORE_LIBRARY = $$BALLISTIC_CORE_DIR/$${BALLISTIC_CORE_NAME}.lib
} else {
    BALLISTIC_CORE_LIBRARY = $$BALLISTIC_CORE_DIR/lib$${BALLISTIC_CORE_NAME}.a
}
LIBS += $$quote($$BALLISTIC_CORE_LIBRARY)
PRE_TARGETDEPS += $$quote($$BALLISTIC_CORE_LIBRARY)
