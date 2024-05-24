# Project:   CBLib
include MakeCommon

# Tools
CC = cc
LibFile = libfile
AS = objasm

# Toolflags:
CCCommonFlags =  -c -depend !Depend -IC: -throwback -DCOPY_ARRAY_ARGS -DUSE_FILEPERC -fahi -apcs 3/32/fpe2/swst/fp/nofpr -memaccess -L22-S22-L41 -o $@
CCFlags = $(CCCommonFlags) -DNDEBUG -Otime
CCDebugFlags = $(CCCommonFlags) -g -DUSE_CBDEBUG -DDEBUG_OUTPUT -DDEBUG_DUMP -DFORTIFY 
CCModuleFlags = $(CCCommonFlags) -DNDEBUG -Ospace -zM -zps1 -ff -DINCLUDE_FINALISATION_CODE
CCObsoleteFlags = $(CCFlags) -DCBLIB_OBSOLETE
ASFlags = -throwback -NoCache -depend !Depend -apcs 3/32 -o $@
ASDebugFlags = -G $(ASFlags)
LibFileFlags = -c -o $@

# Acorn Make doesn't find object files in subdirectories if referenced by
# non-standard file name suffixes so use addprefix not addsuffix here
ReleaseObjects = $(addprefix o.,$(ObjectList))
DebugObjects = $(addprefix debug.,$(DebugList))
ObsoleteObjects = $(addprefix obs.,$(ObsoleteList))
ModuleObjects = $(addprefix oz.,$(ObjectList))

XReleaseObjects = $(addprefix o.,$(XObjectList))
XDebugObjects = $(addprefix debug.,$(XObjectList))
XObsoleteObjects = $(addprefix obs.,$(XObjectList))
XModuleObjects = $(addprefix oz.,$(XObjectList))

# Final targets:
all: @.debug.$(LibName)Lib $(XDebugObjects) \
     @.o.$(LibName)Lib $(XReleaseObjects) \
     @.obs.$(LibName)Lib $(XObsoleteObjects) \
     @.oz.$(LibName)Lib $(XModuleObjects)

@.o.$(LibName)Lib: $(ReleaseObjects)
	$(LibFile) $(LibFileFlags) $(ReleaseObjects)

@.debug.$(LibName)Lib: $(DebugObjects)
	$(LibFile) $(LibFileFlags) $(DebugObjects)

@.obs.$(LibName)Lib: $(ObsoleteObjects)
	$(LibFile) $(LibFileFlags) $(ObsoleteObjects)

@.oz.$(LibName)Lib: $(ModuleObjects)
	$(LibFile) $(LibFileFlags) $(ModuleObjects)

# User-editable dependencies:
.SUFFIXES: .o .c .debug .s .oz .obs
.c.o:; ${CC} $(CCFlags) $<
.c.oz:; ${CC} $(CCModuleFlags) $<
.c.obs:; ${CC} $(CCObsoleteFlags) $<
.c.debug:; ${CC} $(CCDebugFlags) $<
.s.o .s.obs .s.oz:; ${AS} $(ASFlags) $<
.s.debug:; ${AS} $(ASDebugFlags) $<

# Dynamic dependencies:
