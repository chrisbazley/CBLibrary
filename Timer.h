/*
 * CBLibrary: Set a volatile boolean variable after a specified delay
 * Copyright (C) 2003 Christopher Bazley
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

/* Timer.h defines two functions that use RISC OS ticker timer events to change
   a boolean value after a set delay period, without any intervention by the
   foreground process.

Dependencies: None.
Message tokens: None.
History:
  CJB: 05-Nov-04: Added clib-style documentation and dependency information.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 10-Mar-07: Changed wait time from 'unsigned int' to 'long int' to
                  prevent spurious 'implicit narrowing cast' warnings from
                  Norcroft compiler when assigning 'long' values to it.
  CJB: 15-Oct-09: Changed wait time to 'int' to match wimp_pollidle. :-(
  CJB: 11-Dec-20: Deleted redundant uses of the 'extern' keyword.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
*/

#ifndef Timer_h
#define Timer_h

/* ISO library headers */
#include <stdbool.h>

/* Acorn C/C++ library headers */
#include "kernel.h"

/* Local headers */
#include "Macros.h"

#if !defined(USE_OPTIONAL) && !defined(_Optional)
#define _Optional
#endif

_Optional CONST _kernel_oserror *timer_register(volatile bool * /*timeup_flag*/, int /*wait_time*/);
   /*
    * Sets the variable pointed to by 'timeup_flag' to false and sets up a
    * ticker event to change it to true after 'wait_time' centiseconds have
    * elapsed.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */






_Optional CONST _kernel_oserror *timer_deregister(volatile bool * /*timeup_flag*/);
   /*
    * Removes a pending ticker timer event. The value of 'timeup_flag' must be
    * the same as when the timer was registered. Call this function before
    * your program exits or polls the window manager if there is any
    * possibility that a ticker event is still pending.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

#endif
