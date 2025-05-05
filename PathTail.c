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

/* History:
  CJB: 31-Oct-04: Copied and renamed function tail() from source of SFeditor.
  CJB: 22-Jun-09: Whitespace changes only.
  CJB: 07-Aug-18: Modified to use PATH_SEPARATOR from "Platform.h".
  CJB: 26-Oct-18: Modified to use strtail() from CBUtilLib.
  CJB: 01-Nov-18: Reimplemented using the strtail() function from CBUtilLib.
  CJB: 05-May-25: Changed the depth count type from int to size_t.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
 */

/* CBUtilLib headers */
#include "StrExtra.h"

/* Local headers */
#include "Platform.h"
#include "PathTail.h"
#include "Internal/CBMisc.h"

/* ----------------------------------------------------------------------- */
/*                         Public functions                                */

char *pathtail(const char *path, size_t n)
{
  return strtail(path, PATH_SEPARATOR, n >= 0 ? n : 0);
}
