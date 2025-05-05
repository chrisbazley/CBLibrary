/*
 * CBLibrary: Interruptible implementation of file loading and saving
 * Copyright (C) 2003 Christopher Bazley
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* LoadSaveMT.h declares three functions that enable a program to load or save
   data in 'chunks', halting when a specified volatile boolean flag has changed
   value. In conjunction with the library's 'Timer' module, this allows multi-
   tasking file operations to be implemented.

Dependencies: ANSI C library, Acorn library kernel, Acorn's flex library.
Message tokens: NoMem, OpenInFail, ReadFail, OpenOutFail, WriteFail.
History:
  CJB: 05-Nov-04: Added clib-style documentation and dependency information.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 17-Oct-06: Added declarations of new functions load_fileM2() and
                  save_fileM2(). Marked old functions load_fileM() and
                  save_fileM() as obsolete.
  CJB: 27-Oct-06: Minor changes to documentation.
  CJB: 13-Oct-09: Added prototype of a new function, loadsave_initialise, which
                  should be called to initialise this module.
  CJB: 15-Oct-09: Added "NoMem" to list of required message tokens.
  CJB: 11-Dec-20: Deleted redundant uses of the 'extern' keyword.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
*/

#ifndef LoadSaveMT_h
#define LoadSaveMT_h

/* ISO library headers */
#include <stdbool.h>
#include <stdio.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "flex.h"
#include "toolbox.h"

/* Local headers */
#include "Macros.h"

#if !defined(USE_OPTIONAL) && !defined(_Optional)
#define _Optional
#endif

_Optional CONST _kernel_oserror *loadsave_initialise(_Optional MessagesFD */*mfd*/);
   /*
    * Initialises the LoadSaveMT module. Unless 'mfd' is a null pointer, the
    * specified messages file will be given priority over the global messages
    * file when looking up text required by this module.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

unsigned int get_loadsave_perc(FILE *_Optional ** /*handle*/);
   /*
    * Calculates what proportion of a file operation has been completed and
    * returns this as a percentage value. 'handle' must be the same pointer
    * that is passed to load_fileM or save_fileM each time.
    * Returns: the percentage done of the specified file operation.
    */

_Optional CONST _kernel_oserror *load_fileM2(const char * /*file_path*/,
                                             flex_ptr /*buffer_anchor*/,
                                             const volatile bool * /*time_up*/,
                                             FILE *_Optional ** /*handle*/);
   /*
    * Loads data from the specified file 'file_path' into a new flex block
    * anchored at 'buffer_anchor', returning when the variable pointed to by
    * 'time_up' is found to be true and at least some data has been read from
    * the file. The first time you call this function, the FILE ** pointer
    * pointed to by 'handle' should be NULL. Completion will be signified by
    * it returning to that state.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

_Optional CONST _kernel_oserror *save_fileM2(const char * /*file_path*/,
   flex_ptr /*buffer_anchor*/, const volatile bool * /*time_up*/,
   unsigned int /*start_offset*/, unsigned int /*end_offset*/,
   FILE *_Optional ** /*handle*/);
   /*
    * Saves an area of flex block 'buffer_anchor' that is delimited by
    * 'start_offset' (inclusive) and 'end_offset' (exclusive). This data is
    * saved to the file specified by 'file_path', returning when the variable
    * pointed to by 'time_up' is found to be true and at least some data has
    * been written to the file. The first time you call this function, the
    * FILE ** pointer pointed to by 'handle' should be NULL. Completion will
    * be signified by it returning to that state.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

_Optional CONST _kernel_oserror *load_fileM(const char * /*file_path*/,
   flex_ptr /*buffer_anchor*/, const volatile bool * /*time_up*/,
   FILE *_Optional ** /*handle*/, bool /*sprite*/);
   /*
    * This function is deprecated - you should use 'load_fileM2' instead.
    */

_Optional CONST _kernel_oserror *save_fileM(const char * /*file_path*/,
   int /*file_type*/, flex_ptr /*buffer_anchor*/, const volatile bool * /*time_up*/,
   FILE *_Optional ** /*handle*/, bool /*sprite*/);
   /*
    * This function is deprecated - you should use 'save_fileM2' instead.
    */

#endif
