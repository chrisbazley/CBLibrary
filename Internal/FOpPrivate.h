/*
 * CBLibrary: Private type definitions for file operations
 * Copyright (C) 2011 Christopher Bazley
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

/* FOpPrivate.h declares private types that allow file operations to be
   aborted by the abort_file_op function. Do not include in client programs.

Dependencies: None
Message tokens: None.
History:
  CJB: 18-Dec-10: Created this header file.
  CJB: 03-Nov-19: Modified to use my streams library.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
*/

#ifndef FOpPrivate_h
#define FOpPrivate_h

/* ISO library headers */
#include <stdio.h>

/* StreamLib headers */
#include "Writer.h"
#include "Reader.h"

typedef void fileop_destructor(void *fop);

typedef struct fileop_common
{
  _Optional FILE              *f; /* Must be the first member */
  _Optional fileop_destructor *destructor;
  Reader                       reader;
  Writer                       writer;
  long int                     len;
}
fileop_common;

#endif
