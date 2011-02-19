@ECHO OFF

REM This is a batch script to benchmark the example webservers included in the
REM Lua/APR binding on Windows platforms. This script uses "ab.exe" (ApacheBench).

SET SECONDS=5
SET THREADS=4

REM Benchmark the single threaded webserver.
SET PORT=%RANDOM%
START lua ../examples/webserver.lua %PORT%
lua -e "require('apr').sleep(5)"
ab.exe -q -t%SECONDS% http://localhost:%PORT%/
ECHO Finished test 1

REM Benchmark the multi threaded webserver with several thread pool sizes.
REM FOR /l %%i in (1, 1, %THREADS%) DO ECHO %%i
FOR /l %%I in (1, 1, %THREADS%) DO CALL :THREADED_TEST %%I
GOTO :EOF

:THREADED_TEST
  ECHO Starting test %1
  SET PORT=%RANDOM%
  START lua ../examples/threaded-webserver.lua %1 %PORT%
  lua -e "require('apr').sleep(5)"
  ECHO ON
  ab.exe -q -t%SECONDS% -c%1 http://localhost:%PORT%/
  @ECHO OFF
  ECHO.
  GOTO :EOF
