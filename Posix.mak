# Makefile for linux and mac

ifeq ($(release), 1)
build = Release
linkdebug = 
compdebug = -O2
else
build = Debug
linkdebug = 
compdebug = -g -O0
endif

MACHINE = $(shell uname -s)
ifeq ($(MACHINE), Darwin)
platform_compflags = -DPLATFORM_MACOSX_GNU -arch x86_64 -mmacosx-version-min=10.4
platform_linkflags = -arch x86_64 -framework IOKit -framework CoreFoundation -framework CoreAudio -framework SystemConfiguration
platform_dllflags = -install_name @executable_path/$(@F)
platform_include = -I/System/Library/Frameworks/IOKit.framework/Headers/
platform = Mac
else
platform_compflags = -Wno-psabi
platform_linkflags = 
platform_dllflags = 
platform_include = 
platform = Posix
endif

# Macros used by Common.mak

objdir = Build/Obj/$(platform)/$(build)

includes = -I../ohNet/Build/Include $(platform_includes)

objext = o

exeext = elf

libprefix = lib
libext = a

dllprefix = lib
dllext = so

compflags = -fexceptions -Wall -Werror -pipe -D_GNU_SOURCE -D_REENTRANT -DDEFINE_LITTLE_ENDIAN -DDEFINE_TRACE -fvisibility=hidden -DDllImport="__attribute__ ((visibility(\"default\")))" -DDllExport="__attribute__ ((visibility(\"default\")))" -DDllExportClass="__attribute__ ((visibility(\"default\")))" $(platform_compflags) $(compdebug) 
comp = ${CROSS_COMPILE}gcc -fPIC $(compflags) $(includes) 
compout = -o $(objdir)

link = ${CROSS_COMPILE}g++ -lpthread $(platform_linkflags) $(linkdebug) 
linkout = -o $(objdir)

csharp = gmcs /nologo

default : all

make_obj_dir : $(objdir)

$(objdir) :
	mkdir -p $(objdir)

clean:
	rm -rf $(objdir)

include Common.mak
