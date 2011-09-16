:: Batch script to bootstrap a development environment for the Lua/APR binding.
::
:: Author: Peter Odding <peter@peterodding.com>
:: Last Change: September 11, 2011
:: Homepage: http://peterodding.com/code/lua/apr/
:: License: MIT
::
:: This is a Windows batch script that bootstraps a development environment for
:: the Lua/APR binding. Run this script from a Windows SDK command prompt.
:: Apart from the Windows SDK this script depends on the programs "wget.exe",
:: "unzip.exe", "gunzip.exe", "tar.exe" and "sed.exe" from UnxUtils. I've
:: tested this script on a virtualized 32 bits Windows XP installation in both
:: the Win32 and x64 Windows SDK command prompts. There's a ZIP file with this
:: script, the patched "libapreq2.mak" makefile and the above executables
:: available online at the following location:
::
::   http://peterodding.com/code/lua/apr/downloads/win32-bootstrap.zip
::
:: TODO Currently fails to build libapreq2 for x64. Is it even possible?!

:: Pick either "Debug" or "Release".
SET BUILD_MODE=Debug

:: Version strings (embedded in filenames and URLs).
:: This script has only been tested with the versions below.
SET APR_VERSION=1.4.5
SET APU_VERSION=1.3.12
SET API_VERSION=1.2.1
SET APQ_VERSION=2.13
SET SQLITE_VERSION=3070701

:: Location of source code archives mirror.
SET APR_MIRROR=http://www.apache.org/dist/apr
SET APQ_MIRROR=http://www.apache.org/dist/httpd/libapreq
SET SQLITE_SITE=http://sqlite.org

:: Names of source code archives.
SET APR_ARCHIVE=apr-%APR_VERSION%-win32-src.zip
SET APU_ARCHIVE=apr-util-%APU_VERSION%-win32-src.zip
SET API_ARCHIVE=apr-iconv-%API_VERSION%-win32-src-r2.zip
SET APQ_ARCHIVE=libapreq2-%APQ_VERSION%.tar.gz
SET SQLITE_ARCHIVE=sqlite-amalgamation-%SQLITE_VERSION%.zip

:: Try to detect the intended platform.
IF "%PLATFORM%" == "" (
  IF /i "%CPU%" == "i386" (
    SET PLATFORM=Win32
  ) ELSE (
    IF /i "%CPU%" == "AMD64" (
      SET PLATFORM=x64
    ) ELSE (
      ECHO Failed to determine CPU type, please set PLATFORM=Win32 or PLATFORM=x64
      EXIT /b 1
    )
  )
)

:: Get any missing source code archives.
IF NOT EXIST %APR_ARCHIVE% WGET %APR_MIRROR%/%APR_ARCHIVE% || (ECHO Failed to download APR archive & EXIT /b 1)
IF NOT EXIST %APU_ARCHIVE% WGET %APR_MIRROR%/%APU_ARCHIVE% || (ECHO Failed to download APR-util archive & EXIT /b 1)
IF NOT EXIST %API_ARCHIVE% WGET %APR_MIRROR%/%API_ARCHIVE% || (ECHO Failed to download APR-iconv archive & EXIT /b 1)
IF NOT EXIST %APQ_ARCHIVE% WGET %APQ_MIRROR%/%APQ_ARCHIVE% || (ECHO Failed to download libapreq2 archive & EXIT /b 1)
IF NOT EXIST %SQLITE_ARCHIVE% WGET %SQLITE_SITE%/%SQLITE_ARCHIVE% || (ECHO Failed to download SQLite3 archive & EXIT /b 1)

:: Create the directory where the resulting binaries are stored.
IF NOT EXIST binaries MKDIR binaries || (ECHO Failed to create binaries directory & EXIT /b 1)

:: Unpack the source code archives.
IF NOT EXIST apr (
  UNZIP -qo %APR_ARCHIVE% || (ECHO Failed to unpack APR archive & EXIT /b 1)
	MOVE apr-%APR_VERSION% apr || (ECHO Failed to rename APR directory & EXIT /b 1)
  :: I haven't a clue why this is needed, but it certainly helps :-)
	TYPE apr\include\apr.hw > apr\apr.h
	TYPE apr\include\apr.hw > apr\include\apr.h
)
IF NOT EXIST apr-util (
	UNZIP -qo %APU_ARCHIVE% || (ECHO Failed to unpack APR-util archive & EXIT /b 1)
	MOVE apr-util-%APU_VERSION% apr-util || (ECHO Failed to rename APR-util directory & EXIT /b 1)
)
IF NOT EXIST apr-iconv (
	UNZIP -qo %API_ARCHIVE% || (ECHO Failed to unpack APR-iconv archive & EXIT /b 1)
	MOVE apr-iconv-%API_VERSION% apr-iconv || (ECHO Failed to rename APR-iconv directory & EXIT /b 1)
)
IF NOT EXIST libapreq2 (
  COPY %APQ_ARCHIVE% %APQ_ARCHIVE%.tmp
  GUNZIP -f %APQ_ARCHIVE% || (ECHO Failed to decompress libapreq2 archive & EXIT /b 1)
  MOVE %APQ_ARCHIVE%.tmp %APQ_ARCHIVE%
	TAR xf *.tar || (ECHO Failed to unpack libapreq2 archive & EXIT /b 1)
  DEL libapreq2-*.tar
	MOVE libapreq2-%APQ_VERSION% libapreq2 || (ECHO Failed to rename libapreq2 directory & EXIT /b 1)
)
IF NOT EXIST sqlite3 (
	UNZIP -qo %SQLITE_ARCHIVE% || (ECHO Failed to unpack SQLite3 archive & EXIT /b 1)
	MOVE sqlite-amalgamation-%SQLITE_VERSION% sqlite3 || (ECHO Failed to rename SQLite3 directory & EXIT /b 1)
  :: Copy sqlite3.h (needed to build apr_dbd_sqlite3-1.dll).
  COPY /Y sqlite3\sqlite3.h apr-util\include || (ECHO Failed to copy sqlite3.h & EXIT /b 1)
)

:: APR creates 32 bit builds in the "Debug" and "Release" directories
:: but 64 bit builds in the "x64\Debug" or "x64\Release" directories.
SET BUILD_DIR=%BUILD_MODE%
IF %PLATFORM% == x64 SET BUILD_DIR=x64\%BUILD_DIR%

:: Build the APR library.
PUSHD apr
NMAKE /nologo /f libapr.mak CFG="libapr - %PLATFORM% %BUILD_MODE%" || (POPD & ECHO Failed to build APR library & EXIT /b 1)
POPD
COPY apr\%BUILD_DIR%\libapr-1.dll binaries || (ECHO Failed to copy APR binary & EXIT /b 1)
IF EXIST apr\%BUILD_DIR%\libapr-1.pdb COPY apr\%BUILD_DIR%\libapr-1.pdb binaries

:: Build the APR-iconv library.
PUSHD apr-iconv
NMAKE /nologo /f libapriconv.mak CFG="libapriconv - %PLATFORM% %BUILD_MODE%" || (POPD & ECHO Failed to build APR-iconv library & EXIT /b 1)
POPD
:: Workaround for "LINK : fatal error LNK1181: cannot open input file ..\Debug\libapriconv-1.lib".
IF %PLATFORM% == x64 XCOPY /Y apr-iconv\x64\%BUILD_MODE% apr-iconv
COPY apr-iconv\%BUILD_DIR%\libapriconv-1.dll binaries
IF EXIST apr-iconv\%BUILD_DIR%\libapriconv-1.pdb COPY apr-iconv\%BUILD_DIR%\libapriconv-1.pdb binaries
:: TODO I'm not sure how or why but apparently the above does not build the
:: *.so files, instead they are built by the apr-util section below. This is
:: why the *.so files are copied after building apr-util instead of here...

:: Build the APR-util library.
IF "%PLATFORM%" == "x64" (
  :: I haven't a clue why this is needed, but it certainly helps :-)
  TYPE apr-util\include\apu.hw > apr-util\include\apu.h
)
PUSHD apr-util
NMAKE /nologo /f libaprutil.mak CFG="libaprutil - %PLATFORM% %BUILD_MODE%" || (POPD & ECHO Failed to build APR-util library & EXIT /b 1)
POPD
COPY apr-util\%BUILD_DIR%\libaprutil-1.dll binaries
IF EXIST apr-util\%BUILD_DIR%\libaprutil-1.pdb COPY apr-util\%BUILD_DIR%\libaprutil-1.pdb binaries

:: TODO As explained above, apr-util builds the apr-iconv *.so files... 
IF NOT EXIST binaries\iconv MKDIR binaries\iconv
COPY apr-iconv\%BUILD_MODE%\iconv\*.so binaries\iconv
COPY apr-iconv\%BUILD_MODE%\iconv\*.pdb binaries\iconv

:: Build the SQLite3 database driver.
PUSHD sqlite3
CL /LD /D"SQLITE_API=__declspec(dllexport)" sqlite3.c || (POPD & ECHO Failed to build SQLite3 library & EXIT /b 1)
POPD
COPY /Y sqlite3\sqlite3.dll binaries || (POPD & ECHO Failed to copy sqlite3.dll & EXIT /b 1)
COPY /Y sqlite3\sqlite3.lib apr-util\dbd || (POPD & ECHO Failed to copy sqlite3.lib & EXIT /b 1)
PUSHD apr-util\dbd
NMAKE /nologo /f apr_dbd_sqlite3.mak CFG="apr_dbd_sqlite3 - %PLATFORM% %BUILD_MODE%" || (POPD & ECHO Failed to build SQLite3 driver & EXIT /b 1)
POPD
COPY apr-util\dbd\%BUILD_MODE%\apr_dbd_sqlite3-1.dll binaries
IF EXIST apr-util\dbd\%BUILD_MODE%\apr_dbd_sqlite3-1.pdb COPY apr-util\dbd\%BUILD_MODE%\apr_dbd_sqlite3-1.pdb binaries

:: Build the libapreq2 library.
SET ROOT="%CD%"
:: The makefile included with libapreq2 assumes it's build as part of Apache.
:: We replace it with a custom makefile that doesn't include the Apache glue.
TYPE libapreq2.mak > libapreq2\win32\libapreq2.mak
PUSHD libapreq2
NMAKE /nologo /f win32\libapreq2.mak CFG="libapreq2 - %PLATFORM% %BUILD_MODE%" ROOT="%ROOT%" APR_LIB="%ROOT%\apr\%BUILD_DIR%\libapr-1.lib" APU_LIB="%ROOT%\apr-util\%BUILD_DIR%\libaprutil-1.lib" || (POPD & ECHO Failed to build libapreq2 library & EXIT /b 1)
POPD
COPY libapreq2\win32\libs\libapreq2.dll binaries
IF EXIST libapreq2\win32\libs\libapreq2.pdb COPY libapreq2\win32\libs\libapreq2.pdb binaries
