# Microsoft Developer Studio Generated NMAKE File, Based on libapreq.dsp
# Compile with NMAKE /f libapreq2.mak "CFG=libapreq2 - Win32 Debug"

APR_LIB=$(ROOT)\apr\Debug\libapr-1.lib
APU_LIB=$(ROOT)\apr-util\Debug\libaprutil-1.lib
APREQ_HOME=$(ROOT)\libapreq2

!IF "$(CFG)" == ""
CFG=libapreq2 - Win32 Release
!MESSAGE No configuration specified. Defaulting to libapreq2 - Win32 Release.
!ENDIF

!IF "$(CFG)" != "libapreq2 - Win32 Release" && "$(CFG)" != "libapreq2 - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libapreq2.mak" CFG="libapreq2 - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libapreq2 - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libapreq2 - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

CPP=cl.exe /I$(ROOT)\apr\include /I$(ROOT)\apr-util\include
RSC=rc.exe /I$(ROOT)\apr\include /I$(ROOT)\apr-util\include

CFG_HOME=$(APREQ_HOME)\win32
OUTDIR=$(CFG_HOME)\libs
INTDIR=$(CFG_HOME)\libs
LIBDIR=$(APREQ_HOME)\library

LINK32_OBJS= \
	"$(INTDIR)\cookie.obj" \
	"$(INTDIR)\param.obj" \
	"$(INTDIR)\parser.obj" \
	"$(INTDIR)\parser_header.obj" \
	"$(INTDIR)\parser_multipart.obj" \
	"$(INTDIR)\parser_urlencoded.obj" \
	"$(INTDIR)\util.obj" \
	"$(INTDIR)\version.obj" \
	"$(INTDIR)\error.obj" \
	"$(INTDIR)\libapreq.res"

!IF  "$(CFG)" == "libapreq2 - Win32 Release"

ALL: "$(OUTDIR)\libapreq2.dll"

"$(OUTDIR)":
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MD /W3 /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "APREQ_DECLARE_EXPORT" /I"$(APREQ_HOME)\include" /FD /c 
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC_PROJ=/l 0x409 /d "NDEBUG" /i "$(APREQ_HOME)\include"
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\libapreq2.bsc" 
LINK32=link.exe
MANIFEST=$(OUTDIR)\libapreq2.dll.manifest
LINK32_FLAGS="$(APR_LIB)" "$(APU_LIB)" kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /incremental:no /machine:I386 /out:"$(OUTDIR)\libapreq2.dll" /implib:"$(OUTDIR)\libapreq2.lib"

"$(OUTDIR)\libapreq2.dll": "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) $(LINK32_FLAGS) $(DEF_FLAGS) $(LINK32_OBJS)
    if exist $(MANIFEST) mt /nologo /manifest $(MANIFEST) /outputresource:$(OUTDIR)\libapreq2.dll;2

!ELSEIF  "$(CFG)" == "libapreq2 - Win32 Debug"

ALL: "$(OUTDIR)\libapreq2.dll"

"$(OUTDIR)":
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MDd /W3 /Gm /EHsc /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "APREQ_DECLARE_EXPORT" /I"$(APREQ_HOME)\include" /FD /RTC1  /c 
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC_PROJ=/l 0x409 /d "_DEBUG" /i "$(APREQ_HOME)\include"
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\libapreq2.bsc" 
LINK32=link.exe
MANIFEST=$(OUTDIR)\libapreq2.dll.manifest
LINK32_FLAGS="$(APR_LIB)" "$(APU_LIB)" kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /incremental:yes /pdb:"$(OUTDIR)\libapreq2.pdb" /debug /machine:I386 /out:"$(OUTDIR)\libapreq2.dll" /implib:"$(OUTDIR)\libapreq2.lib" /pdbtype:sept 

"$(OUTDIR)\libapreq2.dll": "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) $(LINK32_FLAGS) $(DEF_FLAGS) $(LINK32_OBJS)
    if exist $(MANIFEST) mt /nologo /manifest $(MANIFEST) /outputresource:$(OUTDIR)\libapreq2.dll;2

!ENDIF 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<


!IF "$(CFG)" == "libapreq2 - Win32 Release" || "$(CFG)" == "libapreq2 - Win32 Debug"

SOURCE=$(LIBDIR)\cookie.c

"$(INTDIR)\cookie.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) /Fo"$(INTDIR)\cookie.obj" $(CPP_PROJ) $(SOURCE)

SOURCE=$(LIBDIR)\param.c

"$(INTDIR)\param.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) /Fo"$(INTDIR)\param.obj" $(CPP_PROJ) $(SOURCE)

SOURCE=$(LIBDIR)\parser.c

"$(INTDIR)\parser.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) /Fo"$(INTDIR)\parser.obj" $(CPP_PROJ) $(SOURCE)

SOURCE=$(LIBDIR)\parser_header.c

"$(INTDIR)\parser_header.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) /Fo"$(INTDIR)\parser_header.obj" $(CPP_PROJ) $(SOURCE)

SOURCE=$(LIBDIR)\parser_multipart.c

"$(INTDIR)\parser_multipart.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) /Fo"$(INTDIR)\parser_multipart.obj" $(CPP_PROJ) $(SOURCE)

SOURCE=$(LIBDIR)\parser_urlencoded.c

"$(INTDIR)\parser_urlencoded.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) /Fo"$(INTDIR)\parser_urlencoded.obj" $(CPP_PROJ) $(SOURCE)

SOURCE=$(LIBDIR)\version.c

"$(INTDIR)\version.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) /Fo"$(INTDIR)\version.obj" $(CPP_PROJ) $(SOURCE)

SOURCE=$(LIBDIR)\util.c

"$(INTDIR)\util.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) /Fo"$(INTDIR)\util.obj" $(CPP_PROJ) $(SOURCE)

SOURCE=$(LIBDIR)\error.c

"$(INTDIR)\error.obj" : $(SOURCE) "$(INTDIR)"
        $(CPP) /Fo"$(INTDIR)\error.obj" $(CPP_PROJ) $(SOURCE)

SOURCE=.\libapreq.rc

"$(INTDIR)\libapreq.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) /fo"$(INTDIR)\libapreq.res" $(RSC_PROJ) $(SOURCE)

!ENDIF 

