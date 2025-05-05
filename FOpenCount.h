/*
 * CBLibrary: Maintain a count of the number of open files
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

/* FopenCount.h declares three functions that provide a veneer to the <stdio.h>
   fopen and fclose functions in order to keep an accessible count of the
   number of open streams.

Dependencies: ANSI C library.
Message tokens: None.
History:
  CJB: 05-Nov-04: Added clib-style documentation and dependency information.
  CJB: 05-Mar-05: Updated documentation on fclose_dec.
  CJB: 11-Dec-20: Deleted redundant uses of the 'extern' keyword.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
*/

#ifndef FopenCount_h
#define FopenCount_h

/* ISO library headers */
#include <stdio.h>

#if !defined(USE_OPTIONAL) && !defined(_Optional)
#define _Optional
#endif

_Optional FILE *fopen_inc(const char * /*filename*/, const char * /*mode*/);
   /*
    * Opens the file whose name is the string pointed to by 'filename', and
    * associates a stream with it. This function is a direct replacement for
    * fopen.
    * Returns: a pointer to the object controlling the stream. If the open
    *          operation fails, fopen returns a null pointer.
    */


int fclose_dec(FILE * /*stream*/);
   /*
    * Causes the stream pointed to by 'stream' to be flushed and the associated
    * file to be closed. This function is a direct replacement for fclose. May
    * cause abnormal program termination if called more times than fopen_inc.
    * Returns: zero if the stream was successfully closed, or nonzero if any
    *          errors were detected or if the stream was already closed.
    */

unsigned int fopen_num(void);
   /*
    * Gives access to an internal count of the number of open streams.
    * Returns: the number of streams opened by fopen_inc and not yet closed by
    *          fclose_dec.
    */

#endif
