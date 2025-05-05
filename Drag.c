/*
 * CBLibrary: Handle the sender's half of the drag & drop message protocol
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
  CJB: 08-Oct-06: Separated the code for handling drag and drop from the rest
                  of c.Saver.
  CJB: 23-Oct-06: First public release version.
  CJB: 12-Nov-06: drag_start() now deregisters the scheduler idle function if
                  an error occurs after registration.
  CJB: 15-Nov-06: A new function _drag_finished() incorporates code that was
                  common to drag_abort() and _drag_userdrag_handler(). The mouse
                  pointer position is no longer read separately in various
                  different functions, and never after a drag has terminated.
  CJB: 17-Nov-06: No longer passes a spurious message ID to a
                  DragFinishedHandler if there is no drag claimant.
                  _drag_dragclaim_msg_handler() now resets the pointer shape if
                  flags bit 0 is set in a DragClaim rec'd after the drop, even
                  if not set in a preceding DragClaim). Now guards against
                  badly-behaved tasks claiming a drag that was abandoned. Uses
                  !dragging_msg_ref (instead of a bool) to indicate there is no
                  outstanding Dragging message. No longer relies on !drag_flags
                  if !dragclaim_task.
  CJB: 14-Apr-07: drag_finalise() now attempts to soldier on if an error
                  occurs during finalisation. Modified this function and
                  _drag_finished() to use the new MERGE_ERR macro.
  CJB: 22-Jun-09: Use variable name rather than type with 'sizeof' operator,
                  removed unnecessary casts from 'void *' and tweaked spacing.
  CJB: 14-Oct-09: Titivated formatting, replaced 'magic' values with named
                  constants, use new DragBoxOp enumerated type and value names.
                  Removed dependencies on MsgTrans and Err modules by storing
                  pointers to a messages file descriptor and an error-reporting
                  callback upon initialisation. Rewrote _drag_decide_file_type
                  as a pure function, with idiomatic 'for' loops over arrays.
  CJB: 05-May-12: Added support for stress-testing failure of _kernel_osbyte.
                  Made the arguments to drag_initialise conditional upon
                  CBLIB_OBSOLETE.
  CJB: 23-Dec-14: Apply Fortify to Event & Wimp library function calls.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 04-Oct-15: Ensure that the drag_finished and drag_aborted flags aren't
                  set to false before the drag_start function returns an error.
  CJB: 26-Dec-15: Created functions to delete drag claims and reset the mouse
                  pointer shape in order to avoid code duplication.
                  Consequently the drag_finalise function now zeros the task
                  handle of the current drag claimant instead of merely
                  resetting the pointer shape.
                  Plugged leaks in the case where a drag has already finished
                  and wimp_send_message returns an error: any drag claim is
                  deleted and any memory associated with the drag is freed
                  because we can't wait for Dragging message(s) to return
                  unacknowledged.
  CJB: 21-Apr-16: Modified format strings to avoid GNU C compiler warnings.
  CJB: 01-Nov-18: Replaced DEBUG macro usage with DEBUGF.
  CJB: 27-Oct-19: Replaced code dealing with -1 terminated arrays of filetypes
                  with calls to pick_file_type(), copy_file_types() and
                  count_file_types().
  CJB: 31-Oct-21: Fixed debug output of uninitialized bounding box in
                  _drag_send_dragging_msg() when client passed a bounding box.
  CJB: 03-May-25: Fix #include filename case.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
*/

/* ISO library headers */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "wimp.h"
#include "event.h"
#include "wimplib.h"
#include "toolbox.h"

/* CBOSLib headers */
#include "MessTrans.h"
#include "WimpExtra.h"
#include "FileTypes.h"

/* Local headers */
#include "scheduler.h"
#include "Drag.h"
#ifdef CBLIB_OBSOLETE
#include "msgtrans.h"
#include "Err.h"
#endif /* CBLIB_OBSOLETE */
#include "Internal/CBMisc.h"

/* Constant numeric values */
enum
{
  OSByteScanKeys             = 129,  /* _kernel_osbyte reason code */
  OSByteReadNVRAM            = 161,  /* _kernel_osbyte reason code */
  OSByteScanKeysNoLimit      = 0xff, /* Time limit for OSByteScanKeys */
  OSByteScanKeysSingle       = 0xff, /* EOR this with the internal key number */
  OSByteR1ResultMask         = 0xff, /* To decode return value of _kernel_osbyte */
  OSByteR2ResultShift        = 8,    /* To decode return value of _kernel_osbyte */
  IntKeyNum_Shift            = 0,    /* Internal key number for OSByteScanKeys */
  NVRAMMiscFlags             = 0x1c, /* Index of relevant byte in NVRAM */
  NVRAMMiscFlags_DragASprite = 1<<1  /* Relevant flag bit of the above NVRAM byte */
};

/* -----------------------------------------------------------------------
                        Internal function prototypes
*/

static WimpMessageHandler _drag_dragclaim_msg_handler;
static WimpEventHandler _drag_msg_bounce_handler, _drag_userdrag_handler;
static SchedulerIdleFunction _drag_null_event_handler;
static _Optional CONST _kernel_oserror *_drag_send_dragging_msg(void);
static _Optional CONST _kernel_oserror *_drag_call_wdb_handler(DragBoxOp action);
static void _drag_free_mem(void);
static _Optional CONST _kernel_oserror *_drag_get_pointer_info(void);
static _Optional CONST _kernel_oserror *_drag_finished(void);
static void check_error(_Optional CONST _kernel_oserror *e);
#ifdef COPY_ARRAY_ARGS
static CONST _kernel_oserror *lookup_error(const char *token);
#endif
static void _drag_reset_ptr(void);
static bool _drag_delete_claim(void);
static bool _drag_call_drop_handler(int file_type);

/* -----------------------------------------------------------------------
                          Internal library data
*/

static bool drag_aborted, drag_finished = true, shift_held, solid_drags;
static bool initialised;
static unsigned int drag_flags;
#ifdef COPY_ARRAY_ARGS
/* The following pointers reference heap blocks, if COPY_ARRAY_ARGS defined */
static _Optional int *client_file_types;
static _Optional BBox *client_data_bbox; /* may be NULL */
#else
static const int *client_file_types;
static _Optional const BBox *client_data_bbox; /* may be NULL */
#endif
static void *client_drag_data;
static DragBoxHandler *client_fn_box;
static _Optional DragFinishedHandler *client_fn_drop;
static int dragclaim_msg_ref, dragging_msg_ref;
static int dragclaim_task;
static WimpGetPointerInfoBlock pointer;
static _Optional MessagesFD *desc;
#ifndef CBLIB_OBSOLETE
static void (*report)(CONST _kernel_oserror *);
#endif

/* -----------------------------------------------------------------------
                         Public library functions
*/

_Optional CONST _kernel_oserror *drag_initialise(
#ifdef CBLIB_OBSOLETE
                         void
#else
                         _Optional MessagesFD *mfd,
                         void (*report_error)(CONST _kernel_oserror *)
#endif /* CBLIB_OBSOLETE */
)
{
  unsigned int mask;

  assert(!initialised);

#ifdef CBLIB_OBSOLETE
  desc = msgs_get_descriptor();
#else
  /* Store pointers to messages file descriptor and error-reporting function */
  desc = mfd;
  report = report_error;
#endif /* CBLIB_OBSOLETE */

  /* Register a handler for DragClaim messages (received during a drag) */
  ON_ERR_RTN_E(event_register_message_handler(Wimp_MDragClaim,
                                              _drag_dragclaim_msg_handler,
                                              (void *)NULL));

  /* Register a handler for messages that return to us as wimp event 19 */
  ON_ERR_RTN_E(event_register_wimp_handler(-1,
                                           Wimp_EUserMessageAcknowledge,
                                           _drag_msg_bounce_handler,
                                           (void *)NULL));

  /* Register a handler for user drag events (to detect end of a drag) */
  ON_ERR_RTN_E(event_register_wimp_handler(-1,
                                           Wimp_EUserDrag,
                                           _drag_userdrag_handler,
                                           (void *)NULL));

  /* Ensure that messages are not masked */
  event_get_mask(&mask);
  CLEAR_BITS(mask, Wimp_Poll_UserMessageMask |
                   Wimp_Poll_UserMessageRecordedMask |
                   Wimp_Poll_UserMessageAcknowledgeMask);
  event_set_mask(mask);

  initialised = true;

  return NULL; /* success */
}

/* ----------------------------------------------------------------------- */

#ifdef INCLUDE_FINALISATION_CODE
_Optional CONST _kernel_oserror *drag_finalise(void)
{
  _Optional CONST _kernel_oserror *return_error = NULL;

  assert(initialised);
  initialised = false;

  MERGE_ERR(return_error, drag_abort());

  /* Reset the mouse pointer shape if drag claimant had altered it
     (we can't wait for our Dragging message to bounce) */
  (void)_drag_delete_claim();

  /* Deregister our handler for DragClaim messages */
  MERGE_ERR(return_error,
            event_deregister_message_handler(Wimp_MDragClaim,
                                             _drag_dragclaim_msg_handler,
                                             (void *)NULL));

  /* Deregister our handler for messages that return to us as wimp event 19 */
  MERGE_ERR(return_error,
            event_deregister_wimp_handler(-1,
                                          Wimp_EUserMessageAcknowledge,
                                          _drag_msg_bounce_handler,
                                          (void *)NULL));

  /* Deregister our handler for user drag events */
  MERGE_ERR(return_error,
            event_deregister_wimp_handler(-1,
                                          Wimp_EUserDrag,
                                          _drag_userdrag_handler,
                                          (void *)NULL));

  _drag_free_mem();

  return return_error;
}
#endif

/* ----------------------------------------------------------------------- */

_Optional CONST _kernel_oserror *drag_abort(void)
{
  assert(initialised);
  DEBUGF("Drag: Request to abort a drag\n");

  if (drag_finished)
    return NULL; /* no drag in progress */

  drag_aborted = true;
  return _drag_finished();
}

/* ----------------------------------------------------------------------- */

_Optional CONST _kernel_oserror *drag_start(const int *const file_types,
  _Optional const BBox *const data_bbox, DragBoxHandler *const drag_box_method,
  _Optional DragFinishedHandler *const drop_method, void *const handle)
{
  _Optional CONST _kernel_oserror *e;

  assert(initialised);
  assert(drag_box_method);
  assert(file_types[0] != FileType_Null);

  /* Check whether Shift key held at start of drag (move or copy?) */
  int osbyte_result = _kernel_osbyte(OSByteScanKeys,
                                 IntKeyNum_Shift ^ OSByteScanKeysSingle,
                                 OSByteScanKeysNoLimit);
  if (osbyte_result == _kernel_ERROR)
    return _kernel_last_oserror();

  shift_held = (osbyte_result & OSByteR1ResultMask) != 0;
  DEBUGF("Drag: Shift key %s held\n", shift_held ? "is" : "is not");

  /* Read NVRAM flag for whether user wants solid drags or outlines */
  osbyte_result = _kernel_osbyte(OSByteReadNVRAM, NVRAMMiscFlags, 0);
  if (osbyte_result == _kernel_ERROR)
    return _kernel_last_oserror();

  solid_drags = TEST_BITS(osbyte_result,
                          NVRAMMiscFlags_DragASprite << OSByteR2ResultShift);
  DEBUGF("Drag: User wants %s drags\n", solid_drags ? "solid" : "outline");

  /* We want to receive null events during the drag */
  ON_ERR_RTN_E(scheduler_register_delay(_drag_null_event_handler, handle, 0, 3));

  /* Initialise state variables for a new drag */
  dragclaim_task = 0;
  client_fn_box = drag_box_method;
  client_fn_drop = drop_method;
  client_drag_data = handle;

#ifdef COPY_ARRAY_ARGS
  _drag_free_mem();

  /* make a private copy of the array of file types (+1 for terminator) */
  size_t const array_len = count_file_types(file_types) + 1;
  client_file_types = malloc(array_len * sizeof(file_types[0]));
  if (client_file_types == NULL)
  {
    scheduler_deregister(_drag_null_event_handler, handle); /* 12-Nov-06 */
    return lookup_error("NoMem");
  }
  (void)copy_file_types(&*client_file_types, file_types, array_len - 1);

  /* make a private copy of the data's bounding box */
  if (data_bbox != NULL)
  {
    client_data_bbox = malloc(sizeof(*client_data_bbox));
    if (client_data_bbox == NULL)
    {
      scheduler_deregister(_drag_null_event_handler, handle); /* 12-Nov-06 */
      _drag_free_mem();
      return lookup_error("NoMem");
    }
    *client_data_bbox = *data_bbox;
  }
#else
  client_file_types = file_types;
  client_data_bbox = data_bbox;
#endif

  /* Update our cached mouse pointer position */
  e = _drag_get_pointer_info();
  if (e != NULL)
  {
    scheduler_deregister(_drag_null_event_handler, handle);
    _drag_free_mem();
    return e;
  }

  /* Tell the client to start a wimp drag box to represent the
     data during the drag */
  e = _drag_call_wdb_handler(DragBoxOp_Start);
  if (e != NULL)
  {
    scheduler_deregister(_drag_null_event_handler, handle); /* 12-Nov-06 */
    _drag_free_mem();
  }
  else
  {
    drag_finished = drag_aborted = false;
  }

  return e;
}

/* -----------------------------------------------------------------------
                        Wimp message handlers
*/

static int _drag_dragclaim_msg_handler(WimpMessage *message, void *handle)
{
  /* This is a handler for DragClaim messages */
  const WimpDragClaimMessage *dragclaim;

  assert(message != NULL);
  NOT_USED(handle);

  dragclaim = (WimpDragClaimMessage *)&message->data;
  DEBUGF("Drag: Received a DragClaim message (ref. %d in reply to %d)\n",
        message->hdr.my_ref, message->hdr.your_ref);

  if (!dragging_msg_ref || message->hdr.your_ref != dragging_msg_ref)
  {
    DEBUGF("Drag: Not a reply to our last message (ref. %d)\n", dragging_msg_ref);
    return 0; /* not a reply to our message */
  }
  dragging_msg_ref = 0;

  /* Our copy of the flags from a previous DragClaim message is only valid if
     the drag was not later relinquished. */
  if (!dragclaim_task)
    drag_flags = 0;

  /* Record details of the new drag claimant */
  dragclaim_msg_ref = message->hdr.my_ref;
  dragclaim_task = message->hdr.sender;
  DEBUGF("Drag: Drag claimed by task %d with message ref. %d\n", dragclaim_task,
        dragclaim_msg_ref);

  if (drag_finished)
  {
    DEBUGF("Drag: Drag is finished\n");

    /* Restore the default mouse pointer shape if this drag claimant, or the
       previous one, changed it. */
    if (TEST_BITS(drag_flags, Wimp_MDragClaim_PtrShapeChanged) ||
        TEST_BITS(dragclaim->flags, Wimp_MDragClaim_PtrShapeChanged))
    {
      _drag_reset_ptr();
      CLEAR_BITS(drag_flags, Wimp_MDragClaim_PtrShapeChanged);
    }

    /* The client's DragFinishedHandler function will return true if it sent
       a DataSave message to the drag claimant. */
    if (!drag_aborted &&
        !_drag_call_drop_handler(
          pick_file_type(dragclaim->file_types,
            client_file_types ? &*client_file_types : &(int){FileType_Null})))
    {
      /* Tell the claimant to relinquish the drag so that the ghost caret is
         removed and auto-scrolling stopped. */
      DEBUGF("Drag: Telling claimaint to relinquish drag\n");
      drag_aborted = true;
      check_error(_drag_send_dragging_msg());
    }
    else
    {
      _drag_free_mem();
    }
  }
  else
  {
    DEBUGF("Drag: Drag is still in progress\n");

    /* Restore the default mouse pointer shape if the previous drag claimant
       changed it, but this one did not. */
    if (TEST_BITS(drag_flags, Wimp_MDragClaim_PtrShapeChanged) &&
        !TEST_BITS(dragclaim->flags, Wimp_MDragClaim_PtrShapeChanged))
    {
      _drag_reset_ptr();
    }

    /* Remove or reinstate our representation of the dragged data, as
       requested by the drag claimant */
    if (TEST_BITS(dragclaim->flags, Wimp_MDragClaim_RemoveDragBox))
    {
      if (!TEST_BITS(drag_flags, Wimp_MDragClaim_RemoveDragBox))
      {
        /* Update our cached mouse pointer position */
        check_error(_drag_get_pointer_info());

        /* Hide our Wimp drag box (or visually-pleasing equivalent) */
        check_error(_drag_call_wdb_handler(DragBoxOp_Hide));
      }
    }
    else
    {
      if (TEST_BITS(drag_flags, Wimp_MDragClaim_RemoveDragBox))
      {
        /* Update our cached mouse pointer position */
        check_error(_drag_get_pointer_info());

        /* Show our Wimp drag box (or visually-pleasing equivalent) */
        check_error(_drag_call_wdb_handler(DragBoxOp_Start));
      }
    }

    /* Record the flags from this DragClaim message for future comparison */
    drag_flags = dragclaim->flags;
  }

  return 1; /* claim message */
}

/* -----------------------------------------------------------------------
                        Wimp event handlers
*/

static int _drag_msg_bounce_handler(int event_code, WimpPollBlock *event, IdBlock *id_block, void *handle)
{
  /* This is a handler for bounced messages */
  NOT_USED(event_code);
  assert(event != NULL);
  NOT_USED(id_block);
  NOT_USED(handle);

  DEBUGF("Drag: Received a bounced message (ref. %d)\n",
        event->user_message_acknowledge.hdr.my_ref);

  /* No need to guard against !dragging_msg_ref because my_ref != 0 */
  if (event->user_message_acknowledge.hdr.my_ref != dragging_msg_ref)
  {
    DEBUGF("Drag: Not our last message (ref. %d)\n", dragging_msg_ref);
    return 0; /* not our message?! */
  }
  dragging_msg_ref = 0;

  switch (event->user_message_acknowledge.hdr.action_code)
  {
    case Wimp_MDragging:
      /* One of our recorded Dragging messages has bounced */
      DEBUGF("Drag: It is a bounced Dragging message\n");

      if (_drag_delete_claim())
      {
        if (!drag_finished)
        {
          /* Update our cached mouse pointer position */
          check_error(_drag_get_pointer_info());

          /* Reinstate our representation of the dragged data, if the drag
             claimant had asked us to remove it */
          if (TEST_BITS(drag_flags, Wimp_MDragClaim_RemoveDragBox))
            check_error(_drag_call_wdb_handler(DragBoxOp_Start));
        }

        /* Send Dragging message to owner of the window at the mouse pointer */
        check_error(_drag_send_dragging_msg());
      }
      else
      {
        DEBUGF("Drag: There is currently no drag claimant\n");
        if (drag_finished)
        {
          if (!drag_aborted)
          {
            (void)_drag_call_drop_handler(
                    client_file_types ? client_file_types[0] : FileType_Null);
          }
          _drag_free_mem();
        }
      }
      return 1; /* claim event */
  }
  return 0; /* pass on event */
}

/* ----------------------------------------------------------------------- */

static int _drag_userdrag_handler(int event_code, WimpPollBlock *event, IdBlock *id_block, void *handle)
{
  /* This is a handler for UserDrag events (i.e. a drag has finished) */
  NOT_USED(event_code);
  assert(event != NULL);
  NOT_USED(id_block);
  NOT_USED(handle);

  DEBUGF("Drag: Drag terminated with box %d,%d,%d,%d\n",
        event->user_drag_box.bbox.xmin, event->user_drag_box.bbox.ymin,
        event->user_drag_box.bbox.xmax, event->user_drag_box.bbox.ymax);

  check_error(_drag_finished());

  return 1; /* claim event */
}

/* -----------------------------------------------------------------------
                         Miscellaneous internal functions
*/

static void check_error(_Optional CONST _kernel_oserror *e)
{
#ifdef CBLIB_OBSOLETE
  (void)err_check(e);
#else
  if (e != NULL && report)
    report(&*e);
#endif
}

/* ----------------------------------------------------------------------- */

#ifdef COPY_ARRAY_ARGS
static CONST _kernel_oserror *lookup_error(const char *token)
{
  /* Look up error message from the token, outputting to an internal buffer */
  return messagetrans_error_lookup(desc, DUMMY_ERRNO, token, 0);
}
#endif

/* ----------------------------------------------------------------------- */

static SchedulerTime _drag_null_event_handler(void *handle,
  SchedulerTime new_time, const volatile bool *time_up)
{
  /* This is a handler for null events (generated when system otherwise idle) */
  NOT_USED(handle);
  NOT_USED(new_time);
  NOT_USED(time_up);

  if (!drag_finished)
  {
    /* Update our cached mouse pointer position */
    check_error(_drag_get_pointer_info());

    /* Send a Dragging message to the drag claimant or else
       the owner of the window at the mouse pointer */
    check_error(_drag_send_dragging_msg());
  }
  return new_time + 25; /* don't return until 1/4 of a second has elapsed */
}

/* ----------------------------------------------------------------------- */

static _Optional CONST _kernel_oserror *_drag_send_dragging_msg(void)
{
  _Optional CONST _kernel_oserror *e = NULL;
  WimpMessage message;
  WimpDraggingMessage *dragging = (WimpDraggingMessage *)&message.data;

  message.hdr.action_code = Wimp_MDragging;
  dragging->window_handle = pointer.window_handle;
  dragging->icon_handle = pointer.icon_handle;
  dragging->x = pointer.x;
  dragging->y = pointer.y;
  dragging->flags = Wimp_MDragging_DataFromSelection |
                    (drag_aborted ? Wimp_MDragging_DoNotClaimMessage : 0);
  DEBUGF("Drag: Dragging message flags are %d\n", dragging->flags);
  if (client_data_bbox != NULL)
  {
    /* Our client has supplied a bounding box for their data */
    dragging->bbox = *client_data_bbox;
    DEBUGF("Drag: Bounding box in Dragging message is %d,%d,%d,%d\n",
          dragging->bbox.xmin, dragging->bbox.ymin, dragging->bbox.xmax,
          dragging->bbox.ymax);
  }
  else
  {
    DEBUGF("Drag: No bounding box in Dragging message\n");
    dragging->bbox.xmin = 0;
    dragging->bbox.xmax = -1; /* x0 > x1 indicates no bounding box */
  }

  /* Copy list of file types into message body (-1 for terminator) */
  size_t const array_len = copy_file_types(dragging->file_types,
    client_file_types ? &*client_file_types : &(int){FileType_Null},
    ARRAY_SIZE(dragging->file_types) - 1) + 1;

  message.hdr.size = WORD_ALIGN(sizeof(message.hdr) +
    offsetof(WimpDraggingMessage, file_types) +
    sizeof(dragging->file_types[0]) * array_len);

  if (dragclaim_task)
  {
    DEBUGF("Drag: Sending recorded Dragging message to claimant %d\n",
          dragclaim_task);

    message.hdr.your_ref = dragclaim_msg_ref; /* from claimant's last message */
    e = wimp_send_message(Wimp_EUserMessageRecorded, &message,
                          dragclaim_task, 0, NULL);
  }
  else
  {
    DEBUGF("Drag: Sending%s Dragging message to unknown task\n",
           drag_finished ? " recorded" : "");

    message.hdr.your_ref = 0; /* not a reply */
    e = wimp_send_message(drag_finished ?
                            Wimp_EUserMessageRecorded :
                            Wimp_EUserMessage,
                          &message,
                          pointer.window_handle,
                          pointer.icon_handle,
                          NULL);
  }

  if (e == NULL)
  {
    DEBUGF("Drag: Sent Dragging message (ref. %d in reply to %d)\n",
          message.hdr.my_ref, message.hdr.your_ref);

    dragging_msg_ref = message.hdr.my_ref;
  }
  else if (drag_finished)
  {
    /* The drag has already finished and we can't communicate with the drag
       destination so clean up. */
    (void)_drag_delete_claim();
    _drag_free_mem();
  }

  return e;
}

/* ----------------------------------------------------------------------- */

static _Optional CONST _kernel_oserror *_drag_call_wdb_handler(DragBoxOp action)
{
  if (action == DragBoxOp_Start)
  {
    DEBUGF("Drag: Asking client to start Wimp drag box\n");
  }
  else if (action == DragBoxOp_Hide)
  {
    DEBUGF("Drag: Asking client to hide Wimp drag box\n");
  }
  else
  {
    assert(action == DragBoxOp_Cancel);
    DEBUGF("Drag: Asking client to cancel Wimp drag box\n");
  }

  assert(client_fn_box);
  return client_fn_box(action,
                       solid_drags,
                       pointer.x,
                       pointer.y,
                       client_drag_data);
}

/* ----------------------------------------------------------------------- */

static bool _drag_call_drop_handler(int file_type)
{
  if (!client_fn_drop)
    return false;

  return client_fn_drop(shift_held, pointer.window_handle, pointer.icon_handle,
    pointer.x, pointer.y, file_type, dragclaim_task,
    dragclaim_task ? dragclaim_msg_ref : 0, client_drag_data);
}

/* ----------------------------------------------------------------------- */

static void _drag_free_mem(void)
{
#ifdef COPY_ARRAY_ARGS
  /* Free memory all claimed for a drag operation */
  if (client_file_types != NULL)
  {
    DEBUGF("Drag: Freeing file types list\n");
    free(client_file_types);
    client_file_types = NULL;
  }
  if (client_data_bbox != NULL)
  {
    DEBUGF("Drag: Freeing bounding box\n");
    free(client_data_bbox);
    client_data_bbox = NULL;
  }
#endif
}

/* ----------------------------------------------------------------------- */

static _Optional CONST _kernel_oserror *_drag_get_pointer_info(void)
{
  DEBUGF("Drag: Updating cached mouse pointer info\n");
  return wimp_get_pointer_info(&pointer);
}

/* ----------------------------------------------------------------------- */

static void _drag_reset_ptr(void)
{
  DEBUGF("Drag: Resetting mouse pointer shape\n");
  _kernel_oscli("Pointer 1");
}

/* ----------------------------------------------------------------------- */

static bool _drag_delete_claim(void)
{
  bool claim_deleted = false;
  if (dragclaim_task)
  {
    DEBUGF("Drag: Drag relinquished by task %d (last message ref. %d)\n",
          dragclaim_task, dragclaim_msg_ref);
    dragclaim_task = 0; /* forget task handle of former claimant */

    /* Reset the mouse pointer shape if a drag claimant has altered it */
    if (TEST_BITS(drag_flags, Wimp_MDragClaim_PtrShapeChanged))
      _drag_reset_ptr();

    claim_deleted = true;
  }
  return claim_deleted;
}

/* ----------------------------------------------------------------------- */

static _Optional CONST _kernel_oserror *_drag_finished(void)
{
  _Optional CONST _kernel_oserror *return_error = NULL;

  /* Is a drag currently in progress? */
  if (drag_finished)
    return NULL; /* nothing to do */

  DEBUGF("Drag: Finishing drag\n");
  drag_finished = true;

  /* Record the final mouse position so that it is consistent in subsequent
     Dragging messages and when passed to the DragFinishedHandler */
  MERGE_ERR(return_error, _drag_get_pointer_info());

  MERGE_ERR(return_error, _drag_call_wdb_handler(DragBoxOp_Cancel));

  scheduler_deregister(_drag_null_event_handler, client_drag_data);

  /* Ensure that the drag claimant's representation of our data is
     consistent with the final mouse position */
  MERGE_ERR(return_error, _drag_send_dragging_msg());

  return return_error;
}
