@echo off
rem SPDX-License-Identifier: GPL-3.0-or-later
setlocal EnableExtensions DisableDelayedExpansion
call "%~dp0scripts\build-qt.cmd" %*
exit /b %ERRORLEVEL%
