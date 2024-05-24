/*
 * CBLibrary: Handle the receiver's half of the data transfer message protocol
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

/* Loader3.h declares types and functions that allow a client program to
   delegate the sender's half of the data transfer protocol to this library
   module. A client-supplied function is called to notify when data has been
   received.

Dependencies: ANSI C library, Acorn library kernel, Acorn's WIMP, toolbox,
              event & flex libraries.
Message tokens: NoMem, StrOFlo, OpenInFail.
History:
  CJB: 22-Sep-19: Created this header file from <Loader2.h>.
  CJB: 09-Nov-19: Pass the leaf name instead of "<Wimp$Scrap>" when calling
                  the Loader3ReadMethod. Pass the estimated file size as an
                  extra argument.
  CJB: 07-Nov-20: Added the loader3_load_file function to allow DataOpen and
                  DataLoad handlers to reuse existing code.
  CJB: 11-Dec-20: Deleted redundant uses of the 'extern' keyword.
*/

#ifndef Loader3_h
#define Loader3_h

/* ISO library headers */
#include <stdbool.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "toolbox.h"

/* StreamLib headers */
#include "Reader.h"

/* Local headers */
#include "Macros.h"

/* ---------------- Client-supplied participation routines ------------------ */

typedef bool Loader3ReadMethod (
  Reader     * /*reader*/,
  int          /*estimated_size*/,
  int          /*file_type*/,
  const char * /*leaf_name*/,
  void       * /*client_handle*/);
/*
 * This function is called in order to deliver data when it becomes available.
 * It reads data from the given 'reader' object. 'file_type' is the type of the
 * data to read and 'leaf_name' is its leaf name (from the DataSave message).
 * The 'estimated_size' may be the actual size in bytes (but don't rely on it).
 * 'client_handle' is the address registered with loader3_receive_data().
 * Returns: true on success or false on failure.
 */

typedef void Loader3FailedMethod(CONST _kernel_oserror * /*error*/,
  void * /*client_handle*/);
/*
 * This function is called when a load operation has failed.
 * If an error occurred then 'error' will point to an OS error block;
 * otherwise it will be NULL. 'client_handle' is the address registered with
 * loader3_receive_data().
 */

/* --------------------------- Library functions ---------------------------- */

CONST _kernel_oserror *loader3_initialise(MessagesFD * /*mfd*/);
   /*
    * Initialises the Loader3 component and sets up handlers for DataLoad and
    * Wimp_MRAMTransmit messages. These are used to handle the data transfer
    * protocol on behalf of the client program. A handler for
    * UserMessageAcknowledge events is also registered. The event library's mask
    * is manipulated to allow UserMessage, UserMessageRecorded and
    * UserMessageAcknowledge events.
    * YOU MUST INITIALISE 'LOADER' BEFORE 'LOADER3' OR ELSE IT WILL INTERCEPT
    *                MESSAGES REQUIRED BY THIS COMPONENT!
    * Unless 'mfd' is a null pointer, the specified messages file is given
    * priority over the global messages file when looking up text required by
    * this module.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

CONST _kernel_oserror *loader3_finalise(void);
   /*
    * Deregisters the Loader component's event handlers and releases any memory
    * claimed by this library component. Any incomplete load operations are
    * terminated abruptly. Note that this function is not normally included in
    * pre-built library distributions.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

CONST _kernel_oserror *loader3_receive_data(
  const WimpMessage   * /*message*/,
  Loader3ReadMethod   * /*read_method*/,
  Loader3FailedMethod * /*failed_method*/,
  void                * /*client_handle*/);
   /*
    * Sends a RAMFetch or DataSaveAck message in reply to the specified
    * DataSave message.
    * The specified 'read_method' is called back with the given 'client_handle'
    * in order to deliver data when it becomes available, otherwise
    * 'failed_method' is called. It's guaranteed that both are not called.
    * Returns: a pointer to an OS error block, or else NULL for success.
    *          If an error is returned then 'failed_method' will not be called.
    */

void loader3_cancel_receives(void * /*client_handle*/);
   /*
    * Cancels any outstanding load operations to the specified client handle.
    * Use when the destination has become invalid, e.g. because a document is
    * being closed.
    */

bool loader3_load_file(
  const char *          /*file_name*/,
  int                   /*file_type*/,
  Loader3ReadMethod   * /*read_method*/,
  Loader3FailedMethod * /*failed_method*/,
  void                * /*client_handle*/);
   /*
    * Loads the contents of a file with the specified 'file_name' (which
    * (must be a full path). If successful then the specified 'read_method'
    * is called back with the given 'file_type', 'file_name' and
    * 'client_handle' in order to deliver data, otherwise 'failed_method' is
    * called. It's guaranteed that exactly one of those functions is called.
    * Returns: true on success or false on failure.
    */


#endif
