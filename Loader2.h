/*
 * CBLibrary: Handle the receiver's half of the data transfer message protocol
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

/* Loader2.h declares several types and functions that allow a client program to
   delegate the sender's half of the data transfer protocol to this library
   module. A client-supplied function is called to notify when data has been
   loaded (or failure).

Dependencies: ANSI C library, Acorn library kernel, Acorn's WIMP, toolbox,
              event & flex libraries.
Message tokens: NoMem, OpenInFail, ReadFail.
History:
  CJB: 12-Oct-06: Created this header file.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
                  First public release version.
  CJB: 27-Oct-06: Minor changes to documentation.
  CJB: 11-Oct-09: Added an argument to the prototype of loader2_initialise.
  CJB: 15-Oct-09: Added "NoMem" to list of required message tokens.
  CJB: 11-Dec-14: Deleted redundant brackets from function type definitions.
  CJB: 17-Dec-14: Made the arguments to loader2_initialise conditional upon
                  CBLIB_OBSOLETE.
  CJB: 11-Dec-20: Deleted redundant uses of the 'extern' keyword.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
*/

#ifndef Loader2_h
#define Loader2_h

/* ISO library headers */
#include <stdbool.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "flex.h"
#include "toolbox.h"

/* Local headers */
#include "Macros.h"

#if !defined(USE_OPTIONAL) && !defined(_Optional)
#define _Optional
#endif

/* ---------------- Client-supplied participation routines ------------------ */

typedef _Optional CONST _kernel_oserror *Loader2FileHandler (const char * /*file_path*/,
                                                             flex_ptr     /*buffer*/);
/*
 * This function is called to request for a new flex block to be allocated
 * (anchored at 'buffer') and the file at 'file_path' loaded into it. To be used
 * as a wrapper around code that insists on reading from file instead of a
 * buffer.
 * Return: a pointer to an OS error block, or else NULL for success.
 */

typedef void Loader2FinishedHandler (_Optional CONST _kernel_oserror * /*load_error*/,
                                     int                     /*file_type*/,
                                     void       *_Optional * /*buffer*/,
                                     void                  * /*client_handle*/);
/*
 * This function is called when a load operation has been completed or has
 * irretrievably broken down. In the latter case, the file type will be -1.
 * If an error occurred whilst trying to load the data then 'load_error' will
 * point to an OS error block; otherwise it will be NULL. If successful then
 * 'buffer' will point to the anchor of a flex block containing the data that
 * was loaded and 'file_type' will reflect the type of received data. It is the
 * responsibility of your function to either re-anchor or free this block.
 * 'client_handle' will be the pointer passed to loader_receive_data().
 */

/* --------------------------- Library functions ---------------------------- */

#ifdef CBLIB_OBSOLETE
_Optional CONST _kernel_oserror *loader2_initialise(void);
#else
_Optional CONST _kernel_oserror *loader2_initialise(_Optional MessagesFD */*mfd*/);
#endif
   /*
    * Initialises the Loader2 component and sets up handlers for DataLoad and
    * Wimp_MRAMTransmit messages. These are used to handle the data transfer
    * protocol on behalf of the client program. A handler for
    * UserMessageAcknowledge events is also registered. The event library's mask
    * is manipulated to allow UserMessage, UserMessageRecorded and
    * UserMessageAcknowledge events.
    * YOU MUST INITIALISE 'LOADER' BEFORE 'LOADER2' OR ELSE IT WILL INTERCEPT
    *                MESSAGES REQUIRED BY THIS COMPONENT!
    * Unless 'mfd' is a null pointer, the specified messages file will be given
    * priority over the global messages file when looking up text required by
    * this module.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

_Optional CONST _kernel_oserror *loader2_finalise(void);
   /*
    * Deregisters the Loader component's event handlers and releases any memory
    * claimed by this library component. Any incomplete load operations will be
    * terminated abruptly. Note that this function is not normally included in
    * pre-built library distributions.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

_Optional CONST _kernel_oserror *loader2_receive_data(
                                const WimpMessage         * /*data_save*/,
                                Loader2FileHandler        * /*load_method*/,
                                Loader2FinishedHandler    * /*finished_method*/,
                                void                      * /*client_handle*/);
   /*
    * Sends a RAMFetch or DataSaveAck message in reply to the specified
    * DataSave message. If the second argument is NULL then the default
    * Loader2FileHandler will be used ('loader2_buffer_file'). Otherwise, the
    * client's Loader2FileHandler will be invoked when it is time to load data
    * from a file. This is deprecated because it prevents RAM transfer between
    * tasks. If a LoaderFinishedHandler function is specified then this will
    * be called when the load operation is complete or has irretrievably broken
    * down; 'client_handle' is an opaque value that will be passed to it.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

void loader2_cancel_receives(Loader2FinishedHandler * /*finished_method*/,
                             void                   * /*client_handle*/);
   /*
    * Cancels any outstanding load operations to the specified client function
    * and handle. Use when the destination has become invalid, e.g. because a
    * document is being closed.
    */

Loader2FileHandler loader2_buffer_file;
   /*
    * This is the default Loader2FileHandler function, which is used if no
    * alternative was registered with loader2_receive_data() and the data saving
    * task does not support RAM transfer. It loads the file specified by
    * 'file_path' into a newly allocated flex block, the address of which is
    * stored in the supplied 'buffer' anchor.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

#endif
