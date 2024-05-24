/*
 * CBLibrary: Get the RISC OS type of a file
 * Copyright (C) 2021 Christopher Bazley
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
  CJB: 31-May-21: Created this source file.
 */

/* ISO library headers */
#include <stddef.h>

/* Acorn C/C++ library headers */
#include "kernel.h"

/* CBOSLib headers */
#include "OSFile.h"

/* Local headers */
#include "Internal/CBMisc.h"
#include "FileUtils.h"

/* ----------------------------------------------------------------------- */
/*                         Public functions                                */

CONST _kernel_oserror *get_file_type(const char *f, int *type)
{
  OS_File_CatalogueInfo info;
  CONST _kernel_oserror *e = os_file_read_cat_no_path(f, &info);
  if (!e) {
    *type = decode_load_exec(info.load, info.exec, NULL);
  }
  return e;
}
