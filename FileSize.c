/*
 * CBLibrary: Get the size of a file
 * Copyright (C) 2019 Christopher Bazley
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
  CJB: 10-Nov-19: Created this file.
 */

/* Acorn C/C++ library headers */
#include "kernel.h"

/* CBOSLib headers */
#include "OSFile.h"

/* Local headers */
#include "Internal/CBMisc.h"
#include "FileUtils.h"

/* ----------------------------------------------------------------------- */
/*                         Public functions                                */

CONST _kernel_oserror *get_file_size(const char *const f, int *const size)
{
  assert(f != NULL);
  assert(size != NULL);

  OS_File_CatalogueInfo cat;
  ON_ERR_RTN_E(os_file_read_cat_no_path(f, &cat));

  switch (cat.object_type)
  {
  case ObjectType_NotFound:
    return os_file_generate_error(f, OS_File_GenerateError_FileNotFound);

  case ObjectType_Directory:
    return os_file_generate_error(f, OS_File_GenerateError_IsADirectory);

  default:
    *size = (int)cat.length;
    break;
  }
  return NULL;
}
