/*
 * CBLibrary: Manage the event mask for multiple clients requiring null events
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

/* NullPoll.h declares two functions that allow management of an application's
   event mask on behalf of multiple clients. Do not attempt to use in
   conjunction with the Scheduler module!

Dependencies: ANSI C library, Acorn's event library.
Message tokens: None.
History:
  CJB: 02-Nov-04: No longer #includes "kernel.h" Clib header.
                  Added clib-style documentation.
  CJB: 04-Nov-04: Added dependency information.
  CJB: 05-Mar-05: Updated documentation on nullpoll_deregister.
  CJB: 19-Oct-09: Removed deprecation notice.
  CJB: 11-Dec-20: Deleted redundant uses of the 'extern' keyword.
 */

#ifndef NullPoll_h
#define NullPoll_h

void nullpoll_register(void);
   /*
    * Unmasks null events (if masked) and increments a count of processes
    * that require this state. Typically used at the start of some background
    * processing in a program that does not wish to receive null events all the
    * time.
    */

void nullpoll_deregister(void);
   /*
    * Decrements a count of processes that require null events and only masks
    * them out when this count reaches zero. Typically used at the end of some
    * background processing. May cause abnormal program termination if no
    * processes are registered as requiring null events.
    */

#endif
