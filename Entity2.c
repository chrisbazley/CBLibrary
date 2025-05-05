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

/* History:
  CJB: 06-Oct-19: Created this source file from Entity.
  CJB: 10-Nov-19: Pass the leaf name instead of "<Wimp$Scrap>" when calling
                  the Entity2ReadMethod. Pass the estimated file size as an
                  extra argument.
  CJB: 01-Nov-20: Assign a compound literal when claiming an entity.
  CJB: 02-Oct-21: Release entities upon exit triggered by entity2_dispose_all
                  to avoid leaks if the client doesn't call entity2_finalise.
                  Assign a compound literal when releasing an entity.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
*/

/* ISO library headers */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "wimp.h"
#include "event.h"
#include "wimplib.h"
#include "toolbox.h"
#include "flex.h"

/* StreamLib headers */
#include "ReaderFlex.h"
#include "WriterFlex.h"

/* CBUtilLib headers */
#include "LinkedList.h"

/* CBOSLib headers */
#include "MessTrans.h"
#include "WimpExtra.h"
#include "FileTypes.h"

/* Local headers */
#include "Saver2.h"
#include "Loader3.h"
#include "Entity2.h"
#include "WriterNull.h"
#include "NoBudge.h"
#include "Internal/CBMisc.h"

/* The following structure holds all the state for a data request */
typedef struct
{
  LinkedListItem       list_item;
  size_t               entity;
  int                  data_request_ref;
  void                *client_handle;
  _Optional Entity2ProbeMethod  *probe_method;
  _Optional Entity2ReadMethod   *read_method;
  _Optional Entity2FailedMethod *failed_method;
}
RequestOpData;

typedef struct
{
#ifdef COPY_ARRAY_ARGS
  /* The following pointer references a heap block
     if COPY_ARRAY_ARGS is defined */
  _Optional int *file_types;
#else
  _Optional const int *file_types;
#endif
  _Optional Saver2WriteMethod     *write_method; /* call this to get the associated data */
  _Optional Entity2EstimateMethod *estimate_method; /* call this to get the file type */
  _Optional Entity2LostMethod     *lost_method; /* call this when the claimant is usurped */
  void *client_handle; /* this is passed to the above functions */
}
Entity2Info;

/* Constant numeric values */
enum
{
  DefaultBufferSize = BUFSIZ,
  MaxTokenLen   = 31, /* For Entity<n>NoData message token names. */
  NEntities     = 8,  /* Could be increased to 32 (one for each flag bit in a
                         ClaimEntity/ReleaseEntity/DataRequest message). */
  PreExpandHeap = BUFSIZ /* Number of bytes to pre-allocate before disabling
                            flex budging (and thus heap expansion). */
};

/* -----------------------------------------------------------------------
                          Internal library data
*/

/* This array stores information about the claimant of each entity */
static Entity2Info entities_info[NEntities];

static int claimentity_msg_ref[NEntities];

static unsigned int owned_entities; /* Entity owned by our task (flag bits) */
static bool initialised, finalisation_pending;
static Entity2ExitMethod *client_exit_method;
static int releaseentity_msg_ref;
static unsigned int data_sent_count, claimentity_count;

static LinkedList request_op_data_list;
static _Optional MessagesFD *desc;
static void (*report_fn)(CONST _kernel_oserror *);

/* -----------------------------------------------------------------------
                         Miscellaneous internal functions
*/

static void destroy_op(RequestOpData *const request_op_data)
{
  DEBUGF("Entity2: Removing record of request %p\n", (void *)request_op_data);
  linkedlist_remove(&request_op_data_list, &request_op_data->list_item);
  free(request_op_data);
}

/* ----------------------------------------------------------------------- */

static void report_error(CONST _kernel_oserror *const e)
{
  assert(e != NULL);
  if (report_fn)
  {
    report_fn(e);
  }
}

/* ----------------------------------------------------------------------- */

static CONST _kernel_oserror *lookup_error(const char *const token, const char *param)
{
  /* Look up error message from the token, outputting to an internal buffer */
  return messagetrans_error_lookup(desc, DUMMY_ERRNO, token, 1, param);
}

/* ----------------------------------------------------------------------- */

static CONST _kernel_oserror *no_data(unsigned int const entity)
{
  char token[MaxTokenLen + 1];
  int const nout = sprintf(token, "Entity%uNoData", entity);
  assert(nout >= 0); /* no formatting error */
  assert((size_t)nout < sizeof(token)); /* no buffer overflow/truncation */
  NOT_USED(nout);
  return lookup_error(token, "");
}

/* ----------------------------------------------------------------------- */

static CONST _kernel_oserror *no_mem(void)
{
  return lookup_error("NoMem", "");
}

/* ----------------------------------------------------------------------- */

static void exit_if_pending(void)
{
  if (finalisation_pending)
  {
    finalisation_pending = false;

    entity2_release(owned_entities);

    if (client_exit_method)
    {
      DEBUGF("Entity2: Calling exit function\n");
      client_exit_method();
    }
  }
}

/* ----------------------------------------------------------------------- */

static void send_done(void)
{
  assert(data_sent_count > 0);
  data_sent_count--;
  DEBUGF("Entity2: %u data transfers outstanding\n", data_sent_count);
  if (data_sent_count == 0)
  {
    exit_if_pending();
  }
}

/* ----------------------------------------------------------------------- */

static void send_failed(_Optional CONST _kernel_oserror *const e, void *const client_handle)
{
  /* Data associated with an entity could not be sent to another task. */
  NOT_USED(client_handle);

  /* Report any error, if an error-reporting function is registered */
  if (e != NULL)
  {
    /* Look up error message from the token, outputting to an internal buffer */
    report_error(messagetrans_error_lookup(desc, e->errnum, "EntitySendFail",
      1, e->errmess));
  }
  send_done();
}

/* ----------------------------------------------------------------------- */

static void send_complete(int const file_type, _Optional const char *const file_path,
  int const datasave_ref, void *const client_handle)
{
  /* Data associated with an entity was sent to another task. */
  NOT_USED(datasave_ref);
  NOT_USED(file_path);
  NOT_USED(file_type);
  NOT_USED(client_handle);
  send_done();
}

/* ----------------------------------------------------------------------- */

static _Optional CONST _kernel_oserror *request_data(
  RequestOpData *const request_op_data,
  const WimpDataRequestMessage *const data_request)
{
  assert(data_request != NULL);

  WimpMessage message;
  message.hdr.action_code = Wimp_MDataRequest;
  message.hdr.your_ref = 0;

  WimpDataRequestMessage *const drm = (WimpDataRequestMessage *)&message.data;
  drm->destination_window = data_request->destination_window;
  drm->destination_icon = data_request->destination_icon;
  drm->destination_x = data_request->destination_x;
  drm->destination_y = data_request->destination_y;
  drm->flags = data_request->flags;

  /* Copy list of file types into message body (-1 for terminator) */
  size_t const array_len = copy_file_types(drm->file_types,
    data_request->file_types, ARRAY_SIZE(drm->file_types) - 1) + 1;

  message.hdr.size = WORD_ALIGN(sizeof(message.hdr) +
                       offsetof(WimpDataRequestMessage, file_types) +
                       sizeof(drm->file_types[0]) * array_len);

  _Optional CONST _kernel_oserror *const e =
    wimp_send_message(Wimp_EUserMessageRecorded, &message, 0, 0, NULL);

  if (e == NULL)
  {
    DEBUGF("Broadcast DataRequest message (ref. %d)\n", message.hdr.my_ref);
    request_op_data->data_request_ref = message.hdr.my_ref;
  }
  return e;
}


/* ----------------------------------------------------------------------- */

static _Optional CONST _kernel_oserror *claim_entities(unsigned int const flags)
{
  /* Does our task already own the entities to be claimed? */
  unsigned int const to_claim = flags & ~owned_entities;
  if (!to_claim)
  {
    return NULL;
  }

  /* Notify other tasks that we are claiming the specified entities */
  WimpMessage message;
  WimpClaimEntityMessage *const cem = (WimpClaimEntityMessage *)&message.data;

  message.hdr.size = sizeof(message.hdr) + sizeof(WimpClaimEntityMessage);
  message.hdr.your_ref = 0;
  message.hdr.action_code = Wimp_MClaimEntity;
  cem->flags = to_claim;

  ON_ERR_RTN_E(wimp_send_message(Wimp_EUserMessage, &message, 0, 0, NULL));

  DEBUGF("Entity2: Broadcast ClaimEntity message with flags %u (ref. %d)\n",
        cem->flags, message.hdr.my_ref);

  /* We record ClaimEntity message IDs in an array so that we do not forget
     earlier broadcasts */
  DEBUGF("Entity2: allocating slot %u\n", claimentity_count);
  assert(!claimentity_msg_ref[claimentity_count]);
  claimentity_msg_ref[claimentity_count++] = message.hdr.my_ref;
  if (claimentity_count >= ARRAY_SIZE(claimentity_msg_ref))
  {
    claimentity_count = 0; /* wrap around */
  }

  SET_BITS(owned_entities, to_claim);
  return NULL;
}

/* ----------------------------------------------------------------------- */

static bool load_data(Reader *const reader, int const estimated_size,
  int const file_type, const char *const filename, void *const client_handle)
{
  RequestOpData *const request_op_data = client_handle;
  assert(request_op_data != NULL);
  DEBUGF("Entity2: Request %p needs data\n", client_handle);

  bool success = false;
  if (request_op_data->read_method)
  {
    DEBUGF("Entity2: Calling read function with arg %p\n",
           request_op_data->client_handle);

    success = request_op_data->read_method(reader, estimated_size,
      file_type, filename, request_op_data->client_handle);
  }
  destroy_op(request_op_data);
  return success;
}

/* ----------------------------------------------------------------------- */

static void probe_finished(int const file_type, void *const client_handle)
{
  RequestOpData *const request_op_data = client_handle;
  assert(request_op_data != NULL);
  DEBUGF("Entity2: Probe %p finished\n", client_handle);

  if (request_op_data->probe_method)
  {
    DEBUGF("Entity2: Calling probe function with arg %p\n",
           request_op_data->client_handle);

    request_op_data->probe_method(file_type, request_op_data->client_handle);
  }
  destroy_op(request_op_data);
}

/* ----------------------------------------------------------------------- */

static void report_fail(_Optional CONST _kernel_oserror *const e, void *const client_handle)
{
  RequestOpData *const request_op_data = client_handle;
  assert(request_op_data != NULL);
  DEBUGF("Entity2: Request %p failed\n", client_handle);

  if (request_op_data->failed_method)
  {
    DEBUGF("Entity2: Calling failed function with arg %p\n",
           request_op_data->client_handle);

    request_op_data->failed_method(e, request_op_data->client_handle);
  }
  destroy_op(request_op_data);
}

/* ----------------------------------------------------------------------- */

static bool request_has_ref(LinkedList *const list, LinkedListItem *const item,
  void *const arg)
{
  const int *msg_ref = arg;
  const RequestOpData * const request_op_data = (RequestOpData *)item;

  assert(msg_ref != NULL);
  assert(request_op_data != NULL);
  NOT_USED(list);

  return request_op_data->data_request_ref == *msg_ref;
}

/* ----------------------------------------------------------------------- */

static _Optional RequestOpData *find_data_req(int msg_ref)
{
  DEBUGF("Entity2: Searching for data request awaiting reply to %d\n", msg_ref);
  if (!msg_ref)
    return NULL;

  _Optional RequestOpData *const request_op_data = (RequestOpData *)linkedlist_for_each(
                    &request_op_data_list, request_has_ref, &msg_ref);
  if (request_op_data == NULL)
  {
    DEBUGF("Entity2: End of linked list (no match)\n");
  }
  else
  {
    DEBUGF("Entity2: Record %p has matching message ID\n", (void *)request_op_data);
  }

  return request_op_data;
}

/* ----------------------------------------------------------------------- */

static int get_estimated_size(size_t const entity, int const file_type)
{
  int estimated_size = DefaultBufferSize;

  assert(entity < ARRAY_SIZE(entities_info));
  if (entities_info[entity].estimate_method)
  {
    estimated_size = entities_info[entity].estimate_method(file_type,
      entities_info[entity].client_handle);

    if (estimated_size <= 0)
    {
      estimated_size = DefaultBufferSize;
    }
  }

  DEBUGF("Entity2: Estimated size %d for entity %zu\n", estimated_size, entity);
  return estimated_size;
}

/* ----------------------------------------------------------------------- */

static _Optional CONST _kernel_oserror *request_own(size_t const entity,
  const WimpDataRequestMessage *const data_request,
  _Optional Entity2ReadMethod *const read_method,
  _Optional Entity2FailedMethod *const failed_method, void *const client_handle)
{
  DEBUGF("Entity2: bypassing message protocol for entity %zu\n", entity);
  assert(entity < ARRAY_SIZE(entities_info));
  assert(data_request != NULL);

  if (!entities_info[entity].write_method || !entities_info[entity].file_types)
  {
    DEBUGF("Entity2: No data function for entity %zu\n", entity);
    return no_data(entity);
  }

  int const file_type = pick_file_type(data_request->file_types,
    &*entities_info[entity].file_types);

  /* It would be better not to have to use an intermediate buffer */
  int const buf_size = get_estimated_size(entity, file_type);
  DEBUGF("Entity2: Allocating local buffer of %d bytes\n", buf_size);
  void *entity_data;
  if (!flex_alloc(&entity_data, buf_size))
  {
    return no_mem();
  }

  Writer writer;
  writer_flex_init(&writer, &entity_data);

  DEBUGF("Entity2: Calling data function with arg %p for entity %zu\n",
         entities_info[entity].client_handle, entity);

  bool success = entities_info[entity].write_method(&writer, file_type,
    "EntityData", entities_info[entity].client_handle);

  /* Destroying a writer can fail because it flushes buffered output. */
  long int const nbytes = writer_destroy(&writer);
  _Optional CONST _kernel_oserror *e = NULL;
  if (success)
  {
    if (nbytes < 0)
    {
      success = false;
      e = lookup_error("WriteFail", "EntityData");
    }
    else if (read_method)
    {
      Reader reader;
      reader_flex_init(&reader, &entity_data);
      success = read_method(&reader, flex_size(&entity_data), file_type,
        "EntityData", client_handle);
      reader_destroy(&reader);
    }
  }

  flex_free(&entity_data);

  if (!success && failed_method)
  {
    DEBUGF("Entity2: Calling failed function with arg %p\n", client_handle);
    failed_method(e, client_handle);
  }

  return NULL;
}

/* ----------------------------------------------------------------------- */

static void probe_own(size_t const entity,
  const WimpDataRequestMessage *const data_request,
  _Optional Entity2ProbeMethod *const probe_method,
  _Optional Entity2FailedMethod *const failed_method,
  void *const client_handle)
{
  assert(entity < ARRAY_SIZE(entities_info));
  assert(data_request != NULL);
  DEBUGF("Entity2: bypassing message protocol for entity %zu\n", entity);

  if (entities_info[entity].write_method && entities_info[entity].file_types)
  {
    if (probe_method)
    {
      int const file_type = pick_file_type(data_request->file_types,
        &*entities_info[entity].file_types);

      DEBUGF("Entity2: Calling probe function with arg %p\n", client_handle);
      probe_method(file_type, client_handle);
    }
  }
  else
  {
    DEBUGF("Entity2: No data function for entity %zu\n", entity);
    if (failed_method)
    {
      DEBUGF("Entity2: Calling failed function with arg %p\n", client_handle);
      failed_method(no_data(entity), client_handle);
    }
  }
}

/* ----------------------------------------------------------------------- */

static _Optional CONST _kernel_oserror *probe_or_request_remote(size_t const entity,
  const WimpDataRequestMessage *const data_request,
  _Optional Entity2ProbeMethod *const probe_method,
  _Optional Entity2ReadMethod *const read_method,
  _Optional Entity2FailedMethod *const failed_method,
  void *const client_handle)
{
  assert(entity < ARRAY_SIZE(entities_info));
  assert(data_request != NULL);
  DEBUGF("Entity2: Creating a record for a data %s of entity %zu\n",
        !read_method ? "probe" : "import", entity);

  _Optional RequestOpData *const request_op_data = malloc(sizeof(*request_op_data));
  if (request_op_data == NULL)
  {
    return no_mem();
  }

  /* Initialise record for a new save operation */
  *request_op_data = (RequestOpData){
    .entity = entity,
    .data_request_ref = 0,
    .probe_method = probe_method,
    .read_method = read_method,
    .failed_method = failed_method,
    .client_handle = client_handle,
  };

  /* Add new record to head of linked list */
  linkedlist_insert(&request_op_data_list, NULL, &request_op_data->list_item);
  DEBUGF("Entity2: New record is at %p\n", (void *)request_op_data);

  _Optional CONST _kernel_oserror *const e = request_data(&*request_op_data, data_request);
  if (e != NULL)
  {
    destroy_op(&*request_op_data);
  }
  return e;
}

/* ----------------------------------------------------------------------- */

static bool cancel_matching_request(LinkedList *const list,
  LinkedListItem *const item, void *const arg)
{
  RequestOpData * const request_op_data = (RequestOpData *)item;
  assert(request_op_data != NULL);
  NOT_USED(list);

  /* Check whether this data request is for delivery to the specified handle.
     NULL means cancel all. */
  if (arg == NULL || request_op_data->client_handle == arg) {
    DEBUGF("Entity2: Matched with data request %p\n", (void *)request_op_data);

    /* Are we still waiting for a DataSave message in reply to
       our DataRequest? */
    if (!request_op_data->data_request_ref) {
      /* No - cancel the load operation (thus cancelling the data request) */
      loader3_cancel_receives(request_op_data);
    } else {
      /* Yes - cancel the data request directly */
      report_fail(NULL, request_op_data);
    }
  }
  return false; /* next item */
}

/* ----------------------------------------------------------------------- */

static void releaseentity_bounce(int const my_ref)
{
  if (my_ref != releaseentity_msg_ref)
  {
    DEBUGF("Entity2: Not our ReleaseEntity message (ref. %d)\n",
           releaseentity_msg_ref);
    return;
  }

  releaseentity_msg_ref = 0; /* reset message ID */

  /* No other task requested the data associated with our entities, so it
     will be lost when we die. */
  exit_if_pending();
}

/* ----------------------------------------------------------------------- */

static void datarequest_bounce(int const my_ref)
{
  _Optional RequestOpData *const request_op_data = find_data_req(my_ref);

  if (request_op_data == NULL)
  {
    DEBUGF("Entity2: Unknown DataRequest message\n");
    return;
  }

  DEBUGF("Entity2: It's our DataRequest message\n");
  report_fail(no_data(request_op_data->entity), &*request_op_data);
}

/* ----------------------------------------------------------------------- */

static void release_own(size_t const entity)
{
  assert(entity < ARRAY_SIZE(entities_info));

  /* Tell the owner of this entity that it has been usurped */
#ifdef COPY_ARRAY_ARGS
  free(entities_info[entity].file_types);
#endif

  if (entities_info[entity].lost_method) {
    DEBUGF("Entity2: Calling release function with arg %p for entity %zu\n",
          entities_info[entity].client_handle, entity);

    entities_info[entity].lost_method(entities_info[entity].client_handle);
  } else {
    DEBUGF("Entity2: No release function for entity %zu\n", entity);
  }

  entities_info[entity] = (Entity2Info){.write_method = (Saver2WriteMethod *)NULL};
}

/* -----------------------------------------------------------------------
                        Wimp message handlers
*/

static int claimentity_handler(WimpMessage *const message, void *const handle)
{
  assert(message != NULL);
  assert(message->hdr.action_code == Wimp_MClaimEntity);
  unsigned int not_owned = ~owned_entities;
  NOT_USED(handle);

  const WimpClaimEntityMessage *const claim_entity =
    (WimpClaimEntityMessage *)&message->data;

  DEBUGF("Entity2: Received a ClaimEntity message (ref. %d in reply to %d)\n",
        message->hdr.my_ref, message->hdr.your_ref);

  /* Check our record of the ClaimEntity broadcasts we did recently */
  for (size_t index = 0; index < ARRAY_SIZE(claimentity_msg_ref); index++)
  {
    if (message->hdr.my_ref == claimentity_msg_ref[index]) {
      DEBUGF("Entity2: Ignoring our own claim (freeing slot %zu)\n", index);
      claimentity_msg_ref[index] = 0;
      return 1; /* it is our own broadcast - ignore and claim message */
    }
  }
  DEBUGF("Entity2: Not one of our broadcasts\n");

  if (TEST_BITS(owned_entities, claim_entity->flags))
  {
    /* Another task has usurped our ownership of some entities */
    entity2_release(claim_entity->flags);
    CLEAR_BITS(owned_entities, claim_entity->flags);
  }

  /* Claim message unless entities are being claimed that we didn't own */
  return !TEST_BITS(not_owned, claim_entity->flags);
}

/* ----------------------------------------------------------------------- */

static int datasave_handler(WimpMessage *const message, void *const handle)
{
  /* This handler must receive DataSave messages before the Loader
     component. We need to intercept replies to our DataRequest message. */
  NOT_USED(handle);

  DEBUGF("Entity2: Received a DataSave message (ref. %d in reply to %d)\n",
        message->hdr.my_ref, message->hdr.your_ref);

  _Optional RequestOpData *const request_op_data = find_data_req(message->hdr.your_ref);
  if (request_op_data == NULL)
  {
    DEBUGF("Entity2: Unknown your_ref value\n");
    return 0; /* not a reply to our message */
  }
  DEBUGF("Entity2: It's a reply to our DataRequest\n");

  if (!request_op_data->read_method)
  {
    /* We were just probing for data */
    DEBUGF("Entity2: We are just probing for data\n");
    probe_finished(message->data.data_save.file_type, &*request_op_data);
  }
  else
  {
    /* Attempt to load the data associated with this entity */
    DEBUGF("Entity2: Will load data associated with entity\n");
    request_op_data->data_request_ref = 0; /* prevent future matches */

    _Optional CONST _kernel_oserror *const e = loader3_receive_data(message, load_data,
      report_fail, &*request_op_data);

    if (e != NULL)
    {
      report_fail(e, &*request_op_data);
    }
  }

  return 1; /* claim message to prevent transfer of entity data */
}

/* ----------------------------------------------------------------------- */

static int datarequest_handler(WimpMessage *const message, void *const handle)
{
  assert(message != NULL);
  assert(message->hdr.action_code == Wimp_MDataRequest);
  NOT_USED(handle);

  const WimpDataRequestMessage *const data_request =
    (WimpDataRequestMessage *)&message->data;

  DEBUGF("Entity2: Received a DataRequest message with flags %d "
        "(ref. %d in reply to %d)\n", data_request->flags, message->hdr.my_ref,
        message->hdr.your_ref);

  if (message->hdr.your_ref != 0 &&
      message->hdr.your_ref == releaseentity_msg_ref)
  {
    DEBUGF("Entity2: It's a reply to our ReleaseEntity message\n");
    releaseentity_msg_ref = 0; /* reset message ID */
  }

  if (!TEST_BITS(owned_entities, data_request->flags))
  {
    DEBUGF("Entity2: Not owned by us\n");
    return 0; /* we don't own any of the specified entities */
  }

  for (size_t entity = 0; entity < ARRAY_SIZE(entities_info); entity++)
  {
    if (!TEST_BITS(data_request->flags, 1u<<entity) ||
        !TEST_BITS(owned_entities, 1u<<entity))
      continue; /* we don't own this entity, or data not requested */

    if (entities_info[entity].write_method && entities_info[entity].file_types)
    {
      int const file_type = pick_file_type(data_request->file_types,
        &*entities_info[entity].file_types);

      WimpMessage ds;
      ds.hdr.your_ref = message->hdr.my_ref;
      /* action code and message size are filled out automatically */
      ds.data.data_save.destination_window =
        data_request->destination_window;

      ds.data.data_save.destination_icon = data_request->destination_icon;
      ds.data.data_save.destination_x = data_request->destination_x;
      ds.data.data_save.destination_y = data_request->destination_y;
      ds.data.data_save.estimated_size = get_estimated_size(entity, file_type);
      ds.data.data_save.file_type = file_type;
      STRCPY_SAFE(ds.data.data_save.leaf_name, "EntityData");

      _Optional CONST _kernel_oserror *const e = saver2_send_data(message->hdr.sender,
        &ds, entities_info[entity].write_method, send_complete, send_failed,
        entities_info[entity].client_handle);

      if (e != NULL)
      {
        report_error(&*e);
      }
      else
      {
        data_sent_count++;
        DEBUGF("Entity2: Have started data transfer %u\n", data_sent_count);
      }
    }
    else
    {
      DEBUGF("Entity2: No data function for entity %zu\n", entity);
    }
  }

  /* Claim message unless data requested for any entities that we don't own */
  return !TEST_BITS(~owned_entities, data_request->flags);
}

static const struct
{
  int                 msg_no;
  WimpMessageHandler *handler;
}
msg_handlers[] =
{
  {
    Wimp_MClaimEntity,
    claimentity_handler
  },
  {
    Wimp_MDataRequest,
    datarequest_handler
  },
  {
    Wimp_MDataSave,
    datasave_handler
  }
};

/* -----------------------------------------------------------------------
                        Wimp event handlers
*/

static int msg_bounce_handler(int const event_code, WimpPollBlock *const event,
  IdBlock *const id_block, void *const handle)
{
  assert(event_code == Wimp_EUserMessageAcknowledge);
  NOT_USED(event_code);
  assert(event != NULL);
  NOT_USED(id_block);
  NOT_USED(handle);

  DEBUGF("Entity2: Received a bounced message (ref. %d)\n",
        event->user_message_acknowledge.hdr.my_ref);

  switch (event->user_message_acknowledge.hdr.action_code)
  {
    case Wimp_MReleaseEntity:
      releaseentity_bounce(event->user_message_acknowledge.hdr.my_ref);
      return 1; /* claim event */

    case Wimp_MDataRequest:
      datarequest_bounce(event->user_message_acknowledge.hdr.my_ref);
      return 1; /* claim event */
  }
  return 0; /* pass on event */
}

/* -----------------------------------------------------------------------
                         Public library functions
*/

_Optional CONST _kernel_oserror *entity2_initialise(
  _Optional MessagesFD *const mfd,
  void (*const error_method)(CONST _kernel_oserror *))
{
  assert(!initialised);

  /* Store pointers to messages file descriptor and error-reporting function */
  desc = mfd;
  report_fn = error_method;

  /* We rely on a logical comformity between the flags usage in several
     distinct Wimp messages */
  assert(Wimp_MClaimEntity_Clipboard == Wimp_MDataRequest_Clipboard);
  assert(Wimp_MClaimEntity_Clipboard == Wimp_MReleaseEntity_Clipboard);

  /* Register Wimp message handlers */
  for (size_t i = 0; i < ARRAY_SIZE(msg_handlers); i++)
  {
    ON_ERR_RTN_E(event_register_message_handler(msg_handlers[i].msg_no,
                                                msg_handlers[i].handler,
                                                (void *)NULL));
  }

  /* Register handler for messages that return to us as wimp event 19 */
  ON_ERR_RTN_E(event_register_wimp_handler(-1,
                                           Wimp_EUserMessageAcknowledge,
                                           msg_bounce_handler,
                                           (void *)NULL));

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

_Optional CONST _kernel_oserror *entity2_claim(unsigned int const flags,
  _Optional const int *file_types, _Optional Entity2EstimateMethod *const estimate_method,
  _Optional Saver2WriteMethod *const write_method, _Optional Entity2LostMethod *const lost_method,
  void *const client_handle)
{
  DEBUGF("Entity2: Request to claim flags %u (handle:%p)\n",
    flags, client_handle);

  assert(initialised);
  assert(file_types != NULL || !write_method);

  if (!flags)
    return NULL;

  _Optional CONST _kernel_oserror *e = NULL;

#ifdef COPY_ARRAY_ARGS
  _Optional int *file_types_copy[NEntities];
  for (size_t entity = 0; entity < ARRAY_SIZE(entities_info); entity++)
  {
    if (!TEST_BITS(flags, 1u<<entity))
      continue;

    if (file_types == NULL)
    {
      file_types_copy[entity] = NULL;
      continue;
    }

    /* make a private copy of the array of file types (+1 for terminator) */
    size_t const array_len = count_file_types(&*file_types) + 1;
    file_types_copy[entity] = malloc(array_len * sizeof(file_types[0]));
    if (file_types_copy[entity] == NULL)
    {
      e = no_mem();
      file_types = NULL;
      continue;
    }

    (void)copy_file_types(&*file_types_copy[entity], &*file_types, array_len - 1);
  }
#endif

  if (e == NULL)
  {
    e = claim_entities(flags);
  }

  /* Store new handler functions for the specified entities */
  for (size_t entity = 0; entity < ARRAY_SIZE(entities_info); entity++)
  {
    if (!TEST_BITS(flags, 1u<<entity))
      continue; /* not replacing owner of this entity */

    if (e != NULL)
    {
#ifdef COPY_ARRAY_ARGS
      free(file_types_copy[entity]);
#endif
      continue;
    }

#ifdef COPY_ARRAY_ARGS
    if (!file_types_copy[entity])
    {
      continue;
    }
#endif

    release_own(entity);

    /* Record the new client handle and function pointers */
    entities_info[entity] = (Entity2Info){
      .estimate_method = estimate_method,
      .write_method = write_method,
      .lost_method = lost_method,
      .client_handle = client_handle,
#ifdef COPY_ARRAY_ARGS
      .file_types = file_types_copy[entity],
#else
      .file_types = file_types,
#endif
    };
  }

  DEBUGF("Entity2: Claim complete\n");
  return e;
}

/* ----------------------------------------------------------------------- */

_Optional CONST _kernel_oserror *entity2_request_data(
  const WimpDataRequestMessage *const data_request,
  _Optional Entity2ReadMethod *const read_method,
  _Optional Entity2FailedMethod *const failed_method,
  void *const client_handle)
{
  assert(data_request != NULL);
  DEBUGF("Entity2: Data request for flags 0x%x to coords %d,%d in window %d "
         "and icon %d\n", data_request->flags, data_request->destination_x,
         data_request->destination_y, data_request->destination_window,
         data_request->destination_icon);

  assert(initialised);

  for (size_t entity = 0; entity < ARRAY_SIZE(entities_info); entity++)
  {
    if (!TEST_BITS(data_request->flags, 1u<<entity))
      continue; /* data not requested for this entity */

    if (TEST_BITS(owned_entities, 1u<<entity))
    {
      /* We own this entity so we can bypass the message protocol.
         Deliver any error via the failed_method since we may already
         have called the read_method. */
      ON_ERR_RTN_E(request_own(entity, data_request, read_method,
        failed_method, client_handle));
    }
    else
    {
      /* We don't own this entity, so we must request the associated data
         from its owner. */
      ON_ERR_RTN_E(probe_or_request_remote(entity, data_request, (Entity2ProbeMethod *)NULL,
        read_method, failed_method, client_handle));
    }
  }

  return NULL; /* success */
}

/* ----------------------------------------------------------------------- */

_Optional CONST _kernel_oserror *entity2_probe_data(
  const WimpDataRequestMessage *const data_request,
  _Optional Entity2ProbeMethod *const probe_method,
  _Optional Entity2FailedMethod *const failed_method,
  void *const client_handle)
{
  assert(data_request != NULL);
  DEBUGF("Entity2: Data probe for flags 0x%x to coords %d,%d in window %d "
         "and icon %d\n", data_request->flags, data_request->destination_x,
         data_request->destination_y, data_request->destination_window,
         data_request->destination_icon);

  assert(initialised);

  for (size_t entity = 0; entity < ARRAY_SIZE(entities_info); entity++)
  {
    if (!TEST_BITS(data_request->flags, 1u<<entity))
      continue; /* data not requested for this entity */

    if (TEST_BITS(owned_entities, 1u<<entity))
    {
      /* We own this entity so we can bypass the message protocol.
         Deliver any error via the failed_method since we may already
         have called the read_method. */
      probe_own(entity, data_request, probe_method, failed_method,
        client_handle);
    }
    else
    {
      /* We don't own this entity, so we must request the associated data
         from its owner. */
      ON_ERR_RTN_E(probe_or_request_remote(entity, data_request, probe_method,
        (Entity2ReadMethod *)NULL, failed_method, client_handle));
    }
  }

  return NULL; /* success */
}

/* ----------------------------------------------------------------------- */

void entity2_cancel_requests(void *const client_handle)
{
  /* Cancel any outstanding data requests for the specified client function
     and handle. Use when the destination has become invalid. */
  DEBUGF("Entity2: Cancelling all data requests with arg %p\n", client_handle);
  assert(client_handle != NULL);
  linkedlist_for_each(&request_op_data_list, cancel_matching_request,
                      client_handle);
}

/* ----------------------------------------------------------------------- */

void entity2_release(unsigned int const flags)
{
  DEBUGF("Entity2: Request to release flags %u\n", flags);
  assert(initialised);

  /* If our task does not own the specified entities then we should
     not have any claimant records for them. */
  if (!TEST_BITS(owned_entities, flags)) {
    DEBUGF("Entity2: Not owned by us\n");
    return; /* nothing to do */
  }

  /* Wipe the handler functions for the specified entities */
  for (size_t entity = 0; entity < ARRAY_SIZE(entities_info); entity++)
  {
    if (!TEST_BITS(flags, 1u<<entity) || !TEST_BITS(owned_entities, 1u<<entity))
      continue; /* we don't own this entity, or not released */

    release_own(entity);
  }

  /* Note that our task still owns the released entities until another
     task claims them. */
}

/* ----------------------------------------------------------------------- */

#ifdef INCLUDE_FINALISATION_CODE
_Optional CONST _kernel_oserror *entity2_finalise(void)
{
  _Optional CONST _kernel_oserror *return_error = NULL;

  assert(initialised);
  initialised = false;

  /* Release all entities that we own */
  entity2_release(owned_entities);

  DEBUGF("Entity2: Cancelling outstanding operations\n");
  linkedlist_for_each(&request_op_data_list, cancel_matching_request, (void *)NULL);

  /* Deregister Wimp message handlers */
  for (size_t i = 0; i < ARRAY_SIZE(msg_handlers); i++)
  {
    MERGE_ERR(return_error,
              event_deregister_message_handler(msg_handlers[i].msg_no,
                                               msg_handlers[i].handler,
                                               (void *)NULL));
  }

  /* Deregister handler for messages that return to us as wimp event 19 */
  MERGE_ERR(return_error,
            event_deregister_wimp_handler(-1,
                                          Wimp_EUserMessageAcknowledge,
                                          msg_bounce_handler,
                                          (void *)NULL));

  return return_error;
}
#endif

/* ----------------------------------------------------------------------- */

_Optional CONST _kernel_oserror *entity2_dispose_all(Entity2ExitMethod *const exit_method)
{
  bool data_found;

  DEBUGF("Entity2: Releasing all entities (%s post-function)\n",
        exit_method ? "with" : "without");
  assert(initialised);

  /* Search for entities owned by us, that may have data associated with them */
  data_found = false;
  if (owned_entities)
  {
    for (size_t entity = 0; entity < ARRAY_SIZE(entities_info); entity++)
    {
      if (!TEST_BITS(owned_entities, 1u<<entity) ||
          !entities_info[entity].write_method)
        continue;

      DEBUGF("Entity2: Found data function for entity %zu\n", entity);
      data_found = true;
      break;
    }
  }

  if (data_found)
  {
    /* Broadcast notification that the holder of some entities is dying */
    WimpMessage message = {
      .hdr = {
        .size = sizeof(message.hdr) + sizeof(WimpReleaseEntityMessage),
        .your_ref = 0,
        .action_code = Wimp_MReleaseEntity,
      },
    };
    WimpReleaseEntityMessage *rem = (WimpReleaseEntityMessage *)&message.data;
    rem->flags = owned_entities;
    ON_ERR_RTN_E(wimp_send_message(Wimp_EUserMessageRecorded, &message, 0, 0,
                                   NULL));
    /* A clipboard holder application will respond to this message by
       requesting the data held by us, before we die */
    DEBUGF("Entity2: Broadcast ReleaseEntity message (ref. %d)\n",
          message.hdr.my_ref);
    assert(releaseentity_msg_ref == 0);
    releaseentity_msg_ref = message.hdr.my_ref; /* record our message's ID */
    client_exit_method = exit_method; /* call-back function (may be NULL) */
    finalisation_pending = true;
  }
  else
  {
    DEBUGF("Entity2: We don't own any entities with associated data\n");
    if (exit_method)
      exit_method();
  }

  return NULL; /* success */
}
