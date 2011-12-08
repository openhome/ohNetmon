# Makefile for Windows

!if "$(release)"=="1"
build = Release
linkdebug = 
compdebug = /MT /Ox
!else
build = Debug
linkdebug = /debug
compdebug = /MTd /Zi /Od /RTC1
!endif

# Macros used by Common.mak

platform = Windows

objdir = Build/Obj/$(platform)/$(build)

includes = -I../ohNet/Build/Include

objext = obj

exeext = exe

libprefix = lib
libext = lib

dllprefix =
dllext = dll

compflags = /W4 /WX /EHsc /FR$(objdir) -DDEFINE_LITTLE_ENDIAN -DDEFINE_TRACE -D_CRT_SECURE_NO_WARNINGS $(compdebug) 
comp = cl /nologo $(compflags) $(includes) 
compout = /Fo$(objdir)

link = link /nologo /SUBSYSTEM:CONSOLE /map Ws2_32.lib Iphlpapi.lib Dbghelp.lib /incremental:no $(linkdebug) 
linkout = /out:$(objdir)

csharp = csc /nologo /platform:x86 /nostdlib /reference:%SystemRoot%\microsoft.net\framework\v2.0.50727\mscorlib.dll /debug

default : all

make_obj_dir : $(objdir)

$(objdir) :
	if not exist Build\Obj\$(platform)\$(build) mkdir Build\Obj\$(platform)\$(build)

clean:
	del /S /Q Build\Obj\$(platform)\$(build)

include Common.mak
