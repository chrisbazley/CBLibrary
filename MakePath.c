/*
 * CBLibrary: Create directories in a file path
 * Copyright (C) 2014 Christopher Bazley
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
  CJB: 08-Nov-14: Created this file.
  CJB: 07-Dec-14: Ensure string is restored upon error.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 07-Aug-18: Modified to use PATH_SEPARATOR from "Platform.h".
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
 */

/* ISO library headers */
#include <string.h>

/* CBOSLib headers */
#include "OSFile.h"

/* Local headers */
#include "Platform.h"
#include "FileUtils.h"
#include "Internal/CBMisc.h"

/* ----------------------------------------------------------------------- */
/*                         Public functions                                */

_Optional CONST _kernel_oserror *make_path(char *f, size_t offset)
{
  _Optional char *end;
  _Optional CONST _kernel_oserror *e = NULL;

  assert(f != NULL);
  assert(offset <= strlen(f));

  /* Find each path separator in turn */
  for (end = strchr(f + offset, PATH_SEPARATOR);
       e == NULL && end != NULL;
       end = strchr(&*end + 1, PATH_SEPARATOR))
  {
    *end = '\0'; /* Slice string at path separator */
    e = os_file_create_dir(f, OS_File_CreateDir_DefaultNoOfEntries);
    *end = PATH_SEPARATOR; /* Restore path separator */
  }
  return e;
}
