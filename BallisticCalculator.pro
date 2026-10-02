# SPDX-License-Identifier: GPL-3.0-or-later
TEMPLATE = subdirs
SUBDIRS += core app core_tests ui_tests

core.subdir = src/core
app.subdir = src/app
core_tests.file = tests/core/core_tests.pro
ui_tests.file = tests/ui/ui_tests.pro

app.depends = core
core_tests.depends = core
ui_tests.depends = core

DISTFILES += README.md LICENSE NOTICE .gitignore build-release.cmd rebuild-release.cmd \
    LICENSES/MIT-legacy.txt \
    docs/user-guide.ru.md docs/core-api.ru.md docs/ui-design.ru.md \
    scripts/build-qt.cmd scripts/clean-qt.ps1 scripts/package-qt.ps1 scripts/qt-license-notices.ps1
DISTFILES += scripts/reference-solver.py docs/numerical-validation.ru.md docs/refactoring-validation.ru.md
