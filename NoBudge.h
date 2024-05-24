/*
 * CBLibrary: Manage the flex memory heap budge state for multiple clients
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

/* NoBudge.h declares two functions that allow management of the flex budge
   state (i.e. whether flex blocks can be moved to allow the C heap to expand)
   on behalf of multiple clients.

Dependencies: ANSI C library, Acorn's flex library.
Message tokens: None.
History:
  CJB: 02-Nov-04: Now includes C library's <stddef.h> rather than "kernel.h".
                  Added clib-style documentation.
  CJB: 04-Nov-04: Added dependency information.
  CJB: 05-Mar-05: Updated documentation on nobudge_deregister.
*/

#ifndef NoBudge_h
#define NoBudge_h

/* ISO library headers */
#include <stddef.h>

void nobudge_register(size_t /*heap_ensure*/);
   /*
    * Disables flex budging (unless already disabled) and increments a count of
    * processes that require this state. Typically used when code relies upon
    * pointers into flex blocks remaining valid over function calls (which may
    * result in stack extension). Prior to disabling flex budging this function
    * will attempt to ensure at least 'heap_ensure' bytes of space are available
    * in the heap to reduce the likelyhood of stack overflows.
    */

void nobudge_deregister(void);
   /*
    * Decrements a count of processes that require flex budging disabled and
    * only re-enables it (thus allowing any subsequent heap expansion) when
    * this count reaches zero. May cause abnormal program termination if no
    * processes are registered as requiring flex budging disabled.
    */

#endif
