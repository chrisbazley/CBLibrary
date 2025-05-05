# CBLibrary
(C) 2003 Christopher Bazley

Release 64 (05 May 2025)

Preamble
--------
  This is yet another C library for RISC OS. Actually, it is more like a
personal collection of modules that happen to be common to many of my
applications. It has a large number of external dependencies, including most
of the libraries supplied with the Acorn C/C++ package.

  I do not seriously expect many (any?) other programmers to use CBLibrary,
but it has to be in the public domain in order that the programs that use it
can meaningfully be released under the GNU General Public Licence.

  The most interesting modules are probably 'Drag', 'Entity2', 'Loader3',
'Saver2', 'Scheduler', 'UserData' and 'DirIter'. The first four provide a
complete implementation of the data transfer, drag and drop, and clipboard
protocols described in the RISC OS 3 PRM and application notes 240 and 241.
The 'Scheduler' module manages null events and schedules functions to be
called back at timed intervals. The 'DirIter' module is a nicer way of
traversing the objects in a directory tree. The 'UserData' module
implements a list of user data (such as documents in a word processor), and
provides methods to enumerate unsaved data or find data by file name.

  As supplied the library is built with the symbol NDEBUG defined. Read the
section on 'Error propagation' if you are concerned about how this affects
usage.

Removed functions
-----------------
  Originally CBLibrary provided an esoteric set of functions and macros that
were unrelated apart from having been used in one or more programs I had
written. Some of the functions were portable and some were not; some
depended on Acorn C/C++ libraries and some did not. Consequently I began
splitting it into smaller libraries with clearly-defined portability
requirements and dependencies.

  If attempting to link some software with CBLibrary gives errors like the
following then it is probably because it depends on functions that have
since been moved to other libraries:
```
main.o: In function `main':
main.c:(.text.startup+0x214): undefined reference to `stringbuffer_init'
main.c:(.text.startup+0x268): undefined reference to `stringbuffer_destroy'
main.c:(.text.startup+0x330): undefined reference to `is_switch'
main.c:(.text.startup+0x398): undefined reference to `stringbuffer_append'
main.c:(.text.startup+0x3b8): undefined reference to `stringbuffer_append_separated'
main.c:(.text.startup+0x3d0): undefined reference to `stringbuffer_get_pointer'
```

  The following libraries are derived from CBLibrary and provide
functionality that was previously part of it:

 * SF3KLib (conversion between RISC OS sprites and game bitmaps)
 * StreamLib (abstract interface to raw and compressed files)
 * GKeyLib (portable game file compression/decompression)
 * CBUtilLib (miscellaneous portable functions)
 * CBOSLib (RISC OS-specific SWI veneers)
 * CBDebugLib (RISC OS-specific debugging facilities)

  Software that depends on CBLibrary should be modified to link with one
or more of the libraries cited above.

The 'bool' issue
----------------
  As supplied, CBLibrary expects the type 'bool' to be a byte, since this is
how it is implemented in Castle's C99 compiler. If you have an older
compiler then it is likely that your standard C library's 'stdbool' header
(if you have one) typedefs 'bool' as 'int'. If this is the case you then
will have to rebuild the whole library. This is necessary because various
modules have pointers to type 'bool' in their functional interfaces.

Fortified memory allocation
---------------------------
  I use Simon's P. Bullen's fortified memory allocation shell 'Fortify' to
find memory leaks in my applications, detect corruption of the heap
(e.g. caused by writing beyond the end of a heap block), and do stress
testing (by causing some memory allocations to fail). Fortify is available
separately from this web site:
http://web.archive.org/web/20020615230941/www.geocities.com/SiliconValley/Horizon/8596/fortify.html

  By default, Fortify only intercepts the ANSI standard memory allocation
functions (e.g. 'malloc', 'free' and 'realloc'). This limits its usefulness
as a debugging tool if your program also uses different memory allocator such
as Acorn's 'flex' library.

  The debugging version of CBLibrary must be linked with 'Fortify', for
example by adding 'C:o.Fortify' to the list of object files specified to the
linker. Otherwise, you will get build-time errors like this:
```
ARM Linker: (Error) Undefined symbol(s).
ARM Linker:     Fortify_malloc, referred to from C:debug.CBLib(Pal256).
ARM Linker:     Fortify_free, referred to from C:debug.CBLib(Pal256).
ARM Linker:     Fortify_calloc, referred to from C:debug.CBLib(Drag).
```
  It is important that Fortify is also enabled when compiling code to be
linked with the debugging version of CBLibrary. This means #including the
"Fortify.h" and "PseudoFlex.h" headers in each of your source files, and
pre-defining the C pre-processor symbol 'FORTIFY'. If you are using the Acorn
C compiler then this can be done by adding '-DFORTIFY' to the command line.

  Linking unfortified programs with the debugging version of CBLibrary will
cause run time errors when your program tries to reallocate or free a heap
block allocated within CBLibrary, or CBLibrary tries to reallocate or free a
block allocated by your program. Typically this manifests as 'Flex memory
error' or 'Unrecoverable error in run time system: free failed, (heap
overwritten)'.

Error propagation
-----------------
  Most CBlibrary functions that may fail due to a run-time error are declared
as returning a pointer to a _kernel_oserror struct (defined in the Acorn
library header "kernel.h"). This mechanism is used to propagate both OS
errors (e.g. from SWIs) and recoverable errors such as lack of memory. In
the latter case SWI MessageTrans_ErrorLookup is used, so the returned
pointer will be to one of MessageTrans's internal buffers.

  Having called such library functions you should check to see whether an
error occurred (i.e. non-NULL pointer returned), and if so take appropriate
action such as alerting the user and/or stopping your program. If you wish
to retain an error then you must copy the buffer as soon as possible.

  Another class of error is caused by misuse of CBLibrary functions, usually
because of bugs in a client program. An example would be attempting to
remove a non-existent entry from the list maintained by the ViewsMenu
module. The standard library function assert() is used to guard against
such misuse and will (as long as CBLibrary was not compiled with NDEBUG)
cause the program to be terminated with a message. For this reason it is not
recommended that NDEBUG versions of CBLibrary be used during program
development.

Obsolete functions
------------------
  Because CBLibrary has been continuously adapted to my needs over a period
of several years, various functions and interfaces have been replaced or
deprecated. Usually these are clearly marked in the relevant header files
and will not be included in the library unless pre-processor symbol
'CBLIB_OBSOLETE' is defined. Deprecated features are listed below.

All of the macros defined by "Deprecated.h":
  Obfuscatory synonyms, except CLONE_STR which is superseded by de-facto
  standard function 'strdup'.

Function 'perc_operation':
  Has an excessive number of arguments, some of which are irrelevant to
  load operations; use 'file_perc_save' or 'file_perc_load' as appropriate.

Function 'save_compressedM':
  Superseded by 'save_compressedM2, which allows more accurate specification
  of the area of memory to save.

Function 'load_fileM':
  Special provision for sprite areas is unnecessary; use 'load_fileM2' and
  then 'flex_midextend' if desired.

Function 'save_fileM':
  Special provision for sprite areas is not as flexible as the delimiters
  used by 'save_fileM2', and the file type can be set afterwards if desired.

All of the functions declared by "Loader.h":
  This module has been superseded by 'Loader2', which is more flexible.

Function 'ViewsMenu_strcmp_nc':
  Almost identical to de-facto standard function 'stricmp'.

Function 'ViewsMenu_show_object':
  Identical to function 'DeIconise_show_object'.

Function 'ViewMenu_getfirst':
  Renamed as 'ViewsMenu_getfirst'.

Function 'ViewMenu_getnext':
  Renamed as 'ViewsMenu_getnext'.

All of the functions declared by "RoundRobin.h":
  This module has been superseded by 'Scheduler', which provides similar
  but more advanced functionality.

Function 'msgs_lookupsub' and associated macros:
  Made obsolete by function 'msgs_lookup_subn', which takes a variable
  number of arguments.

All of the functions declared by "NullPoll.h":
  This has been superseded by 'Scheduler', which manipulates the event mask
  as part of an integrated system.

Header file "StrCaseIns.h":
  Redirects to header file "StrExtra.h".

Header files "ErrNotRec.h" and "Err_Rec.h":
  Redirect to 'header file "Err.h". It is the responsibility of the
  programmer to abstain from using functions 'err_suppress_errors' and
  'err_dump_suppressed' if linking with o.ErrNotRec.

All of the functions declared by "FednetComp.h":
  This module has been superseded by 'FedCompMT', which doesn't rely on
  star commands.

Function 'Pal256_colour_brightness':
  Identical to function 'palette_entry_brightness'.

Functions 'msgs_global' and 'msgs_globalsub':
  If search really needs to be restricted to the global messages file then
  use messagetrans_lookup directly.

Functions 'msgs_lookupsubn' and 'msgs_errorsubn':
  Renamed as 'msgs_lookup_subn' and 'msgs_error_subn'.

Function msgs_get_descriptor:
  Relies on the first caller initialising the returned descriptor, or else
  future calls to MsgTrans functions will use an invalid descriptor. Use
  msgs_initialise instead (after having successfully opened a messages file).

Why is the 'Loader' module deprecated?
--------------------------------------
  It may seem like a strange decision to deprecate the 'Loader' module,
since it was originally the most powerful module in CBLibrary. It is also the
only part to have separate documentation (because I was so proud of it).

  The most important functionality of 'Loader' (the implementation of the
receiver's half of the data transfer protocol) has been moved to a new
module 'Loader2'. All that remains is a monolithic mechanism for handling
DataSave, DataLoad and DataOpen messages on behalf of the client program. I
find that my programs are smaller and their source code more readable
without it.

  When I designed 'Loader', I didn't properly understand the drag-and-drop
and clipboard protocols. Both involve a pre-amble to the basic data transfer
protocol, which requires the client program to register its own message
handler in which to perform certain actions before a DataSave message is
processed by 'Loader'. In this situation, the client might just as well
invoke 'Loader2' directly.

  'Loader' cannot automatically claim drags on behalf of a client because
each 'listener' is associated with just one file type (or a filter function,
which can only be queried for specific file types). A drag claim must
include a list of file types in order of preference. It would be difficult,
if not impossible, to create such a list from the registered 'listeners'.

  It is often useful to treat DataLoad and DataOpen messages differently
from DataSave, but that is anathematic to the original design of 'Loader'.
Many programs check the file path before loading a file, in case a copy is
already being edited. 'LoaderPreFilter' functions (which have many arcane
return values) were my attempt to get around this problem. They contain code
which would otherwise be in a DataLoad message handler.

Rebuilding the library
----------------------
  You should ensure that the Acorn C/C++ library directories clib, tboxlibs
and flexlib are on your C$Path, otherwise the compiler won't be able to find
the required header files. You must also ensure that GKeyLib, CBUtilLib,
CBOSLib and CBDebugLib (by the same author as CBLibrary) are on your C$Path.
The dependency on CBDebugLib isn't very strong: it can be eliminated by
modifying the make file so that the macro USE_CBDEBUG is no longer
predefined.

  Ensure that the macros DIR_SEPARATOR and EXT_SEPARATOR are defined
appropriately for the target platform's file system (e.g. '\\' and '.' on
Windows).

  Two make files are supplied:

- 'Makefile' is intended for use with Acorn Make Utility (AMU) and the
   Norcroft C compiler supplied with the Acorn C/C++ Development Suite.
- 'GMakefile' is intended for use with GNU Make and the GNU C Compiler.

  These make files share some variable definitions (lists of objects to be
built) by including a common make file.

  The APCS variant specified for the Norcroft compiler is 32 bit for
compatibility with ARMv5 and fpe2 for compatibility with older versions of
the floating point emulator. Generation of unaligned data loads/stores is
disabled for compatibility with ARM v6.

  The suffix rules put object files in one of several directories, depending
on the compiler options used to compile them:

o: Assertions and debugging output are disabled. The code is optimised for
   execution speed.

oz: Assertions and debugging output are disabled. The code is suitable for
    inclusion in a relocatable module (multiple instantiation of static
    data and stack limit checking disabled). When the Norcroft compiler is
    used, the compiler optimises for smaller code size. (The equivalent GCC
    option seems to be broken.) The macro INCLUDE_FINALISATION_CODE is
    pre-defined; this forces inclusion of library functions that deallocate
    memory on finalisation (required if the heap is in RMA).

debug: Assertions and debugging output are enabled. The code includes
       symbolic debugging data (e.g. for use with DDT). The macro FORTIFY
       is pre-defined to enable Simon P. Bullen's fortified shell for memory
       allocations.

obs: Identical to suffix 'o' except that macro CBLIB_OBSOLETE is pre-defined
     to force inclusion of obsolete library functions. This may allow some
     older programs to be linked without modification.

d: 'GMakefile' passes '-MMD' when invoking gcc so that dynamic dependencies
   are generated from the #include commands in each source file and output
   to a temporary file in the directory named 'd'. GNU Make cannot
   understand rules that contain RISC OS paths such as /C:Macros.h as
   prerequisites, so 'sed', a stream editor, is used to strip those rules
   when copying the temporary file to the final dependencies file.

  The above suffixes must be specified in various system variables which
control filename suffix translation on RISC OS, including at least
UnixEnv$ar$sfix, UnixEnv$as$sfix, UnixEnv$gcc$sfix and UnixEnv$make$sfix.
Unfortunately GNU Make doesn't apply suffix rules to make object files in
subdirectories referenced by path even if the directory name is in
UnixEnv$make$sfix, which is why 'GMakefile' uses the built-in function
addsuffix instead of addprefix to construct lists of the objects to be
built (e.g. foo.o instead of o.foo).

Before compiling the library for RISC OS, move the C source and header
and assembly language files with .c, .h and .s suffixes into subdirectories
named 'c', 'h' and 's' and remove those suffixes from their names. You
probably also need to create 'o', 'oz', 'obs', 'd' and 'debug' subdirectories
for compiler output.

Licence and disclaimer
----------------------
  This library is free software; you can redistribute it and/or modify it
under the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version.

  This library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
for more details.

  You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation,
Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

Credits
-------
  Most of CBLibrary was written by Christopher Bazley.

  The 'Err' and 'MsgTrans' modules are derived from those supplied with
Tony Houghton's tutorial application !FormText.

  The implementations of functions strdup(), stricmp() and strnicmp() were
copied from UnixLib, which is copyright (c) 1995-1999 Simon Callan, Nick
Burrett, Nicholas Clark and Peter Burwood. Portions are (c) The Regents of
the University of California, portions are (c) Sun Microsystems, Inc. and
portions are derived from the GNU C Library and fall under the GNU Library
General Public License.

  I would like to thank Justin Fletcher, Pete Miller and Matthew Phillips for
offering advice on implementation of the drag and drop protocol.

History
-------

Release 50 (18 Feb 2016)
- Fixed an error in the definition of macro _kernel_swi_c which referred
  to a non-existent argument 'carr'.
- Substituted _kernel_swi for _swix because it's easier to intercept for
  stress testing.
- The "ErrButtons" message is now looked up on initialization.
- Increased the fread/fwrite granularity from 64 bytes to 4 KB in the
  load_fileM2 and save_fileM2 functions.
- Increased the size of the buffer used to record data passed to the
  saveas_buffer_filled function for unit tests from 256 bytes to 128 KB.
- Intercepted the wimp_set_colour function to allow error simulation.
- Extra debugging output relating to entity claim message broadcasts and
  when there was no callback from linkedlist_for_each.
- Recategorized all debugging output from the nobudge functions as
  'verbose'.
- Combined all makefiles into one by using $(addprefix) and filename
  suffix-based rules.

Release 51 (02 May 2016)
- Fixed a read outside array bounds in the "Is member of another list" test.
- Fixed illegal modification of string literals in the tests of make_path.
- Created an alternative makefile for use with GNU Make and the GNU C
  compiler.
- Defined a new macro CHECK_PRINTF, for use when declaring printf-like
  functions. It tells the GNU C compiler to check the argument values passed
  to the debugging output functions.
- Fixed a bad format string in userdata_destroy (a StringBuffer pointer
  was misused as a char pointer).
- Deleted excess field specifiers from a bad format string in
  scheduler_initialise.
- Modified other format strings to avoid GNU C compiler warnings. Substituted
  format specifier %zu for %lu in some previously-valid strings to avoid the
  need to cast the matching parameters. Function addresses are no longer
  printed because there is no matching format specifier.
- Result of strlen is no longer wrongly treated as signed and a -ve return
  value from vsprintf is now handled differently from buffer overflow to
  avoid GNU C compiler warnings in debug_vprintf.
- Added one function to allocate a ring buffer of a specified size and
  another to deallocate a buffer. These were needed because it isn't
  strictly legal to embed a struct with a flexible array member in another
  struct.
- Changed the struct member used to store the log size of a ring buffer to a
  narrower type (char instead of unsigned int). Added a copy of this value
  to struct GKeyDecomp because the ring buffer is now separate.
- Modified a loop in _ldr_check_dropzone to avoid GNU C compiler warnings
  about unsigned comparisons with signed integers. Used size_t for this and
  other loop counters to match type of ARRAY_SIZE.
- Added missing curly braces and initializers to a WimpMessage declaration,
  and missing initializers to a WindowShowObjectBlock declaration.
- Added checks for the count of sprites or tiles being -ve in
  sf_spr_to_planets, sf_spr_to_sky, or sf_spr_to_tiles and if not then
  cast them to unsigned to avoid GNU C compiler warnings.
- sf_planets_to_spr now calls sf_planets_to_lone_spr to get the expected
  sprite size instead of using an independent numeric constant. Similar for
  sf_sky_to_spr and sf_sky_to_lone_spr, sf_tiles_to_spr and
  sf_tiles_to_lone_spr.
- Added brackets in the definitions of WORD_ALIGN and set_gadget_hidden to
  avoid GNU C compiler warnings.
- The return values of sprintf and _kernel_osfile are now explicitly ignored
  to avoid GNU C compiler warnings.
- Deleted redundant type casts in csv_parse_string.
- Intercepted the gadget_get_bbox function to allow error simulation.

Release 52 (02 Jun 2016)
- Fixed null pointer dereferences when calling the new ring buffer
  destructor function in gkeycomp_destroy and gkeydecomp_destroy, if those
  functions are called with null.
- Cast pointer parameters to void * to match %p in FileRWInt.c.
- Created a new function named os_file_set_type to which calls to
  set_file_type are now delegated.
- Can now simulate failure of malloc in the RingBuffer_make function.
- Rewrote the timer_register and timer_deregister functions in C.
- The GNU Make file now specifies asasm rather than objasm as the assembler.
- Deleted makefile hacks previously used to successfully assemble the OS
  ticker event callback routine; suffix rules work correctly when using the
  appropriate assembler for the toolchain!
- Changed the order in which library search paths are specified when linking
  the unit tests to ensure that ^.debug.libCB/a takes precedence over
  C:libCB/a.
- Wrote unit tests for the timer functions and a few very simple ones for
  the ring buffer and compressor/decompressor modules.
- Added a test for calling diriterator_destroy with null and documented its
  behaviour.

Release 53 (07 Jun 2016)
- Prevented interception of _kernel_swi for error simulation in
  timer_deregister because it's dangerous to interfere with ticker event
  deregistration.

Release 54 (18 Jul 2017)
- Implemented support for generating dynamic dependencies with gcc.
- Intercepted the fputs, fprintf, fgetc and fputc functions to allow error
  simulation.

Release 55 (19 Aug 2018)
- New command-line argument parser helper functions.
- New abstract file reader interface with implementations for raw files
  and files compressed with Gordon Key's algorithm.
- Added a new function, stringbuffer_append_separated, for use when
  appending file names to a path or extensions to a file name.
- Created a header file to define platform-specific file path component
  and file name extension separator characters.
- Made debugging output even less verbose by default.
- Moved the library name into the common makefile.
- Grouped the many library objects into categories (in preparation for a
  possible split).

Release 56 (04 Nov 2018)
- Deleted code that was only compiled when the OLD_SCL_STUBS macro had NOT
  been pre-defined. The associated 'stubsg_o' output directory has also
  been deleted. All build configurations are now expected to be compatible
  with RISCOS Ltd's generic C library stubs (which do not support C99).
- Removed code and related tests for functionality that has been moved to
  new individual libraries: SF3KLib, StreamLib, GKeyLib, CBUtilLib, CBOSLib
  and CBDebugLib.
- Moved a lot of ancient history from here to a different text file.
- The "FOpPrivate.h" header file has been moved to an 'Internal' directory
  to discourage use by clients of the library.
- Converted all remaining invocations of DEBUG to DEBUGF because it's
  easier to substitute a printf-like function when the developer doesn't
  want to depend on CBDebugLib.
- Consolidated all of the #include commands for debugging facilities into
  one header file, "Internal/CBMisc.h". The dependency on CBDebugLib can
  be removed by not pre-defining a macro, USE_CBDEBUG.

Release 57 (03 Nov 2019)
- Created new versions of the 'Entity', 'Saver' and 'Loader2' modules to
  integrate better with programs that use StreamLib.
- Added a macro implementation of CONTAINER_OF for use with member pointers.
- nobudge_register() no longer tries to allocate memory from the heap if
  flex budge is already disabled or 0 bytes of headroom was requested.
- Modified existing code to use stringbuffer_append_all where appropriate.
- Moved code dealing with -1 terminated arrays of filetypes and reading the
  OS monotonic timer to CBOSLib.
- Less verbose debugging output by default.

Release 58 (12 Nov 2019)
- The leafname is now passed instead of "<Wimp$Scrap>" when calling a
  function of type Loader3ReadMethod or Entity2ReadMethod. The estimated
  file size in bytes is now passed as an extra argument.
- Added a new function to get the size of a specified file and used it
  where appropriate.
- Rewrote load_compressedM() and save_compressedM2() to use StreamLib
  instead of calling GKeyLib functions directly.
- No longer neglect to check the return value of fclose_dec() (with possible
  loss of buffered user data) in save_compressedM2(), save_fileM2() and in
  DataSaveAck message handlers.
- Allocate RAM transfer buffers one byte longer than requested to try to
  avoid having to send a second RAMFetch message.

Release 59 (30 Sep 2020)
- Deleted redundant static function pre-declarations.
- Fixed a misleading debugging message in Loader3.c.
- Fixed missing/misplaced casts in assertions in FedCompMT.c.
- Made debugging output less verbose by default in FedCompMT.c.
- Fixed null pointers instead of strings passed to DEBUGF in canonicalise().

Release 60 (29 Oct 2020)
- The file operation destructor set up by load_compressedM() and
  save_compressedM2() requires the file handle to be open (at least since
  r58). Previously the file was closed by abort_file_op() and then again in
  the destructor.

Release 61 (02 Dec 2020)
- Assign compound literals to ensure initialization of all data when
  claiming an entity, or when initializing a load or save operation.
- Added the loader3_load_file function() to make it easy for applications'
  DataOpen and DataLoad message handlers to reuse the same functions they
  provide for participating in the data transfer message protocol.
- Clarified the debug messages from err_dump_suppressed().

Release 62 (30 Jul 2022)
- Added a function to remove any Toolbox and Wimp event handlers that have
  been registered for an object and then delete it.
- Added a function to get the RISC OS type of a file.
- Allow initialisation of a 256 colour selection dialogue box with a 'const'
  palette array.
- Skip scheduler clients for which removal is already pending in
  _scheduler_client_has_callback(). This allows use of scheduler_deregister()
  followed by scheduler_register() (for the same client) in a
  SchedulerIdleFunction.
- Release entities (e.g. global clipboard) upon exit triggered by
  entity2_dispose_all() to avoid leaks if the client doesn't call
  entity2_finalise().
- Deleted a bad assertion in notify_closed() function of the deiconizer
  (which failed for unknown UI object classes, including Menu).
- Fixed debug output of an uninitialized bounding box in
  _drag_send_dragging_msg() if the client passed a bounding box.
- Added debugging output from set_gadget_faded().
- Redefined the macro err_check_fatal() and the function err_check() as
  inline functions.
- Redefined the ON_ERR_RPT() macro as a simple call to err_check() that
  discards its result, since err_check() is now defined inline.
- Refactored err_complain(), err_report() and err_check_rep() to share
  instead of duplicating code to handle error suppression/recording.
- Rewrote fancy_error() to use an internal buffer allocated by
  messagetrans_error_lookup() instead of by its caller (for simpler usage).
- Optimise a comparison with the empty string in err_dump_suppressed().
- Deleted redundant uses of the 'extern' keyword from headers.
- Prefer to declare variables with an initializer.
- Assign compound literals to ensure assignment of all members when
  releasing an entity or registering a new scheduler client.
- Use CONTAINER_OF in scheduler code instead of assuming struct layout.
- Shuffle the function definition order in the error-reporting module to
  avoid the need for pre-declarations.

Release 63 (17 Jun 2023)
- Use size_t rather than unsigned int for the number of substitution
  parameters passed to msg functions.

Release 64 (05 May 2025)
- Fix #include filename case.
- Fix pedantic warnings about format specifying type void *.
- Changed the pathtail depth count type from int to size_t.

Contact details
---------------
Christopher Bazley

Email: mailto:cs99cjb@gmail.com

WWW:   http://starfighter.acornarcade.com/mysite/
