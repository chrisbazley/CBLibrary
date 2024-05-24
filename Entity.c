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

/* History:
  CJB: 08-Oct-06: Created this source file from scratch.
  CJB: 25-Oct-06: First public release version.
  CJB: 14-Apr-07: Modified entity_finalise() to soldier on if an error occurs,
                  using the new MERGE_ERR macro.
  CJB: 27-Jan-08: Fixed two memory leaks in the _ent_datarequest_msg_handler
                  function, for entities with non-persistent data (i.e. should
                  be freed when no longer required):
                  1) It neglected to free the flex pointer returned by the
                     EntityDataMethod function if it could not be re-anchored.
                  2) It neglected to free the heap block containing the new
                     anchor, if the saver_send_data function failed.
                  Also updated this function to allocate and partially populate
                  a WimpMessage message for saver_send_data, rather than
                  passing an inordinate number of arguments.
  CJB: 22-Jun-09: Amalgamated similar implementations of entity_probe_data() and
                  entity_request_data() in a new static function, which now
                  broadcasts a separate DataRequest message for each entity not
                  owned by the client task (previously broadcast only one
                  message with multiple flags set). Use variable name rather
                  than type with 'sizeof' operator, tweaked spacing and use
                  format specifier '%u' instead of '%d' for unsigned values.
                  Callback function _ent_data_sent() now makes an error message
                  from token "EntitySendFail" instead of generic "SaveFail".
                  New function _ent_no_data() makes an entity-specific error
                  message (e.g. "Clipboard is empty") for use when there is no
                  EntityDataMethod associated with an entity owned by the
                  client task, such a function returns no data, or a DataRequest
                  message returns undelivered.
  CJB: 08-Sep-09: Stop using reserved identifier '_RequestOpData' (starts with
                  an underscore followed by a capital letter). Amalgamated the
                  definition of struct type 'entity_info' with entities_info[].
                  Now iterates over an array of Wimp message numbers and
                  function pointers when registering or deregistering message
                  handlers (reduces code size and ensures symmetry).
  CJB: 14-Oct-09: Replaced 'magic' values with named constants and added some
                  new assertions. Modified _ent_msg_bounce_handler to have a
                  single point of return, and _ent_find_data_req to use a 'for'
                  loop. Removed dependencies on MsgTrans and Err modules by
                  storing pointers to a messages file descriptor and an error-
                  reporting callback upon initialisation. Simplified the
                  control flow in _ent_probe_or_request.
  CJB: 16-Oct-09: Missing argument to DEBUGF macro in _ent_probe_or_request
                  caused crash if debugging output enabled.
  CJB: 26-Jun-10: Fixed a bug where _ent_probe_or_request wrongly freed the
                  client's persistent entity data if not enough memory to
                  create a copy for the Loader2FinishedHandler function!
  CJB: 18-Feb-12: Additional assertions to detect message token formatting
                  errors and buffer overflow/truncation.
  CJB: 06-May-12: Made the arguments to entity_initialise conditional upon
                  CBLIB_OBSOLETE.
  CJB: 24-Aug-13: Stopped misusing sizeof(WimpReleaseEntityMessage) in place
                  of sizeof(WimpClaimEntityMessage), although equal.
  CJB: 17-Dec-14: Updated to use the generic linked list implementation.
  CJB: 23-Dec-14: Apply Fortify to Event & Wimp library function calls.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 06-Feb-16: Extra output to debug claim broadcasts not delivered back.
  CJB: 09-Apr-16: Modified format strings and changed some types to
                  'unsigned int' to avoid GNU C compiler warnings.
  CJB: 21-Apr-16: Cast pointer parameters to void * to match %p. No longer
                  prints function pointers (no matching format specifier).
                  Used size_t for loop counters to match type of ARRAY_SIZE.
  CJB: 01-Nov-18: Replaced DEBUG macro usage with DEBUGF.
  CJB: 25-Aug-20: Deleted a redundant static function pre-declaration.
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
#include "flex.h"

/* CBUtilLib headers */
#include "LinkedList.h"

/* CBOSLib headers */
#include "MessTrans.h"
#include "WimpExtra.h"

/* Local headers */
#include "Internal/CBMisc.h"
#include "Saver.h"
#include "Loader2.h"
#include "Entity.h"
#include "NoBudge.h"
#ifdef CBLIB_OBSOLETE
#include "MsgTrans.h"
#include "Err.h"
#endif /* CBLIB_OBSOLETE */

typedef struct
{
  Loader2FinishedHandler *funct; /* may be NULL */
  void                   *arg;
}
RequestOpCallback;

/* The following structure holds all the state for a data request */
typedef struct
{
  LinkedListItem    list_item;
  unsigned int      entity;
  int               data_request_ref;
  bool              probe;
  RequestOpCallback callback;
}
RequestOpData;

/* Constant numeric values */
enum
{
  MaxTokenLen   = 31, /* For Entity<n>NoData message token names. */
  NEntities     = 8,  /* Could be increased to 32 (one for each flag bit in a
                         ClaimEntity/ReleaseEntity/DataRequest message). */
  PreExpandHeap = 512 /* Number of bytes to pre-allocate before disabling
                         flex budging (and thus heap expansion). */
};

/* -----------------------------------------------------------------------
                        Internal function prototypes
*/

static WimpMessageHandler _ent_claimentity_msg_handler, _ent_datarequest_msg_handler, _ent_datasave_msg_handler;
static WimpEventHandler _ent_msg_bounce_handler;
static SaverFinishedHandler _ent_data_sent;
static Loader2FinishedHandler _ent_load_finished;
static RequestOpData *_ent_find_data_req(int msg_ref);
static CONST _kernel_oserror *_ent_no_data(unsigned int entity);
static CONST _kernel_oserror *_ent_probe_or_request(unsigned int flags, int window, int icon, int x, int y, const int *file_types, const RequestOpCallback *callback, bool probe);
static CONST _kernel_oserror *lookup_error(const char *token);
static bool check_error(CONST _kernel_oserror *e);
static LinkedListCallbackFn _ent_cancel_matching_request, _ent_request_has_ref;

/* -----------------------------------------------------------------------
                          Internal library data
*/

/* This array stores information about the claimant of each entity */
static struct
{
  EntityDataMethod *data_method; /* call this to get the associated data */
  EntityLostMethod *lost_method; /* call this when the claimant is usurped */
  void             *client_handle; /* this is passed to the above functions */
}
entities_info[NEntities];

static int claimentity_msg_ref[NEntities];

static unsigned int owned_entities; /* Entity owned by our task (flag bits) */
static bool initialised, finalisation_pending;
static EntityExitMethod *client_exit_method;
static int releaseentity_msg_ref;
static unsigned int data_sent_count, claimentity_count;
static const int empty_list[] =
{
  FileType_Null
};

static const struct
{
  int                 msg_no;
  WimpMessageHandler *handler;
}
msg_handlers[] =
{
  {
    Wimp_MClaimEntity,
    _ent_claimentity_msg_handler
  },
  {
    Wimp_MDataRequest,
    _ent_datarequest_msg_handler
  },
  {
    Wimp_MDataSave,
    _ent_datasave_msg_handler
  }
};

static LinkedList request_op_data_list;
static MessagesFD *desc;
#ifndef CBLIB_OBSOLETE
static void (*report)(CONST _kernel_oserror *);
#endif

/* -----------------------------------------------------------------------
                         Public library functions
*/

CONST _kernel_oserror *entity_initialise(
#ifdef CBLIB_OBSOLETE
                         void
#else
                         MessagesFD  *mfd,
                         void       (*report_error)(CONST _kernel_oserror *)
#endif
)
{
  assert(!initialised);

  /* Store pointers to messages file descriptor and error-reporting function */
#ifdef CBLIB_OBSOLETE
  desc = msgs_get_descriptor();
#else
  desc = mfd;
  report = report_error;
#endif

  /* We rely on a logical comformity between the flags usage in several
     distinct Wimp messages */
  assert(Wimp_MClaimEntity_Clipboard == Wimp_MDataRequest_Clipboard);
  assert(Wimp_MClaimEntity_Clipboard == Wimp_MReleaseEntity_Clipboard);

  /* Register Wimp message handlers */
  for (size_t i = 0; i < ARRAY_SIZE(msg_handlers); i++)
  {
    ON_ERR_RTN_E(event_register_message_handler(msg_handlers[i].msg_no,
                                                msg_handlers[i].handler,
                                                NULL));
  }

  /* Register handler for messages that return to us as wimp event 19 */
  ON_ERR_RTN_E(event_register_wimp_handler(-1,
                                           Wimp_EUserMessageAcknowledge,
                                           _ent_msg_bounce_handler,
                                           NULL));

  /* Zero initialisation (only required for re-init) */
  memset(claimentity_msg_ref, 0, sizeof(claimentity_msg_ref));

  memset(entities_info, 0, sizeof(entities_info));

  owned_entities = data_sent_count = releaseentity_msg_ref =
                   claimentity_count = 0;

  linkedlist_init(&request_op_data_list);

  initialised = true;

  return NULL; /* success */
}

/* ----------------------------------------------------------------------- */

CONST _kernel_oserror *entity_claim(unsigned int flags, EntityLostMethod * lost_method, EntityDataMethod * data_method, void *client_handle)
{
  DEBUGF("Entity: Request to claim flags %u (handle:%p)\n", flags, client_handle);

  assert(initialised);
  if (!flags)
    return NULL;

  /* Does our task already own the entities to be claimed? */
  if (flags & ~owned_entities)
  {
    /* Notify other tasks that we are claiming the specified entities */
    WimpMessage message;
    WimpClaimEntityMessage *cem = (WimpClaimEntityMessage *)&message.data;

    message.hdr.size = sizeof(message.hdr) + sizeof(WimpClaimEntityMessage);
    message.hdr.your_ref = 0;
    message.hdr.action_code = Wimp_MClaimEntity;
    cem->flags = flags & ~owned_entities;

    ON_ERR_RTN_E(wimp_send_message(Wimp_EUserMessage, &message, 0, 0, NULL));

    DEBUGF("Entity: Broadcast ClaimEntity message with flags %u (ref. %d)\n",
          cem->flags, message.hdr.my_ref);

    /* We record ClaimEntity message IDs in an array so that we do not forget
       earlier broadcasts */
    DEBUGF("Entity: allocating slot %u\n", claimentity_count);
    assert(!claimentity_msg_ref[claimentity_count]);
    claimentity_msg_ref[claimentity_count++] = message.hdr.my_ref;
    if (claimentity_count >= ARRAY_SIZE(claimentity_msg_ref))
      claimentity_count = 0; /* wrap around */

    SET_BITS(owned_entities, flags);
  }

  /* Store new handler functions for the specified entities */
  for (size_t entity = 0; entity < ARRAY_SIZE(entities_info); entity++)
  {
    if (!TEST_BITS(flags, 1u<<entity))
      continue; /* not replacing owner of this entity */

    /* Tell the previous claimant that it has been usurped */
    if (entities_info[entity].lost_method != NULL) {
      DEBUGF("Entity: Calling EntityLostMethod with handle %p for entity %zu\n",
            entities_info[entity].client_handle, entity);
      assert(TEST_BITS(owned_entities, 1u<<entity));
      entities_info[entity].lost_method(entities_info[entity].client_handle);
    }

    /* Record the new client handle and function pointers */
    entities_info[entity].lost_method = lost_method;
    entities_info[entity].data_method = data_method;
    entities_info[entity].client_handle = client_handle;
  }

  DEBUGF("Entity: Claim complete\n");
  return NULL; /* success */
}

/* ----------------------------------------------------------------------- */

CONST _kernel_oserror *entity_probe_data(unsigned int flags, int window, const int *file_types, Loader2FinishedHandler *inform_entity_data, void *client_handle)
{
  /* When merely probing for data, the destination icon handle and
     coordinates shouldn't be significant. */
  RequestOpCallback callback;

  callback.funct = inform_entity_data;
  callback.arg = client_handle;

  return _ent_probe_or_request(flags,
                               window,
                               -1,
                               0,
                               0,
                               file_types,
                               &callback,
                               true);
}

/* ----------------------------------------------------------------------- */

CONST _kernel_oserror *entity_request_data(unsigned int flags, int window, int icon, int x, int y, const int *file_types, Loader2FinishedHandler *deliver_entity_data, void *client_handle)
{
  RequestOpCallback callback;

  callback.funct = deliver_entity_data;
  callback.arg = client_handle;

  return _ent_probe_or_request(flags,
                               window,
                               icon,
                               x,
                               y,
                               file_types,
                               &callback,
                               false);
}

/* ----------------------------------------------------------------------- */

void entity_cancel_requests(Loader2FinishedHandler *deliver_entity_data, void *client_handle)
{
  /* Cancel any outstanding data requests for the specified client function
     and handle. Use when the destination has become invalid. */
  RequestOpCallback callback;

  DEBUGF("Entity: Cancelling all data requests with handle %p\n", client_handle);

  callback.funct = deliver_entity_data;
  callback.arg = client_handle;

  linkedlist_for_each(&request_op_data_list,
                      _ent_cancel_matching_request,
                      &callback );
}

/* ----------------------------------------------------------------------- */

void entity_release(unsigned int flags)
{
  DEBUGF("Entity: Request to release flags %u\n", flags);
  assert(initialised);

  /* If our task does not own the specified entities then we should
     not have any claimant records for them. */
  if (!TEST_BITS(owned_entities, flags)) {
    DEBUGF("Entity: Not owned by us\n");
    return; /* nothing to do */
  }

  /* Wipe the handler functions for the specified entities */
  for (size_t entity = 0; entity < ARRAY_SIZE(entities_info); entity++)
  {
    if (!TEST_BITS(flags, 1u<<entity) || !TEST_BITS(owned_entities, 1u<<entity))
      continue; /* we don't own this entity, or not released */

    /* Tell the owner of this entity that it has been usurped */
    if (entities_info[entity].lost_method != NULL) {
      DEBUGF("Entity: Calling release function with handle %p for entity %zu\n",
            entities_info[entity].client_handle, entity);
      entities_info[entity].lost_method(entities_info[entity].client_handle);
    } else {
      DEBUGF("Entity: No release function for entity %zu\n", entity);
    }

    /* Wipe the client handle and function pointers */
    entities_info[entity].lost_method = NULL;
    entities_info[entity].data_method = NULL;
    entities_info[entity].client_handle = NULL;

  } /* next entity */

  /* Note that our task still owns the released entities until another
     task claims them. */
}

/* ----------------------------------------------------------------------- */

#ifdef INCLUDE_FINALISATION_CODE
CONST _kernel_oserror *entity_finalise(void)
{
  CONST _kernel_oserror *return_error = NULL;

  assert(initialised);
  initialised = false;

  /* Release all entities that we own */
  entity_release(owned_entities);

  /* Deregister Wimp message handlers */
  for (size_t i = 0; i < ARRAY_SIZE(msg_handlers); i++)
  {
    MERGE_ERR(return_error,
              event_deregister_message_handler(msg_handlers[i].msg_no,
                                               msg_handlers[i].handler,
                                               NULL));
  }

  /* Deregister handler for messages that return to us as wimp event 19 */
  MERGE_ERR(return_error,
            event_deregister_wimp_handler(-1,
                                          Wimp_EUserMessageAcknowledge,
                                          _ent_msg_bounce_handler,
                                          NULL));

  return return_error;
}
#endif

/* ----------------------------------------------------------------------- */

CONST _kernel_oserror *entity_dispose_all(EntityExitMethod * exit_method)
{
  bool data_found;

  DEBUGF("Entity: Releasing all entities (%s post-function)\n",
        exit_method ? "with" : "without");
  assert(initialised);

  /* Search for entities owned by us, that may have data associated with them */
  data_found = false;
  if (owned_entities)
  {
    for (size_t entity = 0; entity < ARRAY_SIZE(entities_info); entity++)
    {
      if (!TEST_BITS(owned_entities, 1u<<entity) ||
          entities_info[entity].data_method == NULL)
        continue;

      DEBUGF("Entity: Found data function for entity %zu\n", entity);
      data_found = true;
      break;
    }
  }

  if (data_found)
  {
    /* Broadcast notification that the holder of some entities is dying */
    WimpMessage message;
    WimpReleaseEntityMessage *rem = (WimpReleaseEntityMessage *)&message.data;
    message.hdr.size = sizeof(message.hdr) + sizeof(WimpReleaseEntityMessage);
    message.hdr.your_ref = 0;
    message.hdr.action_code = Wimp_MReleaseEntity;
    rem->flags = owned_entities;
    ON_ERR_RTN_E(wimp_send_message(Wimp_EUserMessageRecorded, &message, 0, 0,
                                   NULL));
    /* A clipboard holder application will respond to this message by
       requesting the data held by us, before we die */
    DEBUGF("Entity: Broadcast ReleaseEntity message (ref. %d)\n",
          message.hdr.my_ref);
    assert(releaseentity_msg_ref == 0);
    releaseentity_msg_ref = message.hdr.my_ref; /* record our message's ID */
    client_exit_method = exit_method; /* call-back function (may be NULL) */
    finalisation_pending = true;
  }
  else
  {
    DEBUGF("Entity: We don't own any entities with associated data\n");
    if (exit_method != NULL)
      exit_method();
  }

  return NULL; /* success */
}

/* -----------------------------------------------------------------------
                        Wimp message handlers
*/

static int _ent_claimentity_msg_handler(WimpMessage *message, void *handle)
{
  /* This is a handler for ClaimEntity messages */
  const WimpClaimEntityMessage *claim_entity;
  unsigned int not_owned = ~owned_entities;

  assert(message != NULL);
  NOT_USED(handle);

  claim_entity = (WimpClaimEntityMessage *)&message->data;

  DEBUGF("Entity: Received a ClaimEntity message (ref. %d in reply to %d)\n",
        message->hdr.my_ref, message->hdr.your_ref);

  /* Check our record of the ClaimEntity broadcasts we did recently */
  for (size_t index = 0; index < ARRAY_SIZE(claimentity_msg_ref); index++)
  {
    if (message->hdr.my_ref == claimentity_msg_ref[index]) {
      DEBUGF("Entity: Ignoring our own claim (freeing slot %zu)\n", index);
      claimentity_msg_ref[index] = 0;
      return 1; /* it is our own broadcast - ignore and claim message */
    }
  }
  DEBUGF("Entity: Not one of our broadcasts\n");

  if (TEST_BITS(owned_entities, claim_entity->flags))
  {
    /* Another task has usurped our ownership of some entities */
    entity_release(claim_entity->flags);
    CLEAR_BITS(owned_entities, claim_entity->flags);
  }

  /* Claim message unless entities are being claimed that we didn't own */
  return !TEST_BITS(not_owned, claim_entity->flags);
}

/* ----------------------------------------------------------------------- */

static int _ent_datasave_msg_handler(WimpMessage *message, void *handle)
{
  /* This handler must receive DataSave messages before the Loader
     component. We need to intercept replies to our DataRequest message. */
  CONST _kernel_oserror *e;
  RequestOpData *request_op_data;
  NOT_USED(handle);

  DEBUGF("Entity: Received a DataSave message (ref. %d in reply to %d)\n",
        message->hdr.my_ref, message->hdr.your_ref);

  if (message->hdr.your_ref == 0 ||
      (request_op_data = _ent_find_data_req(message->hdr.your_ref)) == NULL)
  {
    DEBUGF("Entity: Unknown your_ref value\n");
    return 0; /* not a reply to our message */
  }
  DEBUGF("Entity: It's a reply to our DataRequest\n");

  if (request_op_data->probe)
  {
    /* We were just probing for data */
    DEBUGF("Entity: We are just probing for data\n");
    _ent_load_finished(NULL,
                       message->data.data_save.file_type,
                       NULL,
                       request_op_data);
  }
  else
  {
    /* Attempt to load the data associated with this entity */
    DEBUGF("Entity: Will load data associated with entity\n");
    request_op_data->data_request_ref = 0; /* prevent future matches */
    e = loader2_receive_data(message,
                             NULL,
                             _ent_load_finished,
                             request_op_data);
    if (e != NULL)
      _ent_load_finished(e, FileType_Null, NULL, request_op_data);
  }

  return 1; /* claim message to prevent transfer of entity data */
}

/* ----------------------------------------------------------------------- */

static int _ent_datarequest_msg_handler(WimpMessage *message, void *handle)
{
  /* This is a handler for DataRequest messages */
  const WimpDataRequestMessage *data_request;

  assert(message != NULL);
  NOT_USED(handle);

  data_request = (WimpDataRequestMessage *)&message->data;

  DEBUGF("Entity: Received a DataRequest message with flags %d "
        "(ref. %d in reply to %d)\n", data_request->flags, message->hdr.my_ref,
        message->hdr.your_ref);

  if (message->hdr.your_ref != 0 &&
      message->hdr.your_ref == releaseentity_msg_ref)
  {
    DEBUGF("Entity: It's a reply to our ReleaseEntity message\n");
    releaseentity_msg_ref = 0; /* reset message ID */
  }

  if (!TEST_BITS(owned_entities, data_request->flags))
  {
    DEBUGF("Entity: Not owned by us\n");
    return 0; /* we don't own any of the specified entities */
  }

  for (size_t entity = 0; entity < ARRAY_SIZE(entities_info); entity++)
  {
    EntityDataMethod *get_data_func;

    if (!TEST_BITS(data_request->flags, 1u<<entity) ||
        !TEST_BITS(owned_entities, 1u<<entity))
      continue; /* we don't own this entity, or data not requested */

    /* Request data from the owner of this entity */
    get_data_func = entities_info[entity].data_method;
    if (get_data_func != NULL)
    {
      bool data_persists;
      flex_ptr entity_data, new_anchor;
      int data_type;
      WimpMessage msg;
      CONST _kernel_oserror *e;

      DEBUGF("Entity: Calling data function with %p for entity %zu\n",
            entities_info[entity].client_handle, entity);

      data_persists = true; /* default for safety */
      entity_data = get_data_func(data_request->file_types,
                                  false,
                                  entities_info[entity].client_handle,
                                  &data_persists,
                                  &data_type);
      if (entity_data == NULL)
      {
        DEBUGF("Entity: Client supplied no entity data\n");
        continue; /* no data for this entity */
      }
      DEBUGF("Entity: Entity data is anchored at %p (file type &%x)\n",
            (void *)entity_data, data_type);

      /* We re-anchor the flex block holding the entity data unless it is
         persistent. This prevents a race condition if the same EntityDataMethod
         is called again before we reach _ent_data_sent(). */
      if (!data_persists)
      {
        /* Allocate one word of heap memory as a new anchor for the flex block
           containing the data associated with this entity. */
        new_anchor = malloc(sizeof(*new_anchor));
        if (new_anchor == NULL)
        {
          DEBUGF("Entity: Not enough memory to create new anchor!\n");

          /* Report the error, if an error-reporting function is registered */
          check_error(lookup_error("NoMem"));

          flex_free(entity_data); /* free the data associated with the entity */
          continue; /* could not allocate memory for new anchor */
        }

        if (!flex_reanchor(new_anchor, entity_data))
        {
          DEBUGF("Entity: Failed to reanchor data associated with entity %zu!\n",
                entity);
          flex_free(entity_data); /* free the data associated with the entity */
          free(new_anchor); /* free the intended new anchor */
          continue; /* could not re-anchor the data */
        }

        DEBUGF("Entity: Data is temporary - re-anchored at %p\n", (void *)new_anchor);
      }
      else
      {
        DEBUGF("Entity: Data is persistent - may not re-anchor\n");
        new_anchor = entity_data;
      }

      msg.hdr.your_ref = message->hdr.my_ref;
      /* action code and message size are filled out automatically */
      msg.data.data_save.destination_window =
                                             data_request->destination_window;
      msg.data.data_save.destination_icon = data_request->destination_icon;
      msg.data.data_save.destination_x = data_request->destination_x;
      msg.data.data_save.destination_y = data_request->destination_y;
      /* estimated size is filled out automatically */
      /* According to the RISC OS 3 PRM a file type value of &ffffffff in a
         DataSave message means the file is untyped (&3000 is more usual). */
      msg.data.data_save.file_type = (data_type == FileType_None ?
                                     FileType_Null : data_type);
      STRCPY_SAFE(msg.data.data_save.leaf_name, "EntityData");

      e = saver_send_data(message->hdr.sender,
                          &msg,
                          new_anchor,
                          0,
                          flex_size(new_anchor),
                          NULL,
                          _ent_data_sent,
                          data_persists ? NULL : new_anchor);
      if (check_error(e))
      {
        /* We cannot rely on _ent_data_sent() to free entity data if an error
           occurred */
        if (!data_persists)
        {
          flex_free(new_anchor); /* free the flex block reanchored here */
          free(new_anchor); /* free the anchor itself (in a heap block) */
        }
        continue;
      }
      data_sent_count++;
      DEBUGF("Entity: Have started data transfer %u\n", data_sent_count);
    }
    else
    {
      DEBUGF("Entity: No data function for entity %zu\n", entity);
    }
  } /* next entity */

  /* Claim message unless data requested for any entities that we don't own */
  return !TEST_BITS(~owned_entities, data_request->flags);
}

/* -----------------------------------------------------------------------
                        Wimp event handlers
*/

static int _ent_msg_bounce_handler(int event_code, WimpPollBlock *event, IdBlock *id_block, void *handle)
{
  /* This is a handler for bounced messages */
  RequestOpData *request_op_data;
  int claim = 0;

  NOT_USED(event_code);
  assert(event != NULL);
  NOT_USED(id_block);
  NOT_USED(handle);

  DEBUGF("Entity: Received a bounced message (ref. %d)\n",
        event->user_message_acknowledge.hdr.my_ref);

  switch (event->user_message_acknowledge.hdr.action_code)
  {
    case Wimp_MReleaseEntity:
      if (event->user_message_acknowledge.hdr.my_ref != releaseentity_msg_ref)
      {
        DEBUGF("Entity: Not our ReleaseEntity message (ref. %d)\n",
              releaseentity_msg_ref);
        break;
      }
      releaseentity_msg_ref = 0; /* reset message ID */

      /* No other task requested the data associated with our entities, so it
         will be lost when we die. */
      if (finalisation_pending)
      {
        finalisation_pending = false;
        if (client_exit_method != NULL)
          client_exit_method();
      }
      claim = 1; /* claim event */
      break;

    case Wimp_MDataRequest:
      request_op_data = _ent_find_data_req(
                                    event->user_message_acknowledge.hdr.my_ref);
      if (request_op_data == NULL)
      {
        DEBUGF("Entity: Unknown DataRequest message\n");
        break;
      }
      DEBUGF("Entity: It's our DataRequest message\n");
      _ent_load_finished(_ent_no_data(request_op_data->entity),
                         FileType_Null,
                         NULL,
                         request_op_data);
      claim = 1; /* claim event */
      break;

    default:
      break; /* unknown message code */
  }
  return claim;
}

/* -----------------------------------------------------------------------
                         Miscellaneous internal functions
*/

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

static CONST _kernel_oserror *_ent_no_data(unsigned int entity)
{
  char token[MaxTokenLen + 1];
  int nout;

  nout = sprintf(token,
          "Entity%uNoData",
          entity);
  assert(nout >= 0); /* no formatting error */
  assert((size_t)nout < sizeof(token)); /* no buffer overflow/truncation */
  NOT_USED(nout);

  return lookup_error(token);
}

/* ----------------------------------------------------------------------- */

static CONST _kernel_oserror *lookup_error(const char *token)
{
  /* Look up error message from the token, outputting to an internal buffer */
  return messagetrans_error_lookup(desc, DUMMY_ERRNO, token, 0);
}

/* ----------------------------------------------------------------------- */

static void _ent_data_sent(bool success, CONST _kernel_oserror *err, const char *file_path, int datasave_ref, void *client_handle)
{
  /* This function is called after an attempt to send the data associated with
     an entity to another task. */
  NOT_USED(datasave_ref);
  NOT_USED(file_path);
  NOT_USED(success);

  /* Report any error, if an error-reporting function is registered */
  if (err != NULL)
  {
    /* Look up error message from the token, outputting to an internal buffer */
    check_error(messagetrans_error_lookup(desc,
                                          err->errnum,
                                          "EntitySendFail",
                                          1,
                                          err->errmess));
  }

  /* If the data returned by the client's EntityDataMethod is not persistent
     then 'client handle' is a pointer to the flex anchor */
  flex_ptr new_anchor = (flex_ptr)client_handle; /* a neat trick */
  if (new_anchor != NULL)
  {
    DEBUGF("Entity: Freeing entity data anchored at %p\n", (void *)new_anchor);
    flex_free(new_anchor); /* free the flex block pointed to by the anchor */
    free(new_anchor); /* free the anchor itself (in a heap block) */
  }

  data_sent_count--;
  DEBUGF("Entity: %u data transfers outstanding\n", data_sent_count);
  if (data_sent_count == 0 && finalisation_pending)
  {
    finalisation_pending = false;
    DEBUGF("Entity: Calling exit function\n");
    if (client_exit_method != NULL)
      client_exit_method();
  }
}

/* ----------------------------------------------------------------------- */

static CONST _kernel_oserror *_ent_request_data(int window, int icon, int x, int y, unsigned int flags, const int *file_types, int *my_ref)
{
  WimpMessage message;
  WimpDataRequestMessage *drm = (WimpDataRequestMessage *)&message.data;
  CONST _kernel_oserror *e = NULL;

  message.hdr.your_ref = 0;
  message.hdr.action_code = Wimp_MDataRequest;

  drm->destination_window = window;
  drm->destination_icon = icon;
  drm->destination_x = x;
  drm->destination_y = y;
  drm->flags = flags;

  /* Copy list of file types into message body */
  assert(file_types != NULL);
  size_t array_len = 0;
  do
  {
    assert(array_len < 16); /* not certain but suggests a bug */
    /* Forcibly terminate list if too long */
    if (array_len >= ARRAY_SIZE(drm->file_types) - 1)
    {
      DEBUGF("Entity: Forcing termination of file types list!\n");
      drm->file_types[ARRAY_SIZE(drm->file_types) - 1] = FileType_Null;
      break;
    }
    drm->file_types[array_len] = file_types[array_len];
    DEBUGF("Entity: File type %zu is &%x\n", array_len,
          file_types[array_len]);
  }
  while (file_types[array_len++] != FileType_Null);
  DEBUGF("Entity: File types array length is %zu\n", array_len);

  message.hdr.size = WORD_ALIGN(sizeof(message.hdr) +
                     offsetof(WimpDataRequestMessage, file_types) +
                     sizeof(drm->file_types[0]) * array_len);

  e = wimp_send_message(Wimp_EUserMessageRecorded, &message, 0, 0, NULL);
  if (e == NULL)
  {
    DEBUGF("Broadcast DataRequest message (ref. %d)\n", message.hdr.my_ref);
    if (my_ref != NULL)
      *my_ref = message.hdr.my_ref; /* record ID of outgoing message */
  }
  return e;
}

/* ----------------------------------------------------------------------- */

static void _ent_load_finished(CONST _kernel_oserror *load_error, int file_type, flex_ptr buffer, void *client_handle)
{
  /* This function is called when a load operation has finished
     (whether successful or not) */
  RequestOpData *request_op_data = (RequestOpData *)client_handle;

  assert(file_type == FileType_Null || load_error == NULL);

  DEBUGF("Entity: Request %p finished %ssuccessfully\n", client_handle,
        file_type == FileType_Null ? "un" : "");

  if (buffer != NULL)
    DEBUGF("Entity: Current address of flex block is %p\n", *buffer);

  if (request_op_data->callback.funct != NULL)
  {
    /* Call the client-supplied function to notify it that the load
       operation is complete. */
    DEBUGF("Entity: Calling client function with handle %p\n",
          request_op_data->callback.arg);

    request_op_data->callback.funct(load_error, file_type, buffer, request_op_data->callback.arg);
  }
  else
  {
    DEBUGF("Entity: No client function\n");
    if (buffer != NULL)
      flex_free(buffer);
  }

  /* Free data block for this request and de-link it from the list*/
  DEBUGF("Entity: Removing record of request %p\n", client_handle);

  linkedlist_remove(&request_op_data_list, &request_op_data->list_item);

  free(request_op_data);
}

/* ----------------------------------------------------------------------- */

static RequestOpData *_ent_find_data_req(int msg_ref)
{
  RequestOpData *request_op_data;

  DEBUGF("Entity: Searching for data request awaiting reply to %d\n", msg_ref);
  request_op_data = (RequestOpData *)linkedlist_for_each(
                    &request_op_data_list, _ent_request_has_ref, &msg_ref);
  if (request_op_data == NULL)
  {
    DEBUGF("Entity: End of linked list (no match)\n");
  }
  else
  {
    DEBUGF("Entity: Record %p has matching message ID\n", (void *)request_op_data);
  }

  return request_op_data;
}

/* ----------------------------------------------------------------------- */

static CONST _kernel_oserror *_ent_probe_or_request(
                                    unsigned int             flags,
                                    int                      window,
                                    int                      icon,
                                    int                      x,
                                    int                      y,
                                    const int               *file_types,
                                    const RequestOpCallback *callback,
                                    bool                     probe)
{
  DEBUGF("Entity: Data %s for flags %u to coords %d,%d in window %d and "
         "icon %d\n", probe ? "probe": "request", flags, x, y, window, icon);
  assert(initialised);
  assert(callback);

  /* Treat a null file types pointer as an empty list */
  if (file_types == NULL)
    file_types = empty_list;

  for (size_t entity = 0; entity < ARRAY_SIZE(entities_info); entity++)
  {
    if (!TEST_BITS(flags, 1u<<entity))
      continue; /* data not requested for this entity */

    if (TEST_BITS(owned_entities, 1u<<entity))
    {
      /* We own this entity, so we can bypass the Wimp message protocol */
      int data_type = FileType_Null;
      flex_ptr data = NULL;
      CONST _kernel_oserror *e = NULL;
      void *copy_of_data;

      if (entities_info[entity].data_method != NULL)
      {
        bool data_persists;

        DEBUGF("Entity: Calling data function with handle %p for entity %zu\n",
               entities_info[entity].client_handle, entity);

        data_persists = true; /* default for safety */
        data = entities_info[entity].data_method(
                                            file_types,
                                            probe,
                                            entities_info[entity].client_handle,
                                            &data_persists,
                                            &data_type);

        DEBUGF("Entity: function returned %s data %p of type 0x%x\n",
               data_persists ? "persistent" : "temporary", (void *)data, data_type);

        if (probe)
        {
          /* Only a probe to find the type of data associated with the entity */
          assert(data == NULL);
        }
        else if (data == NULL)
        {
          /* There is no data associated with the entity (or not available) */
          e = _ent_no_data(entity);
        }
        else if (data_persists)
        {
          /* The entity data is persistent so we must create a copy for the
             Loader2FinishedHandler function to take ownership of it */
          if (flex_alloc(&copy_of_data, flex_size(data)))
          {
            nobudge_register(PreExpandHeap); /* protect dereference of
                                                flex pointer */
            memcpy((char *)copy_of_data, *data, flex_size(data));
            nobudge_deregister();

            data = &copy_of_data;
          }
          else
          {
            e = lookup_error("NoMem");
            data = NULL;
          }
        }
      }
      else
      {
        DEBUGF("Entity: No data function for entity %zu\n", entity);
        e = _ent_no_data(entity);
      }

      if (callback->funct != NULL)
      {
        DEBUGF("Entity: Calling function with handle %p to deliver ",
               callback->arg);
        if (e == NULL)
        {
          DEBUGF("data %p of type 0x%x\n", (void *)data, data_type);
        }
        else
        {
          DEBUGF("error 0x%x '%s'\n", e->errnum, e->errmess);
        }
        assert(e == NULL || data == NULL);
        callback->funct(e,
                        e == NULL ? data_type : FileType_Null,
                        data,
                        callback->arg);
      }
      else
      {
        DEBUGF("Entity: No callback function to deliver data\n");
        if (data != NULL)
          flex_free(data);
      }
    }
    else
    {
      /* We don't own this entity, so we must request the associated data from
         its owner. */
      CONST _kernel_oserror *e;
      RequestOpData *request_op_data;

      DEBUGF("Entity: Creating a record for a data %s\n",
            probe ? "probe" : "import");

      request_op_data = malloc(sizeof(*request_op_data));
      if (request_op_data == NULL)
        return lookup_error("NoMem");

      /* Initialise record for a new save operation */
      request_op_data->entity = entity;
      request_op_data->probe = probe;
      request_op_data->callback = *callback;

      /* Add new record to head of linked list */
      linkedlist_insert(&request_op_data_list, NULL, &request_op_data->list_item);
      DEBUGF("Entity: New record is at %p\n", (void *)request_op_data);

      e = _ent_request_data(window,
                            icon,
                            x,
                            y,
                            1u<<entity,
                            file_types,
                            &request_op_data->data_request_ref);
      if (e != NULL)
      {
        /* Remove new record again (failed) */
        linkedlist_remove(&request_op_data_list, &request_op_data->list_item);
        free(request_op_data);
        return e; /* failure */
      }
    }
  } /* next entity */

  return NULL; /* success */
}

/* ----------------------------------------------------------------------- */

static bool _ent_cancel_matching_request(LinkedList *list, LinkedListItem *item, void *arg)
{
  RequestOpData * const request_op_data = (RequestOpData *)item;
  const RequestOpCallback * const callback = arg;

  assert(request_op_data != NULL);
  assert(callback != NULL);
  NOT_USED(list);

  /* Check whether this data request is for delivery to the specified
     function with the specified handle */
  if (request_op_data->callback.funct == callback->funct &&
      request_op_data->callback.arg == callback->arg) {
    DEBUGF("Entity: Matched with data request %p\n", (void *)request_op_data);

    /* Are we still waiting for a DataSave message in reply to
       our DataRequest? */
    if (!request_op_data->data_request_ref) {
      /* No - cancel the load operation (thus cancelling the data request) */
      loader2_cancel_receives(_ent_load_finished, request_op_data);
    } else {
      /* Yes - cancel the data request directly */
      _ent_load_finished(NULL, FileType_Null, NULL, request_op_data);
    }
  }
  return false; /* next item */
}

/* ----------------------------------------------------------------------- */

static bool _ent_request_has_ref(LinkedList *list, LinkedListItem *item, void *arg)
{
  const int *msg_ref = arg;
  const RequestOpData * const request_op_data = (RequestOpData *)item;

  assert(msg_ref != NULL);
  assert(request_op_data != NULL);
  NOT_USED(list);

  return (request_op_data->data_request_ref == *msg_ref);
}
