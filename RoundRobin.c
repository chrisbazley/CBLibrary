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

/* History:
  CJB: 07-Mar-04: Added #include "msgtrans.h" (no longer included from
                  Macros.h).
                  Updated to use the new macro names defined in h.Macros.
  CJB: 13-Jun-04: Because all macro definitions are now expression statements,
                  have changed those invocations which omitted a trailing ';'.
  CJB: 13-Jan-05: Changed to use new msgs_error() function, hence no
                  longer requires external error block 'shared_err_block'.
  CJB: 15-Jan-05: Changed to use new DEBUG macro rather than ugly in-line code.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 25-Oct-06: Made compilation of this source file conditional upon pre-
                  processor symbol CBLIB_OBSOLETE (superceded by c.Scheduler).
  CJB: 14-Apr-07: Modified roundrobin_finalise() to soldier on if an error
                  occurs, using the new MERGE_ERR macro.
  CJB: 22-Jun-09: Use variable name rather than type with 'sizeof' operator
                  and tweaked spacing.
  CJB: 09-Sep-09: Stop using reserved identifiers '_RoundRobin_record' and
                  '_RoundRobin_handler' (start with an underscore followed by a
                  capital letter).
  CJB: 26-Feb-12: Made RoundRobin_resume, RoundRobin_suspend and
                  RoundRobin_deregister return NULL to allow compilation of
                  very old code that requires a return value.
  CJB: 23-Dec-14: Apply Event library function calls.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 21-Apr-16: Cast pointer parameters to void * to match %p. No longer
                  prints function pointers (no matching format specifier).
                  Used size_t for loop counters to match type of ARRAY_SIZE.
  CJB: 01-Nov-18: Replaced DEBUG macro usage with DEBUGF.
  CJB: 03-May-25: Fix #include filename case.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
 */

#ifdef CBLIB_OBSOLETE /* Use c.Scheduler instead */

/* ISO library headers */
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "wimp.h"
#include "event.h"
#include "toolbox.h"

/* Local headers */
#include "Err.h"
#include "NullPoll.h"
#include "Timer.h"
#include "RoundRobin.h"
#include "msgtrans.h"
#include "Internal/CBMisc.h"

typedef struct
{
  RoundRobinHandler *handler; /* == NULL marks free blocks */
  void              *handle;
}
RoundRobinRecord;

static int suspended;
static RoundRobinRecord *threads_array;
static size_t threads_array_len, thread_to_call, num_threads;
static unsigned int maxtime;
static volatile bool time_up = true;

/* ----------------------------------------------------------------------- */
/*                       Function prototypes                               */

static WimpEventHandler null_event_handler;
static void ensure_timer_removed(void);

/* ----------------------------------------------------------------------- */
/*                         Public functions                                */

_Optional CONST _kernel_oserror *RoundRobin_initialise(unsigned int time)
{
  ON_ERR_RTN_E(event_register_wimp_handler(-1,
                                           Wimp_ENull,
                                           null_event_handler,
                                           NULL));
  thread_to_call = 0;
  threads_array_len = 0;
  threads_array = NULL;
  suspended = 0;
  maxtime = time;
  atexit(ensure_timer_removed);
  return NULL;
}

/* ----------------------------------------------------------------------- */

#ifdef INCLUDE_FINALISATION_CODE
_Optional CONST _kernel_oserror *RoundRobin_finalise(void)
{
  _Optional CONST _kernel_oserror *return_error;

  return_error = event_deregister_wimp_handler(-1,
                                               Wimp_ENull,
                                               null_event_handler,
                                               NULL);
  free(threads_array);
  threads_array_len = 0;
  if (num_threads > 0 && suspended != 0)
    nullpoll_deregister();

  return return_error;
}
#endif

/* ----------------------------------------------------------------------- */

_Optional CONST _kernel_oserror *RoundRobin_register(RoundRobinHandler *handler, void *handle)
{
  /* Add record to threads data */
  RoundRobinRecord *write_data = NULL;

  if (threads_array != NULL) {
    /* Search for a free RoundRobinRecord block in array */
    for (size_t i = 0; i < threads_array_len && write_data == NULL; i++) {
      if (threads_array[i].handler == NULL)
        write_data = &(threads_array[i]); /* found one */
    }
  }

  if (write_data == NULL) {
    /* Create/Extend array of RoundRobinRecord blocks */
    RoundRobinRecord *new_data;

    new_data = realloc(threads_array,
                       sizeof(*new_data) * (threads_array_len + 1));
    if (new_data == NULL)
      return msgs_error(DUMMY_ERRNO, "NoMem");

    threads_array = new_data;
    write_data = &(threads_array[threads_array_len]);
    threads_array_len++;
    write_data->handler = NULL; /* mark new (end) block as free */
  }

  if (num_threads == 0 && suspended == 0)
    nullpoll_register(); /* We have first client */
  num_threads++;

  /* Fill out fields of RoundRobinRecord block */
  write_data->handler = handler;
  write_data->handle = handle;

  return NULL; /* success */
}

/* ----------------------------------------------------------------------- */

_Optional CONST _kernel_oserror *RoundRobin_deregister(RoundRobinHandler *handler, void *handle)
{
  RoundRobinRecord *write_data = NULL;

  /* Search for the specified thread data */
  for (size_t i = 0; i < threads_array_len && write_data == NULL; i++) {
    if (threads_array[i].handler == handler && threads_array[i].handle == handle)
      write_data = &(threads_array[i]); /* found it */
  }

  assert(write_data != NULL);
  if (write_data == NULL)
    return NULL; /* bad deregistration */

  if (num_threads == 1 && suspended == 0)
    nullpoll_deregister(); /* We have run out of clients */
  num_threads--;

  write_data->handler = NULL; /* mark slot as free */

  return NULL;
}

/* ----------------------------------------------------------------------- */

_Optional CONST _kernel_oserror *RoundRobin_suspend(void)
{
  if (suspended == 0 && num_threads > 0)
    nullpoll_deregister();
  suspended++;
  return NULL;
}

/* ----------------------------------------------------------------------- */

_Optional CONST _kernel_oserror *RoundRobin_resume(void)
{
  /* Can't resume if not suspended! */
  assert(suspended >= 1);
  if (suspended < 1)
    return NULL; /* not suspended */

  if (suspended == 1 && num_threads > 0)
    nullpoll_register();

  suspended--;
  return NULL;
}


/* ----------------------------------------------------------------------- */
/*                         Private functions                               */

static int null_event_handler(int event_code, WimpPollBlock *event, IdBlock *id_block, void *handle)
{
  unsigned int num_this_poll;

  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(id_block);
  NOT_USED(handle);

  if (threads_array == NULL || suspended != 0)
    return 0; /* nothing to do - pass event on */

  {
    _Optional CONST _kernel_oserror *err;
    time_up = false;
    err = timer_register(&time_up, maxtime);
    if (err != NULL) {
      time_up = true; /* could not set up timer event */
      err_check_rep(err);
    }
  }

  DEBUGF("Entering RoundRobin null event dispatcher\n");
  num_this_poll = 0;
  do {
    RoundRobinRecord *block;

    if (thread_to_call >= threads_array_len)
      thread_to_call = 0;

    block = &threads_array[thread_to_call];
    DEBUGF("Thread record %zu at %p\n", thread_to_call, (void *)block);

    if (block->handler != NULL) {
      DEBUGF("Calling handler with handle %p\n", block->handle);
      num_this_poll++;
      block->handler(block->handle, &time_up);
    }

    thread_to_call++;
  } while (!time_up);
  ensure_timer_removed(); /* in case OS_CallAfter event still pending */

  DEBUGF("Handlers called this poll: %u\n", num_this_poll);
  return 0; /* pass event on */
}

/* ----------------------------------------------------------------------- */

static void ensure_timer_removed(void)
{
  /* Last-ditch effort to remove OS_CallAfter routine, if still pending */
  if (!time_up) {
    timer_deregister(&time_up);
    time_up = true;
  }
  /* (suppress errors, as event may occur between check and removal) */
}

#else /* CBLIB_OBSOLETE */
#error Source file RoundRobin.c is deprecated
#endif /* CBLIB_OBSOLETE */
