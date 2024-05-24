/*
 * CBLibrary: Improved scheduler to call interruptible functions when idle
 * Copyright (C) 2006 Christopher Bazley
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

/* Scheduler.h declares several types and functions which provide access to a
   system based upon Wimp null events that regulates the amount of time spent
   in a task and divides it between competing processes. Do not attempt
   to use this module and NullPoll or RoundRobin (which it supercedes) within
   the same program.

Dependencies: ANSI C library, Acorn library kernel, Acorn's event library.
Message tokens: NoMem.
History:
  CJB: 13-Aug-06: Created this header
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 28-Oct-06: Corrected the description of the return value of a
                  SchedulerIdleFunction.
  CJB: 27-Feb-07: Added definition of symbolic constant SCH_CLOCK_MAX.
  CJB: 10-Mar-07: Changed typedef 'sch_clock_t' from 'int32_t' to 'long int' to
                  prevent spurious 'implicit narrowing cast' warnings from
                  Norcroft compiler when assigning 'long' values to it.
  CJB: 26-Aug-09: Added prototype of function scheduler_set_time_slice.
  CJB: 30-Sep-09: Redefined SCH_MIN_PRIORITY and SCH_MAX_PRIORITY as enumerated
                  constants rather than macro values.
  CJB: 12-Oct-09: Renamed type 'sch_clock_t' as 'SchedulerTime'. Added extra
                  arguments to the scheduler_initialise function.
  CJB: 15-Oct-09: Added "NoMem" to list of required message tokens. Redefined
                  SchedulerTime as 'int' to match wimp_pollidle. :-(
  CJB: 26-Jun-10: Made definition of deprecated type and constant names
                  conditional upon definition of CBLIB_OBSOLETE.
  CJB: 05-May-12: Made the new arguments to scheduler_initialise conditional
                  upon CBLIB_OBSOLETE.
  CJB: 11-Dec-14: Deleted redundant brackets from function type definitions.
  CJB: 11-Dec-20: Deleted redundant uses of the 'extern' keyword.
*/

#ifndef Scheduler_h
#define Scheduler_h

/* ISO library headers */
#include <limits.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "toolbox.h"

/* Local headers */
#include "Macros.h"

enum
{
  SchedulerPriority_Min = 1,
  SchedulerPriority_Max = 10,
  SchedulerTime_Max     = INT_MAX
};

/* A type for the 32-bit centisecond counter as read by OS_ReadMonotonicTime. */
typedef int SchedulerTime;

typedef SchedulerTime SchedulerIdleFunction (void                */*handle*/,
                                             SchedulerTime        /*time_now*/,
                                             const volatile bool */*time_up*/);
   /*
    * When your function is called it will be passed the value of 'handle' given
    * on registration. It must return as soon as possible after the volatile
    * bool pointed to by 'time_up' goes true. It may return earlier but if it
    * does so consistently then it will receive less CPU time than other 'idle'
    * functions. The return value should be the earliest OS monotonic time at
    * which to call function again (analogous to the value passed in R2 to SWI
    * Wimp_PollIdle). Often this will be the 'time_now' argument, incremented
    * by some delay period.
    */

CONST _kernel_oserror *scheduler_initialise(
                    SchedulerTime   /*nice*/
#ifndef CBLIB_OBSOLETE
                   ,MessagesFD     */*mfd*/,
                    void          (*/*report_error*/)(CONST _kernel_oserror *)
#endif
);
   /*
    * Initialises the scheduler and sets up a Wimp event handler for null
    * events. When a null event is delivered to your task, the null event
    * handler calls any client-registered functions until the number of
    * centiseconds specified by 'nice' have elapsed (see
    * scheduler_set_time_slice). Unless 'mfd' is a null pointer, the specified
    * messages file will be given priority over the global messages file when
    * looking up text required by this module. Unless 'report_error' is a null
    * pointer, it should point to a function to be called if an error occurs
    * whilst handling an event.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

CONST _kernel_oserror *scheduler_finalise(void);
   /*
    * Removes the scheduler's null event handler and causes all registered
    * functions to be forgotten (thus releasing memory). Null events may be
    * masked out if they are only enabled on behalf of the scheduler (i.e. no
    * other NullPoll registrants). This function is not normally included in
    * pre-built library distributions.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

void scheduler_set_time_slice(SchedulerTime /*nice*/);
   /*
    * Sets the minimum number of centiseconds that should be spent calling
    * client-registered functions upon receipt of a Wimp null event, whilst at
    * least one such function is due. Different values of 'nice' control the
    * granularity of task-switching and hence what proportion of CPU time your
    * task uses compared to other background tasks (e.g. programs running under
    * TaskWindow cede control after 10cs).
    */

CONST _kernel_oserror *scheduler_register(SchedulerIdleFunction */*function*/, void */*handle*/, SchedulerTime /*first_call*/, int /*priority*/);
   /*
    * Registers a function to be called as soon as possible after the OS
    * monotonic time 'first_call', when the system is otherwise idle.
    * Your function will continue to be called at intervals dictated by its
    * return value. Each invocation will be allocated a time slice related to
    * 'priority', which must be between 1 (lowest) and 10 (highest). The value
    * of 'handle' will be passed to your function, and must also be used on
    * deregistration. Unless the scheduler is suspended then registration of
    * the first client function will result in null events being unmasked.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

CONST _kernel_oserror *scheduler_register_delay(SchedulerIdleFunction */*function*/, void */*handle*/, SchedulerTime /*delay*/, int /*priority*/);
   /*
    * As scheduler_register except that the 'delay' argument is a time period
    * (in centiseconds) to delay before calling the registered function for the
    * first time.
    */

void scheduler_deregister(SchedulerIdleFunction */*function*/, void *handle);
   /*
    * Tells the scheduler to stop calling a specified function when the system
    * is idle. It is safe to deregister a function from within itself (or any
    * of its subroutines). Unless the scheduler is suspended then deregistration
    * of the last client function will result in null events being masked.
    * May cause abnormal program termination if the specified function and
    * handle combination are unknown to the scheduler.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

CONST _kernel_oserror *scheduler_poll(int */*event_code*/, WimpPollBlock */*poll_block*/, void */*poll_word*/);
   /*
    * This function is intended as a direct replacement for event_poll. It polls
    * the Wimp to allow other tasks to run and ensures that unnecessary null
    * events are not received by our task (i.e. it decides whether to use SWI
    * Wimp_PollIdle or Wimp_Poll).
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

void scheduler_suspend(void);
   /*
    * Causes all scheduled processing activity to cease. It may result in
    * null events being masked. You can call this function when the scheduler
    * is already suspended, but each one must be mirrored by a call to
    * scheduler_resume.
    */

void scheduler_resume(void);
   /*
    * Causes resumption of all scheduled processing, once this function has been
    * called the same number of times as scheduler_suspend. It may result in
    * null events being unmasked. It may cause abnormal program termination if
    * called when the scheduler is not suspended.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

#ifdef CBLIB_OBSOLETE
/* Deprecated type and enumeration constant names */
#define sch_clock_t      SchedulerTime
#define SCH_MIN_PRIORITY SchedulerPriority_Min
#define SCH_MAX_PRIORITY SchedulerPriority_Max
#define SCH_CLOCK_MAX    SchedulerTime_Max
#endif /* CBLIB_OBSOLETE */

#endif /* Scheduler_h */
