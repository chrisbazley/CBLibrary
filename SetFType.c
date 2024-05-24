/*
 * CBLibrary: Set the RISC OS type of a file
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

/* History:
  CJB: 06-Sep-09: Created this source file.
  CJB: 09-Sep-09: Added assertion.
  CJB: 13-Oct-09: Replace 'magic' value with named constant.
  CJB: 05-May-12: Added support for stress-testing failure of _kernel_osfile.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 29-May-16: Functionality is now delegated to FileSType.c.
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

CONST _kernel_oserror *set_file_type(const char *f, int type)
{
  return os_file_set_type(f, type);
}
