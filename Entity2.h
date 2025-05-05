/*
 * CBLibrary: Manage ownership of system-wide entities (e.g. the clipboard)
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

/* Entity2.h declares several types and functions that allow system-wide
   entities (such as the clipboard and caret) to be claimed and released.

Dependencies: Acorn's WIMP, event & flex libraries.
Message tokens: NoMem, EntitySendFail, Entity<n>NoData (where 0 <= n <= 7).
History:
  CJB: 08-Oct-06: Created this header file from <Entity.h>.
  CJB: 10-Nov-19: Pass the leaf name instead of "<Wimp$Scrap>" when calling
                  the Entity2ReadMethod. Pass the estimated file size as an
                  extra argument.
  CJB: 11-Dec-20: Deleted redundant uses of the 'extern' keyword.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
*/

#ifndef Entity2_h
#define Entity2_h

/* ISO library headers */
#include <stdbool.h>

/* Acorn C/C++ library headers */
#include "kernel.h"

/* StreamLib headers */
#include "Writer.h"

/* CBOSLib headers */
#include "WimpExtra.h"

/* Local headers */
#include "Loader3.h"
#include "Saver2.h"
#include "Macros.h"

#if !defined(USE_OPTIONAL) && !defined(_Optional)
#define _Optional
#endif

/* ---------------- Client-supplied participation routines ------------------ */

typedef int Entity2EstimateMethod (
  int    /*file_type*/,
  void * /*client_handle*/);
/*
 * This function is called to get the estimated size of data associated with
 * an entity when saved as a given file type. 'client_handle' is the pointer
 * passed to entity2_claim() and 'file_type' is a file type from the array
 * registered with entity2_claim().
 * Return: The estimated file size, in bytes.
 */

typedef void Entity2LostMethod (void * /*client_handle*/);
/*
 * This function is called when the current owner of an entity is displaced;
 * either by another application claiming control of that entity, or because
 * entity2_claim() has been called again. It should free any resources
 * associated with its ownership of that entity (e.g. clipboard data) and
 * remove or fade any visual display of that entity (e.g. caret or selection).
 */

typedef void Entity2ExitMethod (void);
/*
 * This function is called by entity2_dispose_all() if no entities are owned
 * by our application, or alternatively when any data requests resulting from
 * a ReleaseEntity broadcast have either been satisfied or failed. Typically
 * it should exit the program and never return.
 */

typedef void Entity2ProbeMethod (
  int    /*file_type*/,
  void * /*client_handle*/);
/*
 * This function is called in order to deliver the results of a probe when
 * data becomes available. 'file_type' is the type of the data, which may be
 * none of those in the requester's list of preferred types. 'client_handle' is
 * the address registered with entity2_probe_data().
 */

typedef bool Entity2ReadMethod (
  Reader     * /*reader*/,
  int          /*estimated_size*/,
  int          /*file_type*/,
  const char * /*leaf_name*/,
  void * /*client_handle*/);
/*
 * This function is called in order to deliver data associated with an entity.
 * It reads data from the given 'reader' object. 'file_type' is the type of the
 * data to read, which may be none of those in the requester's list of
 * preferred types. 'leaf_name' is the leaf name of the entity data.
 * The 'estimated_size' may be the actual size in bytes (but don't rely on it).
 * 'client_handle' is the address registered with entity2_request_data().
 * Returns: true on success or false on failure.
 */

typedef void Entity2FailedMethod(_Optional CONST _kernel_oserror * /*error*/,
  void * /*client_handle*/);
/*
 * This function is called when an entity data request operation has failed.
 * If an error occurred then 'error' will point to an OS error block;
 * otherwise it will be NULL. 'client_handle' is the address registered with
 * entity2_probe_data() or entity2_request_data().
 */

/* --------------------------- Library functions ---------------------------- */

_Optional CONST _kernel_oserror *entity2_initialise(
  _Optional MessagesFD */*mfd*/,
  void (*/*error_method*/)(CONST _kernel_oserror *)
);
   /*
    * Initialises the Entity2 component and registers WIMP message handlers
    * for DataRequest and ClaimEntity. These are used to negotiate ownership of
    * system-wide entities (such as the clipboard) on behalf of the client
    * program. A WIMP event handler for UserMessageAcknowledge is also
    * registered. The event library's mask is manipulated to allow UserMessage,
    * UserMessageRecorded and UserMessageAcknowledge events.
    * YOU MUST INITIALISE 'LOADER' BEFORE 'ENTITY2' OR ELSE IT WILL INTERCEPT
    *                MESSAGES REQUIRED BY THIS COMPONENT!
    * Unless 'mfd' is a null pointer, the specified messages file will be given
    * priority over the global messages file when looking up text required by
    * this module. Unless 'error_method' is a null pointer, it should point to
    * a function to be called if an error occurs whilst handling an event.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

_Optional CONST _kernel_oserror *entity2_claim(
  unsigned int                      /*flags*/,
  _Optional const int             * /*file_types*/,
  _Optional Entity2EstimateMethod * /*estimate_method*/,
  _Optional Saver2WriteMethod     * /*write_method*/,
  _Optional Entity2LostMethod     * /*lost_method*/,
  void                            * /*client_handle*/);
   /*
    * Claims possession of the entities represented by bits set in the 'flags'
    * word (as for a ClaimEntity message) and registers functions to be called
    * when those entities are lost or data is requested for them. If necessary
    * a message is broadcast to inform other tasks that we are claiming the
    * specified entities. 'file_types' must point to a list of file types that
    * the entity data can be delivered in, terminated by FileType_Null.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

_Optional CONST _kernel_oserror *entity2_request_data(
  const WimpDataRequestMessage  * /*data_request*/,
  _Optional Entity2ReadMethod   * /*read_method*/,
  _Optional Entity2FailedMethod * /*failed_method*/,
  void                          * /*client_handle*/);
   /*
    * This function requests any data associated with an entity specified in
    * a given DataRequest message, which must include an array of preferred
    * file types, in order of preference (terminated by FileType_Null).
    * The message header is filled out automatically. It is advisable to
    * specify a window handle in case the owner of the requested entity uses
    * it when replying to the DataRequest. If the entity is owned by the client
    * task then no message is sent and the request is handled synchronously;
    * otherwise the DataRequest is broadcast.
    * 'read_method' specifies a function to be called back with the given
    * 'client_handle' in order to deliver data when it becomes available;
    * otherwise 'failed_method' is called when the data transfer fails.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

_Optional CONST _kernel_oserror *entity2_probe_data(
  const WimpDataRequestMessage  * /*data_request*/,
  _Optional Entity2ProbeMethod  * /*probe_method*/,
  _Optional Entity2FailedMethod * /*failed_method*/,
  void                          * /*client_handle*/);
   /*
    * This function probes any data associated with an entity specified in
    * a given DataRequest message, which must include an array of preferred
    * file types, in order of preference (terminated by FileType_Null).
    * The message header is filled out automatically. It is advisable to
    * specify a window handle in case the owner of the requested entity uses
    * it when replying to the DataRequest. If the entity is owned by the client
    * task then no message is sent and the probe is handled synchronously;
    * otherwise the DataRequest is broadcast.
    * 'probe_method' specifies a function to be called back with the given
    * 'client_handle' in order to report when data becomes available; otherwise
    * 'failed_method' is called when the data transfer fails.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

_Optional CONST _kernel_oserror *entity2_dispose_all(Entity2ExitMethod * /*exit_method*/);
   /*
    * If our task owns any entities then this function broadcasts a message to
    * inform other tasks that this is their last chance to request the
    * associated data. In this case, the function will return early without
    * having called 'exit_method' because finalisation is still pending.
    * If no entities are owned then the specified Entity2ExitMethod function
    * (if any) will be called directly.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

void entity2_cancel_requests(void * /*client_handle*/);
   /*
    * Cancels any outstanding data requests for the specified
    * client handle. Use when the destination has become invalid,
    * e.g. because a document is being closed.
    */

void entity2_release(unsigned int /*flags*/);
   /*
    * Relinquishes possession of the entities represented by bits set in the
    * flags word (as for a ClaimEntity message). Any Entity2LostMethod
    * functions registered when those entities were claimed will be called.
    */

_Optional CONST _kernel_oserror *entity2_finalise(void);
   /*
    * Deregisters the Entity2 component's event handlers and releases any memory
    * claimed by this library component. It also releases any entities that
    * were claimed by the client program, and calls the relevant
    * Entity2LostMethod functions. Note that this function is not normally
    * included in pre-built library distributions.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

#endif
