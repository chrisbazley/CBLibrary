/*
 * CBLibrary: Read the date stamp of a file
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
  CJB: 28-Nov-04: Created this source file.
  CJB: 06-Jan-05: Changed get_date_stamp to return OS errors (for consistency
                  with other direct-called library functions). Now copes
                  properly if FS object not found. Added new function
                  get_current_time for reading CMOS clock.
  CJB: 13-Jan-05: Fixed bug in get_date_stamp(), which was not correctly
                  initialising the value of R2 before calling OS_File 19.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 25-Nov-06: Added debugging output.
  CJB: 08-Apr-07: Changed to use new type os_date_and_time which is 5 bytes in
                  size, instead of pointers to arrays of ints (assumed to have
                  at least two elements, hence 8 bytes).
  CJB: 21-Jan-08: Fixed a memory corruption bug in the get_date_stamp function
                  highlighted by Fortify. '*UTC[4]' means the 5th
                  os_date_and_time (i.e. at offset +20), whereas '(*UTC)[4]'
                  means the fifth uint8_t array element (i.e. at offset +4).
  CJB: 22-Jun-09: Use variable rather than type name with 'sizeof' operator.
  CJB: 14-Oct-09: Added assertions, moved the definition of get_current_time
                  to a separate source file and replaced 'magic' values with
                  named constants.
  CJB: 05-May-12: Moved to decoding of load and execution addresses into a
                  separate function, decode_load_exec. ObjectType_NotFound is
                  now defined elsewhere.
  CJB: 26-Dec-14: Rewritten to use the os_file_read_cat_no_path and
                  os_file_generate_error functions instead of _kernel_osfile.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 01-Nov-18: Replaced DEBUG macro usage with DEBUGF.
 */

/* ISO library headers */
#include <stddef.h>
#include <string.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "swis.h"

/* CBOSLib headers */
#include "OSFile.h"

/* Local headers */
#include "Internal/CBMisc.h"
#include "DateStamp.h"

#define LoadAddressHasStamp (0xfff00000u)
#define LoadAddressStampMSB (0x000000ffu)

/* ----------------------------------------------------------------------- */
/*                         Public functions                                */

CONST _kernel_oserror *get_date_stamp(const char *f, OS_DateAndTime *utc)
{
  CONST _kernel_oserror *e;
  OS_File_CatalogueInfo cat;

  assert(f != NULL);
  assert(utc != NULL);
  DEBUGF("DateStamp: Reading catalogue info for object '%s'\n", f);

  e = os_file_read_cat_no_path(f, &cat);
  if (e != NULL)
  {
    DEBUGF("DateStamp: SWI returned error 0x%x '%s'\n",
           e->errnum, e->errmess);
  }
  else if (cat.object_type == ObjectType_NotFound)
  {
    /* Object not found - generate appropriate error */
    DEBUGF("DateStamp: object not found\n");
    e = os_file_generate_error(f, OS_File_GenerateError_FileNotFound);
    assert(e != NULL);
  }
  else
  {
    /* Object is a file, directory or image file */
    (void)decode_load_exec(cat.load, cat.exec, utc);
  }
  return e;
}
