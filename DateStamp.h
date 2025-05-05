/*
 * CBLibrary: Read the real-time clock or the date stamp of a file
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

/* DateStamp.h declares two functions that output 5 byte UTC times.

Dependencies: ANSI C library, Acorn library kernel.
Message tokens: None.
History:
  CJB: 28-Nov-04: Created this header.
  CJB: 06-Jan-05: Changed declaration of get_date_stamp, which now returns any
                  OS error. Updated help. Added declaration of get_current_time.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 08-Apr-07: Added definition of type os_date_and_time, which is exactly 5
                  bytes in size. This should be used instead of an int array
                  with two elements; too bad about fileinfo_set_date().
  CJB: 21-Jan-08: Use the 'uint8_t' type instead of 'unsigned char'.
  CJB: 30-Sep-09: Renamed the type 'os_date_and_time' as 'OSDateAndTime'.
  CJB: 26-Jun-10: Made definition of deprecated type name conditional upon
                  definition of CBLIB_OBSOLETE.
  CJB: 05-May-12: Type 'OSDateAndTime' is now a struct not an array, to
                  ensure the array has enough elements.
                  Declared a new function to decode load and exec addresses.
  CJB: 30-Oct-18: Moved definitions of the date type and the decode_load_exec
                  function to CBOSLib.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
*/

#ifndef DateStamp_h
#define DateStamp_h

/* ISO library headers */
#include <stdint.h>

/* Acorn C/C++ library headers */
#include "kernel.h"

/* CBOSLib headers */
#include "OSWord.h"

/* Local headers */
#include "Macros.h"

#if !defined(USE_OPTIONAL) && !defined(_Optional)
#define _Optional
#endif

typedef OS_DateAndTime OSDateAndTime;

_Optional CONST _kernel_oserror *get_date_stamp(const char * /*f*/,
                                                OS_DateAndTime * /*utc*/);
   /*
    * Reads the date and time when object 'f' was last changed, in 5 byte UTC
    * time format. On entry 'utc' must point to an object in which to store the
    * date and time value.
    * Returns: a pointer to an OS error block, or else NULL for success. If no
    *          error occured then the date stamp will have been output. For an
    *          unstamped file this will be 00:00:00 01-Jan-1900.
    */

_Optional CONST _kernel_oserror *get_current_time(OS_DateAndTime * /*utc*/);
   /*
    * Reads the current date and time from the CMOS clock in 5 byte UTC time
    * format. On entry 'utc' must point to an object in which to store the date
    * and time value.
    * Returns: a pointer to an OS error block, or else NULL for success. If no
    *          error occured then the current time will have been output.
    */

#ifdef CBLIB_OBSOLETE
/* Deprecated type name */
#define os_date_and_time OS_DateAndTime
#endif /* CBLIB_OBSOLETE */

#endif /* DateStamp_h */
