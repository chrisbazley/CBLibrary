/*
 * CBLibrary: RISC OS file utilities
 * Copyright (C) 2009 Christopher Bazley
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

/* FileUtils.h declares functions useful for manipulating files on RISC OS.

Dependencies: Acorn library kernel.
Message tokens: None.
History:
  CJB: 13-Jun-04: Created this header by copying the canonicalisation function
                  declaration out of h.Loader.
  CJB: 31-Oct-04: Shortened argument names and added clib-style documentation.
  CJB: 04-Nov-04: Added dependency information.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 06-Sep-09: Renamed this header as "FileUtils.h" and added a prototype of
                  set_file_type function.
  CJB: 13-Oct-09: Rewrote explanation of the canonicalise function with
                  reference to new library function os_fscontrol_canonicalise.
  CJB: 08-Nov-14: Added a declaration of the make_path function.
  CJB: 10-Nov-19: Added a declaration of the get_file_size function.
  CJB: 11-Dec-20: Deleted redundant uses of the 'extern' keyword.
  CJB: 31-May-21: Added a declaration of the get_file_type function.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
*/

#ifndef FileUtils_h
#define FileUtils_h

/* Acorn C/C++ library headers */
#include "kernel.h"

/* Local headers */
#include "Macros.h"

#if !defined(USE_OPTIONAL) && !defined(_Optional)
#define _Optional
#endif

_Optional CONST _kernel_oserror *canonicalise(_Optional char ** /*b*/, _Optional const char * /*pv*/, _Optional const char * /*ps*/, const char * /*f*/);
   /*
    * Wrapper for os_fscontrol_canonicalise which uses malloc to allocate a
    * buffer of the right size for the canonicalised path and writes a pointer
    * to this buffer out through 'b', which must never be null. It is the
    * caller's responsibility to free the heap block when no longer required.
    * Returns an error if there was insufficient free memory for the buffer.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

_Optional CONST _kernel_oserror *set_file_type(const char */*f*/, int /*type*/);
   /*
    * Sets the type of a specified file to indicate its contents (e.g. 0xfff
    * means text, whereas 0xfaf means HTML).
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

_Optional CONST _kernel_oserror *get_file_type(const char */*f*/, int * /*type*/);
   /*
    * Gets the type of a specified file.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

_Optional CONST _kernel_oserror *get_file_size(const char */*f*/, int * /*size*/);
   /*
    * Gets the size of a specified file, in bytes. Returns an error if the
    * object doesn't exist or is not a file.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

_Optional CONST _kernel_oserror *make_path(char * /*f*/, size_t /*offset*/);
   /*
    * Searches forwards through the path string pointed to by 'f', starting at
    * the given 'offset'. Whenever a file path separator character (in RISC OS
    * a full stop) is encountered, the preceding path element is created as a
    * directory with the default no. of entries. Pass offset=0 to create all
    * directories in the path. It is not an error if directories already exist.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

#endif
