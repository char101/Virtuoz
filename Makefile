CC = cl
LINK = link
RC = rc
RCFLAGS = /I wtl
LIBS = TTLib\TTLib64.lib
DEFINES = /D "WIN32" /D "_WINDOWS" /D "STRICT" /D "NDEBUG" /D "_UNICODE" /D "UNICODE"
CFLAGS = /nologo /O2 /GS- /GL /Gw /I wtl /MD /EHsc
LDFLAGS = /nologo /opt:ref /ltcg /manifest /incremental:no

.cpp.obj:
	$(CC) $(CFLAGS) $(DEFINES) /c $*.cpp

.rc.res:
	$(RC) $(RCFLAGS) $*.rc

all: virtuoz.exe

virtuoz.exe: stdafx.obj VirtualDesktops.obj VirtualDesktopsConfig.obj MainDlg.obj Virtuoz.obj Virtuoz.res
	$(LINK) $(LDFLAGS) /OUT:$@ $** $(LIBS)
	dir virtuoz.exe

stdafx.obj: stdafx.h stdafx.cpp

VirtualDesktops.obj: VirtualDesktops.h VirtualDesktops.cpp

VirtualDesktopsConfig.obj: VirtualDesktopsConfig.h VirtualDesktopsConfig.cpp

MainDlg.obj: MainDlg.h MainDlg.cpp

Virtuoz.obj: Virtuoz.h Virtuoz.cpp

Virtuoz.res: Virtuoz.rc res\Virtuoz.ico

clean:
	del *.obj *.res *.exe
