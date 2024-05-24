/*
 * CBLibrary: Read the real-time clock
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
  CJB: 14-Oct-09: Split this source file from c.DateStamp, added assertions
                  and replaced 'magic' values with named constants.
  CJB: 05-May-12: Added support for stress-testing failure of _kernel_osword.
                  Updated for change to definition of type 'OS_DateAndTime'.
                  Error pointer returned by get_current_time is no longer
                  const-qualified if CBLIB_OBSOLETE is defined.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 01-Nov-18: Reimplemented using the os_word_read_real_time function
                  from CBOSLib.
 */

/* Acorn C/C++ library headers */
#include "kernel.h"

/* CBOSLib headers */
#include "OSWord.h"

/* Local headers */
#include "Internal/CBMisc.h"
#include "DateStamp.h"

/* ----------------------------------------------------------------------- */
/*                         Public functions                                */

CONST _kernel_oserror *get_current_time(OS_DateAndTime *utc)
{
  return os_word_read_real_time(utc);
}
