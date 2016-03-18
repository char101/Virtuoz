CC = cl
LINK = link
RC = rc
RCFLAGS = /I wtl
LIBS = TTLib\TTLib64.lib
DEFINES = /D "WIN32" /D "_WINDOWS" /D "STRICT" /D "NDEBUG" /D "_UNICODE" /D "UNICODE"
CFLAGS = /nologo /O2 /GS- /GL /Gw /I wtl /MD
LDFLAGS = /nologo /opt:ref /ltcg /manifest /dynamicbase /incremental:no

.cpp.obj:
	$(CC) $(CFLAGS) $(DEFINES) /c $*.cpp

.rc.res:
	$(RC) $(RCFLAGS) $*.rc

all: virtuoz.exe

virtuoz.exe: stdafx.obj VirtualDesktops.obj VirtualDesktopsConfig.obj MainDlg.obj Virtuoz.obj Virtuoz.res
	$(LINK) $(LDFLAGS) /OUT:$@ $** $(LIBS)

clean:
	del *.obj *.res *.exe
