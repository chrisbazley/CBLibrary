/*
 * CBLibrary: Handle the sender's half of the data transfer protocol
 * Copyright (C) 2019 Christopher Bazley
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

/* Saver2.h declares types and functions that allow a client program to
   delegate the sender's half of the data transfer protocol to this library
   module. Client-supplied functions are called to provide a graphical
   representation of the data during the drag and produce data to be saved.

Dependencies: ANSI C library, Acorn library kernel, Acorn's WIMP, event & flex
              libraries.
Message tokens: NoMem, RecDied, OpenOutFail.
History:
  CJB: 22-Sep-19: Created this header file from <Saver.h>.
  CJB: 11-Dec-20: Deleted redundant uses of the 'extern' keyword.
*/

#ifndef Saver2_h
#define Saver2_h

/* ISO library headers */
#include <stdbool.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "toolbox.h"
#include "wimp.h"

/* StreamLib headers */
#include "Writer.h"

/* Local headers */
#include "Macros.h"

/* ---------------- Client-supplied participation routines ------------------ */

typedef bool Saver2WriteMethod(
  Writer     * /*writer*/,
  int          /*file_type*/,
  const char * /*filename*/,
  void       * /*client_handle*/);
/*
 * This function is called in order to get data when it becomes required.
 * It writes data via the given 'writer' object. 'file_type' is the type of
 * the data to write (from the DataSave message). 'filename' is the full path
 * or leaf name of the output file (which should be used only to report error
 * messages to the user). 'client_handle' is the address registered
 * with saver2_send_data().
 * If this function returns true but an error is detected upon destroying
 * the 'writer' object (because buffered data was not successfully flushed)
 * then the caller reports an error.
 * Returns: true on success or false on failure.
 */

typedef void Saver2FailedMethod(CONST _kernel_oserror * /*error*/,
  void * /*client_handle*/);
/*
 * This function is called when a save operation has failed.
 * If an error occurred then 'error' will point to an OS error block;
 * otherwise it will be NULL. 'client_handle' is the address
 * registered with saver2_send_data().
 */

typedef void Saver2CompleteMethod(
  int          /*file_type*/,
  const char * /*file_path*/,
  int          /*datasave_ref*/,
  void       * /*client_handle*/);
/*
 * This function is called when a save operation has completed successfully.
 * 'file_type' is the type of the saved data (from the DataSave message).
 * If the destination was safe then 'file_path' is the full path to which
 * the data was saved (as reported by the recipient); otherwise NULL.
 * 'datasave_ref' is the ID of the DataSave message that we sent initially.
 * It can be matched against any subsequent DataSaved message received.
 * 'client_handle' is the address registered with saver2_send_data().
 */

/* --------------------------- Library functions ---------------------------- */

CONST _kernel_oserror *saver2_initialise(
  int /*task_handle*/, MessagesFD * /*mfd*/);
   /*
    * Initialises the Saver2 component and registers Wimp message handlers for
    * DataSaveAck, DataLoadAck and RAMFetch. These are used to handle the data
    * transfer protocol on behalf of the client program. A Wimp event handler
    * for UserMessageAcknowledge is also registered. The event library's mask
    * is manipulated to allow UserMessage, UserMessageRecorded and
    * UserMessageAcknowledge events. Unless 'mfd' is a null pointer, the
    * specified messages file is given priority over the global messages file
    * when looking up text required by this module. Unless 'error_method' is a
    * null pointer, it should point to a function to be called if an error
    * occurs whilst handling an event.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

CONST _kernel_oserror *saver2_finalise(void);
   /*
    * Deregisters the Saver2 component's event handlers and releases any memory
    * claimed by this library component. Any incomplete save operations are
    * terminated abruptly (and the relevant Saver2WriteMethod functions will
    * be called). Note that this function is not normally included in pre-built
    * library distributions.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

CONST _kernel_oserror *saver2_send_data(
  int                     /*task_handle*/,
  WimpMessage           * /*message*/,
  Saver2WriteMethod     * /*write_method*/,
  Saver2CompleteMethod  * /*complete_method*/,
  Saver2FailedMethod    * /*failed_method*/,
  void                  * /*client_handle*/);
   /*
    * Sends a DataSave message to the task specified by 'task_handle'. The
    * action code and message size are filled out automatically. If
    * 'task_handle' is 0 then the message will instead be sent to the specified
    * window handle.
    * The specified 'write_method' is called back with the given 'client_handle'
    * in order to provide data when it is required. Either 'complete_method' or
    * 'failed_method' is called depending on whether the data transfer completes
    * or fails.
    * Returns: a pointer to an OS error block, or else NULL for success.
    *          If an error is returned then 'failed_method' will not be called.
    */

void saver2_cancel_sends(void * /*client_handle*/);
   /*
    * Cancels any outstanding save operations for the specified client handle.
    * Use when the source has become invalid, e.g. because a document is being
    * closed.
    */

#endif
