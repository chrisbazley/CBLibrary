/*
 * CBLibrary: Manage ownership of system-wide entities (e.g. the clipboard)
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

/* Entity.h declares several types and functions that allow system-wide
   entities (such as the clipboard and caret) to be claimed and released.

Dependencies: Acorn's WIMP, event & flex libraries.
Message tokens: NoMem, EntitySendFail, Entity<n>NoData (where 0 <= n <= 7).
History:
  CJB: 08-Oct-06: Created this header file from scratch.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 16-Oct-06: First public release version.
  CJB: 27-Oct-06: Minor changes to documentation.
  CJB: 22-Jun-09: No longer require message 'SaveFail'; now require new messages
                  'EntitySendFail' and 'Entity...NoData'. Removed documented
                  restriction that only one bit could be set in the flags value
                  passed to the entity_{request|probe}_data() functions.
  CJB: 30-Sep-09: Updated documentation to refer to FileType_Null instead of -1.
  CJB: 11-Oct-09: Added extra arguments to the prototype of entity_initialise.
                  Added "NoMem" to list of required message tokens.
  CJB: 26-Jun-10: Clarified the behaviour when a EntityDataMethod function
                  returns a null pointer.
  CJB: 05-May-12: Made the arguments to entity_initialise conditional upon
                  CBLIB_OBSOLETE.
  CJB: 11-Dec-14: Deleted redundant brackets from function type definitions.
  CJB: 11-Dec-20: Deleted redundant uses of the 'extern' keyword.
*/

#ifndef Entity_h
#define Entity_h

/* ISO library headers */
#include <stdbool.h>

/* Acorn C/C++ library headers */
#include "flex.h"
#include "kernel.h"

/* Local headers */
#include "Loader2.h"
#include "Macros.h"

/* ---------------- Client-supplied participation routines ------------------ */

typedef void EntityLostMethod (void * /*client_handle*/);
/*
 * This function is called when the current owner of an entity is displaced;
 * either by another application claiming control of that entity, or because
 * entity_claim() has been called again. It should free any resources
 * associated with its ownership of that entity (e.g. clipboard data) and
 * remove or fade any visual display of that entity (e.g. caret or selection).
 */

typedef flex_ptr EntityDataMethod (const int * /*pref_file_types*/,
                                   bool        /*probe_only*/,
                                   void      * /*client_handle*/,
                                   bool      * /*data_persists*/,
                                   int       * /*file_type*/);
/*
 * This function is called to get the data associated with an entity, for
 * example to paste from the clipboard. 'client_handle' will be the pointer
 * passed to entity_claim() and 'pref_file_types' will point to a list of
 * file types in order of preference. It should choose the earliest supported
 * type from this list or failing that use its native format. The chosen file
 * type must be written to the 'file_type' pointer. The function should return
 * a pointer to a flex anchor, or NULL if 'probe_only' was true or no data was
 * available. In the latter case, no error will be reported. If a flex pointer
 * was legitimately returned and false was written to 'data_persists' then the
 * block will be automatically freed when no longer required.
 */

typedef void EntityExitMethod (void);
/*
 * This function is called by entity_dispose_all() if no entities are owned
 * by our application, or alternatively when any data requests resulting from
 * a ReleaseEntity broadcast have either been satisfied or failed. Typically
 * it should exit the program and never return.
 */

/* --------------------------- Library functions ---------------------------- */

CONST _kernel_oserror *entity_initialise(
#ifdef CBLIB_OBSOLETE
                       void
#else
                       MessagesFD  */*mfd*/,
                       void       (*/*report_error*/)(CONST _kernel_oserror *)
#endif
);
   /*
    * Initialises the Entity component and registers WIMP message handlers
    * for DataRequest and ClaimEntity. These are used to negotiate ownership of
    * system-wide entities (such as the clipboard) on behalf of the client
    * program. A WIMP event handler for UserMessageAcknowledge is also
    * registered. The event library's mask is manipulated to allow UserMessage,
    * UserMessageRecorded and UserMessageAcknowledge events.
    * YOU MUST INITIALISE 'LOADER' BEFORE 'ENTITY' OR ELSE IT WILL INTERCEPT
    *                MESSAGES REQUIRED BY THIS COMPONENT!
    * Unless 'mfd' is a null pointer, the specified messages file will be given
    * priority over the global messages file when looking up text required by
    * this module. Unless 'report_error' is a null pointer, it should point to
    * a function to be called if an error occurs whilst handling an event.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

CONST _kernel_oserror *entity_claim(unsigned int /*flags*/,
                                    EntityLostMethod * /*lost_method*/,
                                    EntityDataMethod * /*data_method*/,
                                    void * /*client_handle*/);
   /*
    * Claims possession of the entities represented by bits set in the flags
    * word (as for a ClaimEntity message) and registers functions to be called
    * when those entities are lost, or data is requested for them. If necessary
    * a message is broadcast to inform other tasks that we are claiming the
    * specified entities.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

CONST _kernel_oserror *entity_probe_data(
                                unsigned int /*flags*/,
                                int /*window*/,
                                const int * /*file_types*/,
                                Loader2FinishedHandler * /*inform_entity_data*/,
                                void * /*client_handle*/);
   /*
    * This function probes to see whether there is any data associated with the
    * entities specified by the 'flags' value. The 'window' handle is required
    * in case the owner of the specified entities uses it instead of our task
    * handle, when replying to our DataRequest message(s). 'file_types' must
    * point to a list of acceptable file types, in order of preference and
    * terminated by FileType_Null. The Loader2FinishedHandler function
    * will be called to report whether data is available; 'client_handle' is
    * an opaque value that will be passed to it. If no data is available then
    * it will be called with FileType_Null.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

CONST _kernel_oserror *entity_request_data(
                               unsigned int /*flags*/,
                               int /*window*/,
                               int /*icon*/,
                               int /*x*/,
                               int /*y*/,
                               const int * /*file_types*/,
                               Loader2FinishedHandler * /*deliver_entity_data*/,
                               void * /*client_handle*/);
   /*
    * This function requests any data associated with the entities specified by
    * the 'flags' value. If an entity is owned by another task then the drop
    * location specified by 'window', 'icon', 'x' and 'y' will be cited in a
    * DataRequest broadcast. However they should not affect the outcome unless a
    * rogue message handler claims the DataSave message sent in reply, or the
    * other task doesn't reply properly. 'file_types' must point to a list of
    * acceptable file types, in order of preference and terminated by
    * FileType_Null.
    * The Loader2FinishedHandler function will be called to deliver the entity
    * data; 'client_handle' is an opaque value that will be passed to it. It
    * will also be called to report failure (indicated by FileType_Null).
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

CONST _kernel_oserror *entity_dispose_all(EntityExitMethod * exit_method);
   /*
    * If our task owns any entities then this function broadcasts a message to
    * inform other tasks that this is their last chance to request the
    * associated data. In this case, the function will return early without
    * having called 'exit_method' because finalisation is still pending.
    * If no entities are owned then the specified EntityExitMethod function (if
    * any) will be called directly.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

void entity_cancel_requests(Loader2FinishedHandler */*deliver_entity_data*/,
                            void */*client_handle*/);
   /*
    * Cancels any outstanding data requests (and probes) for the specified
    * client function and handle. Use when the destination has become invalid,
    * e.g. because a document is being closed.
    */

void entity_release(unsigned int /*flags*/);
   /*
    * Relinquishes possession of the entities represented by bits set in the
    * flags word (as for a ClaimEntity message). Any EntityLostMethod
    * functions registered when those entities were claimed will be called.
    */

CONST _kernel_oserror *entity_finalise(void);
   /*
    * Deregisters the Entity component's event handlers and releases any memory
    * claimed by this library component. It also releases any entities that
    * were claimed by the client program, and calls the relevant
    * EntityLostMethod functions. Note that this function is not normally
    * included in pre-built library distributions.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

#endif
