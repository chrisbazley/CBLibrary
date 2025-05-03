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

/* History:
  CJB: 13-Aug-06: Created this source file (based upon c.RoundRobin)
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
                  Now uses Fortify to check memory heap usage.
                  Rewrote scheduler_finalise() so that it actually compiles!
                  Modified _scheduler_null_handler() so that client functions
                  that return before their allocated time is up, do not have
                  their next time slice reduced if they want to sleep.
  CJB: 14-Apr-07: Modified scheduler_finalise() to soldier on if an error
                  occurs, using the new MERGE_ERR macro.
  CJB: 22-Jun-09: Use variable name rather than type with 'sizeof' operator and
                  tweaked spacing.
  CJB: 26-Aug-09: Added function scheduler_set_time_slice to allow the amount
                  of time spent in scheduler_poll to be configured after
                  initialisation.
  CJB: 14-Oct-09: Removed dependency on MsgTrans and Err modules by storing
                  pointers to a messages file descriptor and an error-reporting
                  callback upon initialisation. Renamed type sch_record as
                  SchedulerClient. Updated to use new type name SchedulerTime
                  and enumerated constant names SchedulerPriority_{Min|Max}.
                  Renamed _scheduler_at_exit as cancel_ticker; it is now called
                  by _scheduler_null_handler instead of duplicate inline code.
                  Created a simple veneer for SWI OS_ReadMonotonicTime. Use
                  'for' loops in preference to 'while' loops.
  CJB: 05-May-12: Made the new arguments to scheduler_initialise conditional
                  upon CBLIB_OBSOLETE.
  CJB: 17-Dec-14: Updated to use the generic linked list implementation.
  CJB: 23-Dec-14: Apply Fortify to Event library function calls.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 06-May-15: Improved debugging output.
  CJB: 02-Sep-15: Stopped abusing format specifier %ld for centisecond timer
                  values (since last change to type alias SchedulerTime).
  CJB: 31-Jan-16: Substituted _kernel_swi for _swix because it's easier to
                  intercept for stress testing.
  CJB: 09-Apr-16: Deleted excess field specifiers from a bad format string
                  in scheduler_initialise to avoid GNU C compiler warnings.
  CJB: 10-Apr-16: Cast pointer parameters to void * to match %p. No longer
                  prints function pointers (no matching format specifier).
  CJB: 01-Nov-18: Replaced DEBUG macro usage with DEBUGF.
  CJB: 14-Mar-19: Replaced read_clock function with os_read_monotonic_time.
  CJB: 22-Jun-19: Recategorized debug output upon polling or receiving a
                  null event as verbose.
  CJB: 25-Aug-20: Deleted a redundant static function pre-declaration.
  CJB: 16-May-21: Prefer to declare variables with an initializer.
                  Use CONTAINER_OF instead of assuming struct layout.
                  Assign a compound literal when initializing a new client.
                  Skip clients for which removal is already pending in
                  _scheduler_client_has_callback(). This allows use of
                  scheduler_deregister() followed by scheduler_register()
                  (for the same client) in a SchedulerIdleFunction.
  CJB: 03-May-25: Fix #include filename case.
 */

/* ISO library headers */
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "event.h"
#include "swis.h"
#include "toolbox.h"

/* CBUtilLib headers */
#include "LinkedList.h"

/* CBOSLib headers */
#include "MessTrans.h"
#include "OSReadTime.h"

/* Local headers */
#include "Internal/CBMisc.h"
#include "scheduler.h"
#include "Timer.h"
#ifdef CBLIB_OBSOLETE
#include "msgtrans.h"
#include "Err.h"
#endif /* CBLIB_OBSOLETE */


typedef struct
{
  SchedulerIdleFunction *funct; /* pointer to client function */
  void                  *arg;   /* pointer to data required by client function */
}
SchedulerClientCallback;

typedef struct
{
  LinkedListItem          list_item;
  SchedulerTime           next_invocation; /* OS_ReadMonotonicTime of next
                                              invocation */
  SchedulerTime           time_slice;      /* Normal runtime per invocation (in
                                              centiseconds) */
  SchedulerTime           next_time_slice; /* Runtime for next invocation */
  bool                    removal_pending; /* Waiting to be removed from list?*/
  SchedulerClientCallback callback;
}
SchedulerClient;

typedef struct
{
  SchedulerTime time_now, best_relative;
}
SchedulerFindEarliestData;

static LinkedList clients_list;
static SchedulerClient *next_client;
static SchedulerTime max_time_in_app;
static volatile bool time_up;
static bool defer_removals, removal_deferred, list_changed, initialised = false;
static unsigned int suspended, clients_count;
static MessagesFD *desc;
#ifndef CBLIB_OBSOLETE
static void (*report)(CONST _kernel_oserror *);
#endif

/* ----------------------------------------------------------------------- */
/*                       Function prototypes                               */

static void cancel_ticker(void);
static WimpEventHandler _scheduler_null_handler;
static void _scheduler_mask_nulls(bool mask);
static CONST _kernel_oserror *lookup_error(const char *token);
static LinkedListCallbackFn _scheduler_destroy_client, _scheduler_client_has_callback, _scheduler_find_earliest, _scheduler_destroy_pending;
static SchedulerClient *_scheduler_find_client(SchedulerClientCallback *callback);

/* ----------------------------------------------------------------------- */
/*                         Public functions                                */

CONST _kernel_oserror *scheduler_initialise(
                         SchedulerTime   nice
#ifndef CBLIB_OBSOLETE
                        ,MessagesFD     *mfd,
                         void          (*report_error)(CONST _kernel_oserror *)
#endif
)
{
  DEBUGF("Scheduler: initialising with maximum time %d\n", nice);
  assert(!initialised);

  /* Store pointers to messages file descriptor and error-reporting function */
#ifdef CBLIB_OBSOLETE
  desc = msgs_get_descriptor();
#else
  desc = mfd;
  report = report_error;
#endif

  ON_ERR_RTN_E(event_register_wimp_handler(-1,
                                           Wimp_ENull,
                                           _scheduler_null_handler,
                                           NULL));
  _scheduler_mask_nulls(true);
  linkedlist_init(&clients_list);
  next_client = NULL;
  defer_removals = false;
  suspended = clients_count = 0;
  max_time_in_app = nice;
  time_up = true;

  /* Last-ditch effort to remove ticker event routine, if still pending
     when client program terminates */
  atexit(cancel_ticker);

  initialised = true;
  return NULL; /* no error */
}

/* ----------------------------------------------------------------------- */

#ifdef INCLUDE_FINALISATION_CODE
CONST _kernel_oserror *scheduler_finalise(void)
{
  CONST _kernel_oserror *return_error = NULL;

  assert(initialised);
  initialised = false;

  DEBUGF("Scheduler: Finalising\n");

  /* Free the linked list of idle client handlers */
  linkedlist_for_each(&clients_list, _scheduler_destroy_client, NULL);

  if (!suspended && clients_count)
  {
    _scheduler_mask_nulls(true);
    clients_count = 0;
  }

  /* De-register our handler for Null events */
  MERGE_ERR(return_error,
            event_deregister_wimp_handler(-1,
                                          Wimp_ENull,
                                          _scheduler_null_handler,
                                          NULL));

  return return_error;
}
#endif

/* ----------------------------------------------------------------------- */

void scheduler_set_time_slice(SchedulerTime nice)
{
  DEBUGF("Scheduler: changing time slice from %d to %d\n",
        max_time_in_app, nice);

  assert(initialised);
  max_time_in_app = nice;
}

/* ----------------------------------------------------------------------- */

CONST _kernel_oserror *scheduler_register_delay(SchedulerIdleFunction *function, void *handle, SchedulerTime delay, int priority)
{
  SchedulerTime time_now;

  assert(initialised);
  ON_ERR_RTN_E(os_read_monotonic_time(&time_now));
  return scheduler_register(function, handle, time_now + delay, priority);
}

/* ----------------------------------------------------------------------- */

CONST _kernel_oserror *scheduler_register(SchedulerIdleFunction *function, void *handle, SchedulerTime first_call, int priority)
{
  CONST _kernel_oserror *e = NULL;

  DEBUGF("Scheduler: request to register function (handle %p, time %d,"
        " priority %d)\n", handle, first_call, priority);
  assert(initialised);

  /* Ensure that priority is within acceptable bounds */
  assert(priority >= SchedulerPriority_Min && priority <= SchedulerPriority_Max);
  if (priority < SchedulerPriority_Min)
    priority = SchedulerPriority_Min;
  else if (priority > SchedulerPriority_Max)
    priority = SchedulerPriority_Max;

  /* Ensure that the proposed key (function pointer and handle) is unique */
  SchedulerClientCallback callback = {.funct = function, .arg = handle};
  const SchedulerClient *const client_data = _scheduler_find_client(&callback);
  assert(client_data == NULL);
  if (client_data != NULL)
    return NULL; /* already registered */

  /* Create new record for timed function */
  SchedulerClient *const new_record = malloc(sizeof(*new_record));
  if (new_record == NULL)
  {
    DEBUGF("Scheduler: Not enough memory to create record!\n");
    e = lookup_error("NoMem");
  }
  else
  {
    *new_record = (SchedulerClient){
      .removal_pending = false,
      .time_slice = priority,
      .next_time_slice = priority,
      .callback = callback,
      .next_invocation = first_call,
    };

    /* Enable null events if this is the first client */
    DEBUGF("Scheduler: incrementing client count from %d\n", clients_count);
    if (++clients_count == 1 && !suspended)
    {
      _scheduler_mask_nulls(false);
    }

    /* Link new record onto head of our list */
    linkedlist_insert(&clients_list, NULL, &new_record->list_item);
    list_changed = true;
  }
  return e;
}

/* ----------------------------------------------------------------------- */

void scheduler_deregister(SchedulerIdleFunction *function, void *handle)
{
  DEBUGF("Scheduler: Request to deregister function (handle %p)\n", handle);
  assert(initialised);

  SchedulerClientCallback callback = {.funct = function, .arg = handle};
  SchedulerClient *const client_data = _scheduler_find_client(&callback);

  if (client_data != NULL)
  {
    /* We have found the associated record in the linked list */
    if (defer_removals)
    {
      /* Not safe to delink record at this time */
      DEBUGF("Scheduler: Deferring removal from list\n");
      client_data->removal_pending = true;
      removal_deferred = true;
    }
    else
    {
      _scheduler_destroy_client(&clients_list, &client_data->list_item, NULL);
    }

    assert(clients_count > 0);
    if (!clients_count)
    {
      DEBUGF("Scheduler: Invalid client count!\n");
      return;
    }

    DEBUGF("Scheduler: Decrementing client count from %d\n", clients_count);
    if (--clients_count == 0 && !suspended)
    {
      _scheduler_mask_nulls(true);
    }

    return;
  }
  assert("Not found in scheduler_deregister" == NULL);
}

/* ----------------------------------------------------------------------- */

CONST _kernel_oserror *scheduler_poll(int *event_code, WimpPollBlock *poll_block, void *poll_word)
{
  assert(initialised);

  /* If the scheduler is suspended or has no clients then null events should
     be masked (in which case there is no reason to use event_poll_idle). */
  if (clients_count && !suspended)
  {
    SchedulerFindEarliestData earliest;

    /* Read the current time */
    ON_ERR_RTN_E(os_read_monotonic_time(&earliest.time_now));

    /* Find the earliest time that we want to receive a null event */
    DEBUG_VERBOSEF("Scheduler: finding earliest time for next null event\n");
    earliest.best_relative = INT32_MAX; /* far in the future */

    linkedlist_for_each(&clients_list, _scheduler_find_earliest, &earliest);

    /* Yield control to the window manager and tell it not to return with a
       null event before the next call to a client function is due. */
    DEBUG_VERBOSEF("Scheduler: calling event_poll_idle with time %d\n",
           earliest.time_now + earliest.best_relative);

    ON_ERR_RTN_E(event_poll_idle(event_code,
                                 poll_block,
                                 earliest.time_now + earliest.best_relative,
                                 poll_word));
  }
  else
  {
    /* Yield control to the window manager. */
    //DEBUG_VERBOSEF("Scheduler: calling event_poll\n");
    ON_ERR_RTN_E(event_poll(event_code, poll_block, poll_word));
  }
  return NULL; /* no error */
}

/* ----------------------------------------------------------------------- */

void scheduler_resume(void)
{
  assert(initialised);

  /* Can't resume if not suspended! */
  assert(suspended >= 1);
  if (suspended < 1)
  {
    DEBUGF("Scheduler: bad resume! (count %d)\n", suspended);
    return; /* not suspended */
  }

  DEBUGF("Scheduler: resuming (count %d)\n", suspended);
  if (--suspended == 0 && clients_count > 0)
    _scheduler_mask_nulls(false);
}

/* ----------------------------------------------------------------------- */

void scheduler_suspend(void)
{
  assert(initialised);

  DEBUGF("Scheduler: suspending (count %d)\n", suspended);
  if (++suspended == 1)
    _scheduler_mask_nulls(true);
}

/* ----------------------------------------------------------------------- */
/*                         Private functions                               */

static bool check_error(CONST _kernel_oserror *e)
{
  bool is_error = false;
#ifdef CBLIB_OBSOLETE
  is_error = err_check(e);
#else
  if (e != NULL)
  {
    if (report != NULL)
      report(e);
    is_error = true;
  }
#endif
  return is_error;
}

/* ----------------------------------------------------------------------- */

static CONST _kernel_oserror *lookup_error(const char *token)
{
  /* Look up error message from the token, outputting to an internal buffer */
  return messagetrans_error_lookup(desc, DUMMY_ERRNO, token, 0);
}

/* ----------------------------------------------------------------------- */

static int _scheduler_null_handler(int event_code, WimpPollBlock *event, IdBlock *id_block, void *handle)
{
  /* This handler for null events should be registered last to ensure that
     it is called before any other null event handlers */
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(id_block);
  NOT_USED(handle);

  DEBUGF("Scheduler: handling null event (%u clients)\n", clients_count);

  /* Read the (approximate) time that the Wimp returned control to us */
  SchedulerTime entry_time;
  if (check_error(os_read_monotonic_time(&entry_time)))
    return 1; /* claim null event */

  if (suspended || !clients_count)
  {
    DEBUG_VERBOSEF("Scheduler: ignoring null event %s\n", suspended ? "(suspended)" : "");
    return 0; /* pass on null event */
  }

  /* We must not attempt to remove records from the linked list of
     'idle' functions until it is safe to do so */
  defer_removals = true;
  removal_deferred = false;

  /* Call any registered 'idle' client functions until our program's
     time slice has expired (or all functions are blocking) */
  SchedulerClient const *last_called = NULL;
  while (clients_count)
  {
    if (next_client == NULL)
    {
      /* We have lost our place in the list, or reached the end */
      DEBUG_VERBOSEF("Scheduler: returning to head of client list\n");
      LinkedListItem *const head = linkedlist_get_head(&clients_list);
      if (head == NULL)
      {
        DEBUGF("Scheduler: client list is empty!\n");
        break; /* paranoia */
      }
      next_client = CONTAINER_OF(head, SchedulerClient, list_item);
    }

    /* Calculate how long before our task must cede control to the Wimp */
    SchedulerTime pre_time;
    if (check_error(os_read_monotonic_time(&pre_time)))
      break;

    SchedulerTime const time_left = max_time_in_app - (pre_time - entry_time);
    DEBUGF("Scheduler: %d centiseconds remain\n", time_left);
    if (time_left <= 0)
    {
      break; /* out of time! */
    }

    DEBUGF("Scheduler: client %p with arg %p and desired run time %d is ready at %d (time now: %d)\n",
           (void *)next_client, (void *)next_client->callback.arg, next_client->next_time_slice,
           next_client->next_invocation, pre_time);

    if (!next_client->removal_pending &&
        next_client->next_invocation - pre_time <= 0)
    {
      DEBUGF("Scheduler: function with arg %p and desired run time %d has been ready since %d\n",
            next_client->callback.arg, next_client->next_time_slice, next_client->next_invocation);

      /* Register a background ticker event to set a boolean flag when
         it is time for the client function to return. */
      time_up = false;

      CONST _kernel_oserror *err = timer_register(&time_up,
                           time_left < next_client->next_time_slice ?
                             time_left : next_client->next_time_slice);
      if (check_error(err))
      {
        DEBUGF("Scheduler: could not set up timer event!\n");
        time_up = true;
        break;
      }

      /* Call the client function */
      next_client->next_invocation = next_client->callback.funct(
                                       next_client->callback.arg,
                                       pre_time,
                                       &time_up);
      last_called = next_client;
      bool const premature_rtn = !time_up;

      SchedulerTime post_time;
      err = os_read_monotonic_time(&post_time);

      /* Remove ticker event if it is still pending
         (i.e. if client function returned prematurely) */
      cancel_ticker();

      DEBUGF("Scheduler: client function returned %d%s\n",
             next_client->next_invocation,
             premature_rtn ? " (premature return)" : "");

      if (check_error(err))
        break;

      if (!next_client->removal_pending)
      {
        SchedulerTime const elapsed = post_time - pre_time;
        DEBUGF("Scheduler: function ran for %d ticks\n", elapsed);

        /* If the client function returned before its allocated time slice had
           expired, yet it does not want to sleep, then we will call it back
           A.S.A.P. to complete. (Typically happens to the last function called
           before we return from this event handler.) */
        if (elapsed < next_client->next_time_slice &&
            post_time - next_client->next_invocation >= 0)
        {
          /* Reduce the time slice for the next invocation of this function by
             the time elapsed during this invocation */
          next_client->next_time_slice -= elapsed;

          DEBUGF("Scheduler: will call it back A.S.A.P. for remaining %d ticks\n",
                next_client->next_time_slice);

          continue; /* Do not advance to next record in linked list */
        }

        /* Revert to normal run time for next invocation */
        next_client->next_time_slice = next_client->time_slice;
      }
    }
    else
    {
      /* If we have traversed the entire client list without calling any
         functions then we are wasting time that other tasks could use */
      if (next_client == last_called)
      {
        DEBUGF("Scheduler: no client functions ready\n");
        break;
      }
    }

    LinkedListItem *const next = linkedlist_get_next(&next_client->list_item);
    next_client = next ? CONTAINER_OF(next, SchedulerClient, list_item) : NULL;
  }

  /* Do any deferred removals of 'idle' functions */
  defer_removals = false;
  if (removal_deferred)
  {
    DEBUGF("Scheduler: Doing deferred removals\n");
    linkedlist_for_each(&clients_list, _scheduler_destroy_pending, NULL);
  }

  DEBUG_VERBOSEF("Scheduler: exiting null event handler\n");
  return 1; /* claim null event */
}

/* ----------------------------------------------------------------------- */

static void cancel_ticker(void)
{
  if (!time_up)
  {
    DEBUGF("Scheduler: cancelling pending ticker event\n");
    timer_deregister(&time_up);
    time_up = true;
  }
  /* (suppress errors, as event may occur between check and removal) */
}

/* ----------------------------------------------------------------------- */

static void _scheduler_mask_nulls(bool mask)
{
  unsigned int event_mask;
  event_get_mask(&event_mask);
  if (mask)
  {
    SET_BITS(event_mask, Wimp_Poll_NullMask);
  }
  else
  {
    CLEAR_BITS(event_mask, Wimp_Poll_NullMask);
  }
  DEBUGF("Scheduler: %smasking null events\n", mask ? "" : "un");
  event_set_mask(event_mask);
}

/* ----------------------------------------------------------------------- */

static SchedulerClient *_scheduler_find_client(SchedulerClientCallback *callback)
{
  assert(callback != NULL);
  DEBUGF("Scheduler: Searching for client with function arg %p\n", callback->arg);

  LinkedListItem *const item = linkedlist_for_each(
                    &clients_list, _scheduler_client_has_callback, callback);

  if (item == NULL)
  {
    DEBUGF("Scheduler: End of linked list (no match)\n");
    return NULL;
  }

  SchedulerClient *const client_data = CONTAINER_OF(item, SchedulerClient, list_item);
  DEBUGF("Scheduler: Record %p has matching callback\n", (void *)client_data);
  return client_data;
}

/* ----------------------------------------------------------------------- */

static bool _scheduler_destroy_client(LinkedList *list, LinkedListItem *item, void *arg)
{
  SchedulerClient * const client_data = CONTAINER_OF(item, SchedulerClient, list_item);
  NOT_USED(arg);

  DEBUGF("Scheduler: Freeing record for function\n");

  /* If we are about to remove the client to be called on the next null
     event then bring forward the next client instead */
  if (next_client == client_data)
  {
    LinkedListItem *const next = linkedlist_get_next(&client_data->list_item);
    if (next != NULL)
    {
      next_client = CONTAINER_OF(next, SchedulerClient, list_item);
      DEBUGF("Scheduler: Promoting client with function arg %p\n", next_client->callback.arg);
    }
    else
    {
      next_client = NULL;
    }
  }
  linkedlist_remove(list, &client_data->list_item);
  free(client_data);

  return false; /* next item */
}

/* ----------------------------------------------------------------------- */

static bool _scheduler_destroy_pending(LinkedList *list, LinkedListItem *item, void *arg)
{
  SchedulerClient * const client_data = CONTAINER_OF(item, SchedulerClient, list_item);

  if (client_data->removal_pending)
    return _scheduler_destroy_client(list, item, arg);

  return false; /* next item */
}

/* ----------------------------------------------------------------------- */

static bool _scheduler_client_has_callback(LinkedList *list, LinkedListItem *item, void *arg)
{
  const SchedulerClient * const client_data = CONTAINER_OF(item, SchedulerClient, list_item);
  const SchedulerClientCallback * const callback = arg;
  assert(callback != NULL);
  NOT_USED(list);

  /* Check whether this client is for callbacks to the specified
     function with the specified handle */
  return (callback->funct == client_data->callback.funct &&
          callback->arg == client_data->callback.arg &&
          !client_data->removal_pending);
}

/* ----------------------------------------------------------------------- */

static bool _scheduler_find_earliest(LinkedList *list, LinkedListItem *item, void *arg)
{
  const SchedulerClient * const client_data = CONTAINER_OF(item, SchedulerClient, list_item);
  SchedulerFindEarliestData * const earliest = arg;
  assert(earliest != NULL);
  NOT_USED(list);

  if (!client_data->removal_pending)
  {
    const SchedulerTime this_relative = client_data->next_invocation -
                                        earliest->time_now;

    DEBUG_VERBOSEF("Scheduler: function is due at %d (diff %d)\n",
           client_data->next_invocation, this_relative);

    if (this_relative < earliest->best_relative)
      earliest->best_relative = this_relative;
  }

  return false; /* next item */
}
