/*
 * CBLibrary: Handle the sender's half of the data transfer protocol
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

/* Saver.h declares several types and functions that allow a client program to
   delegate the sender's half of the data transfer protocol to this library
   module. Client-supplied functions are called to provide a graphical
   representation of the data during the drag, produce the data to be saved,
   and notify when data has been saved (or failure).

Dependencies: ANSI C library, Acorn library kernel, Acorn's WIMP, event & flex
              libraries.
Message tokens: NoMem, RecDied, OpenOutFail, WriteFail.
History:
  CJB: 09-Aug-06: Created this header file from scratch.
  CJB: 13-Sep-06: First release version.
  CJB: 25-Sep-06: Added support for global clipboard.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
                  Completely redefined the client participation functions.
                  Added declarations of new functions saver_send_data() and
                  saver_cancel_sends(). Removed declarations of old functions
                  saver_start_drag() and saver_abort_drag().
  CJB: 27-Oct-06: Minor changes to documentation.
  CJB: 27-Jan-08: Reduced the number of arguments to saver_send_data by
                  passing a pointer to a WimpMessage structure instead of
                  individually specifying the file type, leaf name, drag
                  destination and message ID to reply to.
  CJB: 12-Oct-09: Added an extra argument to the prototype of saver_initialise.
  CJB: 15-Oct-09: Added "NoMem" to list of required message tokens.
  CJB: 11-Dec-14: Deleted redundant brackets from function type definitions.
  CJB: 26-Dec-14: Corrected the list of required message tokens.
  CJB: 13-Oct-19: Corrected the description of saver_send_data.
  CJB: 11-Dec-20: Deleted redundant uses of the 'extern' keyword.
*/

#ifndef Saver_h
#define Saver_h

/* ISO library headers */
#include <stdbool.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "toolbox.h"
#include "flex.h"
#include "wimp.h"

/* Local headers */
#include "Macros.h"

/* ---------------- Client-supplied participation routines ------------------ */

typedef _kernel_oserror *SaverFileHandler (const char * /*file_path*/,
                                           flex_ptr     /*buffer*/,
                                           unsigned int /*start_offset*/,
                                           unsigned int /*end_offset*/);
/*
 * This function is called to request that some data held in the flex block
 * 'buffer' be saved to file path 'file_path'. To be used as a wrapper around
 * code that insists on outputting to file instead of a buffer. 'start_offset'
 * (inclusive) and 'end_offset' (exclusive) delimit the data to save.
 * Return: a pointer to an OS error block, or else NULL for success.
 */


typedef void SaverFinishedHandler (bool                    /*success*/,
                                   CONST _kernel_oserror * /*save_error*/,
                                   const char            * /*file_path*/,
                                   int                     /*datasave_ref*/,
                                   void                  * /*client_handle*/);
/*
 * This function is called when a save operation has been completed or has
 * irretrievably broken down (as indicated by boolean argument 'success').
 * If an error occurred whilst trying to save the data then 'save_error' will
 * point to an OS error block; otherwise it will be NULL. If the destination
 * was safe then 'file_path' will be the full path to which the data was saved
 * (as reported by the recipient); otherwise it will be NULL. 'datasave_ref' is
 * the ID of the DataSave message that we sent initially. It can be matched
 * against any subsequent DataSaved message received. 'client_handle' will be
 * the opaque value that was registered with saver_send_data().
 */

/* --------------------------- Library functions ---------------------------- */

CONST _kernel_oserror *saver_initialise(int         /*task_handle*/,
                                        MessagesFD */*mfd*/);
   /*
    * Initialises the Saver component and registers Wimp message handlers for
    * DataSaveAck, DataLoadAck and RAMFetch. These are used to handle the data
    * transfer protocol on behalf of the client program. A Wimp event handler
    * for UserMessageAcknowledge is also registered. The event library's mask
    * is manipulated to allow UserMessage, UserMessageRecorded and
    * UserMessageAcknowledge events. Unless 'mfd' is a null pointer, the
    * specified messages file will be given priority over the global messages
    * file when looking up text required by this module.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

CONST _kernel_oserror *saver_finalise(void);
   /*
    * Deregisters the Saver component's event handlers and releases any memory
    * claimed by this library component. Any incomplete save operations will be
    * terminated abruptly (and the relevant SaverFinishedHandler functions will
    * be called). Note that this function is not normally included in pre-built
    * library distributions.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

CONST _kernel_oserror *saver_send_data(int /*task_handle*/,
                                    WimpMessage * /*message*/,
                                    flex_ptr /*data*/,
                                    unsigned int /*start_offset*/,
                                    unsigned int /*end_offset*/,
                                    SaverFileHandler * /*save_method*/,
                                    SaverFinishedHandler * /*finished_method*/,
                                    void * /*client_handle*/);
   /*
    * Sends a DataSave message to the task specified by 'task_handle'. The
    * action code, message size and estimated file size are filled out
    * automatically. If 'task_handle' is 0 then the message will instead be
    * sent to the specified window handle. 'data' must point to the anchor of a
    * flex block containing the data to be saved; 'start_offset' (inclusive)
    * and 'end_offset' (exclusive) delimit the area to save.
    * If a SaverFileHandler function is specified then this will be invoked when
    * it is time to save the data to a file. This is deprecated because it
    * prevents RAM transfer between tasks. If a SaverFinishedHandler function is
    * specified then this will be called when the save operation is complete or
    * has irretrievably broken down; 'client_handle' is an opaque value that
    * will be passed to this function.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

void saver_cancel_sends(flex_ptr /*data*/);
   /*
    * Cancels any outstanding save operations for the specified data. Use when
    * the source has become invalid, e.g. because a document is being closed.
    */

#endif
