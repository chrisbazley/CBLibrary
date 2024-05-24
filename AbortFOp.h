/*
 * CBLibrary: Abort an interruptible file operation
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

/* AbortFOp.h declares a function that allows a file operation of the kind
   supported by the FedCompMT and LoadSaveMT components to be aborted cleanly.

Dependencies: ANSI C library.
Message tokens: None.
History:
  CJB: 05-Nov-04: Added clib-style documentation and dependency information.
  CJB: 11-Dec-20: Deleted redundant uses of the 'extern' keyword.
*/

#ifndef AbortFOp_h
#define AbortFOp_h

/* ISO library headers */
#include <stdio.h>

void abort_file_op(FILE *** /*handle*/);
   /*
    * Causes a stream to be flushed and the associated file to be closed, then
    * deallocation of the heap block that contained a pointer to this stream
    * and NULL to be written to the pointer to that heap block. So 'handle'
    * must point at a pointer to a heap block, the first content of which must
    * be a pointer to a <stdio.h> stream.
    */

#endif
