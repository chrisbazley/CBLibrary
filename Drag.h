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

/* Drag.h declares several types and functions that provide an interface to an
   implementation of the message protocol described in application note 241.
   This provides for seamless drag and drop between different applications.
   Client-supplied functions are called to provide a graphical representation of
   the data during the drag, and when the drag has terminated.

Dependencies: ANSI C library, Acorn library kernel, Acorn's WIMP & event
              libraries.
Message tokens: NoMem.
History:
  CJB: 08-Oct-06: Created this header file from scratch.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
                  First public release version.
  CJB: 12-Nov-06: Minor correction to documentation.
  CJB: 30-Sep-09: Updated documentation to refer to FileType_Null instead of -1.
  CJB: 11-Oct-09: Renamed the members of the 'DragBoxOp' enumeration. Added
                  extra arguments to the prototype of drag_initialise. Added
                  "NoMem" to list of required message tokens.
  CJB: 26-Jun-10: Made definition of deprecated constant names conditional
                  upon definition of CBLIB_OBSOLETE.
  CJB: 05-May-12: Made the arguments to drag_initialise conditional upon
                  CBLIB_OBSOLETE.
  CJB: 11-Dec-14: Deleted redundant brackets from function type definitions.
  CJB: 12-Oct-15: Corrected parameter name in DragFinishedHandler description.
  CJB: 11-Dec-20: Deleted redundant uses of the 'extern' keyword.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
*/

#ifndef Drag_h
#define Drag_h

/* ISO library headers */
#include <stdbool.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "wimp.h"
#include "toolbox.h"

/* Local headers */
#include "Macros.h"

#if !defined(USE_OPTIONAL) && !defined(_Optional)
#define _Optional
#endif

/* ---------------- Client-supplied participation routines ------------------ */

typedef enum
{
  DragBoxOp_Start,
  DragBoxOp_Cancel,
  DragBoxOp_Hide
}
DragBoxOp;

typedef _Optional CONST _kernel_oserror *DragBoxHandler (DragBoxOp /*action*/,
                                                         bool   /*solid_drags*/,
                                                         int    /*mouse_x*/,
                                                         int    /*mouse_y*/,
                                                         void * /*client_handle*/);
/*
 * This function is called to start, hide, or cancel a Wimp drag operation
 * (as indicated by the DragBoxOp code). Typically Wimp_DragBox with drag
 * type 5 is used for DragBoxOp_Start, drag type 7 for DragBoxOp_Hide and
 * Wimp_DragBox -1 for DragBoxOp_Cancel. You may wish to use the DragASprite or
 * DragAnObject module instead of Wimp_DragBox, if 'solid_drags' is true.
 * 'mouse_x' and 'mouse_y' give the current mouse position and 'client_handle'
 * will be the pointer passed to drag_start().
 * Return: a pointer to an OS error block, or else NULL for success.
 */

typedef bool DragFinishedHandler (bool   /*shift_held*/,
                                  int    /*window*/,
                                  int    /*icon*/,
                                  int    /*mouse_x*/,
                                  int    /*mouse_y*/,
                                  int    /*file_type*/,
                                  int    /*claimant_task*/,
                                  int    /*claimant_ref*/,
                                  void * /*client_handle*/);
/*
 * This function is called when a drag terminates because the user released all
 * mouse buttons. 'client_handle' will be the pointer passed to drag_start() and
 * 'shift_held' will be true if the Shift key was held at the start of the drag.
 * The drop location is given by 'window', 'icon', 'mouse_x' and 'mouse_y'
 * (which can used to construct a DataSave message). If the drag was being
 * claimed by a task then 'claimant_task' will be its handle and 'claimant_ref'
 * will be the ID of its last DragClaim message; otherwise 'claimant_task' will
 * be 0. The file type negotiated with the drag claimant (if any) is
 * 'file_type'; it will be one of those on the list passed to drag_start.
 * If this function returns false then a Dragging message with flags bit 4 set
 * will be sent force the claimant (if any) to relinquish the drag.
 * Return: true if a DataSave message was sent to the drag claimant.
 */

/* --------------------------- Library functions ---------------------------- */

_Optional CONST _kernel_oserror *drag_initialise(
#ifdef CBLIB_OBSOLETE
                       void
#else
                       _Optional MessagesFD */*mfd*/,
                       void (*/*report_error*/)(CONST _kernel_oserror *)
#endif
);
   /*
    * Initialises the Drag component and registers Wimp message handlers for
    * DragClaim and event handlers for UserMessageAcknowledge and UserDrag.
    * These are used to participate in the drag and drop protocol on behalf of
    * the client program. The event library's mask is manipulated to allow
    * UserMessage, UserMessageRecorded and UserMessageAcknowledge events.
    * Unless 'mfd' is a null pointer, the specified messages file will be given
    * priority over the global messages file when looking up text required by
    * this module. Unless 'report_error' is a null pointer, it should point to
    * a function to be called if an error occurs whilst handling an event.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

_Optional CONST _kernel_oserror *drag_abort(void);
   /*
    * Forces termination of any drag currently in progress (for use when the
    * user presses the Escape key or the source data becomes invalid). The
    * client's DragDragBoxHandler function is called to cancel the Wimp drag
    * operation, and a final Dragging message is sent to force any drag
    * claimant to relinquish the drag. Null events may be masked out if there
    * are no other clients of the Scheduler.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

_Optional CONST _kernel_oserror *drag_finalise(void);
   /*
    * Deregisters the Drag component's event handlers and releases all memory
    * claimed by this library component. A drag in progress will be aborted as
    * for drag_abort(), except that the mouse pointer shape will be reset
    * immediately (if it had been altered by the drag claimant). Note that this
    * function is not normally included in pre-built library distributions.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

_Optional CONST _kernel_oserror *drag_start(const int                     * /*file_types*/,
                                            _Optional const BBox          * /*data_bbox*/,
                                            DragBoxHandler                * /*drag_box_method*/,
                                            _Optional DragFinishedHandler * /*drop_method*/,
                                            void                          * /*client_handle*/);
   /*
    * Starts a drag operation. Whilst mouse button(s) are held down, a
    * representation of the dragged data will follow the pointer around the
    * screen and compliant applications will auto-scroll their windows when it
    * lingers over them. 'file_types' must point to a list of file types that
    * the dragged data can be delivered in, terminated by FileType_Null.
    * 'data_bbox' may be NULL; otherwise it points to the bounding box of the
    * data relative to the mouse pointer, in units of 1/72000 inch.
    * The DragBoxHandler function will be called to start, hide, or cancel the
    * Wimp drag box. The DragFinishedHandler function (if any) will be called
    * when the user releases all mouse buttons. 'client_handle' is an opaque
    * value that will be passed to the client's functions.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

#ifdef CBLIB_OBSOLETE
/* Deprecated enumeration constant names */
#define DRAG_BOX_START  DragBoxOp_Start
#define DRAG_BOX_CANCEL DragBoxOp_Cancel
#define DRAG_BOX_HIDE   DragBoxOp_Hide
#endif /* CBLIB_OBSOLETE */

#endif /* CSV_h */
