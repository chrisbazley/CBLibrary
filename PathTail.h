/*
 * CBLibrary: Find a given number of elements at the tail of a file path
 * Copyright (C) 2004 Christopher Bazley
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

/* PathTail.h declares a function useful for subdividing file paths stored
   in character arrays.

Dependencies: ANSI C library.
Message tokens: None.
History:
  CJB: 31-Oct-04: Created this header.
  CJB: 04-Nov-04: Added dependency information.
  CJB: 11-Dec-20: Deleted redundant uses of the 'extern' keyword.
  CJB: 05-May-25: Changed the depth count type from int to size_t.
 */

#ifndef PathTail_h
#define PathTail_h

#include <stddef.h>

char *pathtail(const char * /*path*/, size_t /*n*/);
   /*
    * Searches backwards through the string pointed to by path, stopping when
    * it has found n instances of the file path separator character (in RISC OS
    * a full stop) or else when it reaches the start of the string.
    * Returns: a pointer to the character following the last path separator
    *          found, or else the value of path if it was found to contain less
    *          than n elements.
    */

#endif
