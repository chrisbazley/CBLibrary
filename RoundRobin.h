/*
 * CBLibrary: Round-robin scheduler to call interruptible functions when idle
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

/* RoundRobin.h declares one type and several functions that provide access to
   a system for dividing the number of null events an application receives from
   the Wimp between a number of 'threads' doing background processing. This
   module is DEPRECATED in favour of Scheduler. Do not attempt to use both
   within the same program.

Dependencies: ANSI C library, Acorn's event library.
Message tokens: NoMem.
History:
  CJB: 05-Nov-04: Added dependency information.
  CJB: 06-Nov-04: Added clib-style documentation.
  CJB: 05-Mar-05: Updated documentation on RoundRobin_deregister and
                  RoundRobin_resume.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 15-Oct-09: Added "NoMem" to list of required message tokens.
  CJB: 26-Jun-10: Made compilation of this file conditional upon definition of
                  CBLIB_OBSOLETE.
  CJB: 26-Feb-12: Made RoundRobin_resume, RoundRobin_suspend and
                  RoundRobin_deregister return NULL to allow compilation of
                  very old code that requires a return value.
  CJB: 11-Dec-14: Deleted redundant brackets from function type definitions.
  CJB: 11-Dec-20: Deleted redundant uses of the 'extern' keyword.
*/

#ifndef RoundRobin_h
#define RoundRobin_h

#ifdef CBLIB_OBSOLETE

/* ISO library headers */
#include <stdbool.h>

/* Acorn C/C++ library headers */
#include "kernel.h"

/* Local headers */
#include "Macros.h"

typedef void RoundRobinHandler (void                *handle,
                                const volatile bool *time_up);
   /*
    * When your handler is called it will be passed the value of 'handle' given
    * on registration. It must return as soon as possible after the volatile
    * bool pointed to by 'time_up' goes true. It may return earlier but if it
    * does so consistently then it will receive less CPU time overall than
    * other RoundRobinHandlers.
    */

CONST _kernel_oserror *RoundRobin_initialise(unsigned int /*time*/);
   /*
    * Initialises the RoundRobin system and sets up a Wimp event handler for
    * null events. When a null event is delivered to your application, this
    * handler calls any registered RoundRobinHandler functions in rotation
    * until the number of centiseconds specified by 'time' have elapsed.
    * Different values of 'time' control the granularity of task-switching and
    * hence what proportion of CPU time your program grabs compared to other
    * background tasks (for example programs running in a TaskWindow cede
    * control after 10cs).
    * If there are no RoundRobinHandlers registered or the system is suspended
    * then the null event handler will return immediately. Any event handlers
    * independently registered with the Event library may delay your application
    * from ceding control (we do not 'claim' the event).
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

CONST _kernel_oserror *RoundRobin_finalise(void);
   /*
    * Removes the RoundRobin system's null event handler and causes all
    * registered RoundRobinHandlers to be forgotten (thus releasing any memory
    * used). Null events may be masked out if they are only enabled on behalf
    * of the RoundRobin system (i.e. no other NullPoll registrants). Note that
    * this function is not normally included in pre-built library distributions.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

CONST _kernel_oserror *RoundRobin_register(RoundRobinHandler * /*handler*/, void * /*handle*/);
   /*
    * Registers a RoundRobinHandler function to be called on null events when
    * it is its turn, or when the previous RoundRobinHandler has ceded control
    * early. The value of 'handle' will be passed to your RoundRobinHandler,
    * and must also be used on deregistration. Unless the system is suspended
    * (see RoundRobin_suspend) or there are pre-existing NullPoll registrants
    * then registration of the first RoundRobinHandler will result in null
    * events being unmasked.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

CONST _kernel_oserror *RoundRobin_deregister(RoundRobinHandler * /*handler*/, void * /*handle*/);
   /*
    * Deregisters a RoundRobinHandler function. Unless the system is suspended
    * (see RoundRobin_suspend) or there are other NullPoll registrants then
    * deregistration of the last RoundRobinHandler will result in null events
    * being masked out. May cause abnormal program termination if no such
    * handler exists.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

CONST _kernel_oserror *RoundRobin_suspend(void);
   /*
    * Causes cessation of all background processing activity controlled by
    * the RoundRobin system. Registered RoundRobinHandlers will not be called
    * again until RoundRobin_resume is called. May result in null events
    * being masked out if there are no other NullPoll registrants. You can call
    * this function when the system is already suspended, but each one must be
    * mirrored by a call to RoundRobin_resume.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

CONST _kernel_oserror *RoundRobin_resume(void);
   /*
    * Causes resumption of any background processing activity controlled by
    * the RoundRobin system, once this function has been called the same number
    * of times as RoundRobin_suspend. May result in null events being unmasked
    * unless there are other NullPoll registrants. May cause abnormal program
    * termination if called when activity not suspended.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

#else /* CBLIB_OBSOLETE */
#error Header file RoundRobin.h is deprecated
#endif /* CBLIB_OBSOLETE */

#endif
