
# Makefile for linux and mac


ifeq ($(release), 1)
debug_specific_flags = -O2
build_dir = Release
else
debug_specific_flags = -g -O0
build_dir = Debug
endif


MACHINE = $(shell uname -s)
ifeq ($(MACHINE), Darwin)
platform_cflags = -DPLATFORM_MACOSX_GNU -arch x86_64 -mmacosx-version-min=10.4
platform_linkflags = -arch x86_64 -framework IOKit -framework CoreFoundation -framework CoreAudio -framework SystemConfiguration
platform_dllflags = -install_name @executable_path/$(@F)
platform_include = -I/System/Library/Frameworks/IOKit.framework/Headers/
osdir = Mac
else
platform_cflags = -Wno-psabi
platform_linkflags = 
platform_dllflags = 
platform_include = 
osdir = Posix
endif


# Macros used by Common.mak

ar = ${CROSS_COMPILE}ar rc $(objdir)
cflags = -fexceptions -Wall -Werror -pipe -D_GNU_SOURCE -D_REENTRANT -DDEFINE_LITTLE_ENDIAN -DDEFINE_TRACE $(debug_specific_flags) -fvisibility=hidden -DDllImport="__attribute__ ((visibility(\"default\")))" -DDllExport="__attribute__ ((visibility(\"default\")))" -DDllExportClass="__attribute__ ((visibility(\"default\")))" $(platform_cflags)
ohnetdir = ../ohnet/Build/Obj/$(osdir)/$(build_dir)/
objdir = Build/Obj/$(osdir)/$(build_dir)/
inc_build = Build/Include/
includes = -I../ohnet/Build/Include/ $(platform_includes)
objext = o
libprefix = lib
libext = a
exeext = elf
compiler = ${CROSS_COMPILE}gcc -fPIC -o $(objdir)
link = ${CROSS_COMPILE}g++ -lpthread $(platform_linkflags)
linkoutput = -o 
dllprefix = lib
dllext = so
link_dll = ${CROSS_COMPILE}g++ -lpthread $(platform_linkflags) $(platform_dllflags) -shared -shared-libgcc --whole-archive
csharp = gmcs /nologo
dirsep = /


default : all

make_obj_dir :
	mkdir -p $(objdir)

clean :
	rm -rf $(objdir)

all : ohNetmon ohNetworkMonitor

.PHONY : ohNetworkMonitor
ohNetworkMonitor : make_obj_dir $(objdir)$(dllprefix)ohNetworkMonitor.$(dllext)

$(objdir)$(dllprefix)ohNetworkMonitor.$(dllext) : NetworkMonitor.cpp
	$(compiler)NetworkMonitor.$(objext) -c $(cflags) $(includes) NetworkMonitor.cpp
	$(link_dll) $(linkoutput)$(objdir)$(dllprefix)NetworkMonitor.$(dllext) $(objdir)NetworkMonitor.$(objext) $(ohnetdir)DvAvOpenhomeOrgNetworkMonitor1.$(objext) $(ohnetdir)$(libprefix)ohNetCore.$(libext) $(ohnetdir)$(libprefix)TestFramework.$(libext)


.PHONY : ohNetmon
ohNetmon : make_obj_dir $(objdir)ohNetmon.$(exeext)

$(objdir)ohNetmon.$(exeext) : ohNetmon.cpp
	$(compiler)ohNetmon.$(objext) -c $(cflags) $(includes) ohNetmon.cpp
	$(link) $(linkoutput)$(objdir)ohNetmon.$(exeext) $(objdir)ohNetmon.$(objext) $(ohnetdir)$(libprefix)ohNetCore.$(libext) $(ohnetdir)$(libprefix)TestFramework.$(libext)

