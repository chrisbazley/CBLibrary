/*
 * CBLibrary: Veneers to star commands for Fednet game file (de)compression
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

/* FednetComp.h declares two functions that provide a veneer to the CLI
   commands *CSave and *CLoad (provided by the relocatable modules 'Comp' and
   'DeComp', respectively). This module is DEPRECATED in favour of FedCompMT.

Dependencies: ANSI C library, Acorn library kernel, Acorn's flex library.
Message tokens: NoMem, OpenInFail, ReadFail.
History:
  CJB: 04-Nov-04: Added 'const' qualifier to char * function arguments.
                  Added clib-style documentation and dependency information.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 27-Oct-06: Minor changes to documentation.
  CJB: 15-Oct-09: Added "NoMem" to list of required message tokens. Marked this
                  module as deprecated.
  CJB: 26-Jun-10: Made compilation of this file conditional upon definition of
                  CBLIB_OBSOLETE.
  CJB: 11-Dec-20: Deleted redundant uses of the 'extern' keyword.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
*/

#ifndef FednetComp_h
#define FednetComp_h

#ifdef CBLIB_OBSOLETE /* Use c.FedCompMT instead */

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "flex.h"

/* Local headers */
#include "Macros.h"

#if !defined(USE_OPTIONAL) && !defined(_Optional)
#define _Optional
#endif

_Optional CONST _kernel_oserror *load_compressed(const char * /*file_path*/, flex_ptr /*buffer_anchor*/);
   /*
    * Allocates a flex block of sufficient size for the compressed data held in
    * file 'file_path' (writing the base address to 'buffer_anchor') and then
    * invokes *Cload to load the data.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

_Optional CONST _kernel_oserror *save_compressed(const char * /*file_path*/, int /*file_type*/, flex_ptr /*buffer_anchor*/);
   /*
    * Invokes *CSave to save the entire contents of flex block 'buffer_anchor'
    * to the location specified by 'file_path' and then sets the RISC OS file
    * type to 'file_type'.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

#else /* CBLIB_OBSOLETE */
#error Header file FednetComp.h is deprecated
#endif /* CBLIB_OBSOLETE */

#endif
