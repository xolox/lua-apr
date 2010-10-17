@ECHO OFF

REM On Windows the Lua/APR binding is compiled using NMAKE /f Makefile.win.
REM This batch script makes it possible to just type MAKE and be done with it.

NMAKE /nologo /f Makefile.win %*
