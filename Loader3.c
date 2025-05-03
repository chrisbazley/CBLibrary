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

/* History:
  CJB: 24-Sep-19: Created this source file from Loader2.
  CJB: 28-Oct-19: First released version.
  CJB: 09-Nov-19: Pass the leafname instead of "<Wimp$Scrap>" when calling
                  the Loader3ReadMethod. Pass the estimated file size as an
                  extra argument.
  CJB: 10-Nov-19: Allocate RAM transfer buffers one byte longer than requested
                  to try to avoid having to send a second RAMFetch message.
                  Modified loader2_buffer_file() to use get_file_size().
  CJB: 20-Dec-19: Fixed a misleading debugging message in report_fail().
  CJB: 01-Nov-20: Assign a compound literal to initialise a load operation.
  CJB: 07-Nov-20: Added the loader3_load_file function to allow DataOpen and
                  DataLoad handlers to reuse existing code.
  CJB: 03-May-25: Fix #include filename case.
*/

/* ISO library headers */
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "wimp.h"
#include "event.h"
#include "wimplib.h"
#include "flex.h"
#include "toolbox.h"

/* StreamLib headers */
#include "ReaderRaw.h"
#include "ReaderFlex.h"

/* CBUtilLib headers */
#include "LinkedList.h"

/* CBOSLib headers */
#include "MessTrans.h"

/* Local headers */
#include "Internal/CBMisc.h"
#include "Loader3.h"
#include "NoBudge.h"
#include "scheduler.h"
#include "FOpenCount.h"
#include "FileUtils.h"

/* The following structure holds all the state for a given load operation */
typedef struct
{
  LinkedListItem list_item;
  int   last_message_ref;
  int   last_message_type;
  int   bytes_received;
  bool  RAM_capable;
  bool  idle_function;
  bool  no_flex_budge;
  void *RAM_buffer;
  Loader3ReadMethod   *read_method;
  Loader3FailedMethod *failed_method;
  void                *client_handle;
  WimpMessage datasave_msg;
}
LoadOpData;

/* Constant numeric values */
enum
{
  DefaultBufferSize = BUFSIZ,
  BufExtendMul = 2,
  BufExtendDiv = 1,
  DataLoadWaitTime = 3000, /* Centiseconds to wait for a DataLoad
                               in reply to our DataSaveAck */
  DestinationUnsafe = -1,  /* Estimated size value to indicate unsafe
                              destination */
  PreExpandHeap = BUFSIZ /* Number of bytes to pre-allocate
                                before disabling flex budging */
};

/* -----------------------------------------------------------------------
                          Internal library data
*/

static bool initialised;
static LinkedList load_op_data_list;
static MessagesFD *desc;

/* -----------------------------------------------------------------------
                         Miscellaneous internal functions
*/

static CONST _kernel_oserror *lookup_error(const char *const token,
  const char *const param)
{
  /* Look up error message from the token, outputting to an internal buffer */
  return messagetrans_error_lookup(desc, DUMMY_ERRNO, token, 1, param);
}

/* ----------------------------------------------------------------------- */

static CONST _kernel_oserror *no_mem(void)
{
  return lookup_error("NoMem", NULL);
}

/* ----------------------------------------------------------------------- */

static SchedulerIdleFunction time_out;

static void destroy_op(LoadOpData *const load_op_data)
{
  DEBUGF("Loader3: Removing record of operation %p\n", (void *)load_op_data);
  assert(load_op_data != NULL);

  if (load_op_data->no_flex_budge)
    nobudge_deregister();

  if (load_op_data->idle_function)
    scheduler_deregister(time_out, load_op_data);

  if (load_op_data->RAM_buffer != NULL)
    flex_free(&load_op_data->RAM_buffer);

  linkedlist_remove(&load_op_data_list, &load_op_data->list_item);
  free(load_op_data);
}

/* ----------------------------------------------------------------------- */

static void report_fail(LoadOpData *const load_op_data,
  CONST _kernel_oserror *const e)
{
  assert(load_op_data != NULL);

  if (e != NULL)
  {
    DEBUGF("Loader3: Error 0x%x, %s\n", e->errnum, e->errmess);
  }

  if (load_op_data->failed_method != NULL)
  {
    DEBUGF("Loader3: calling failed function with arg %p\n",
           load_op_data->client_handle);

    load_op_data->failed_method(e, load_op_data->client_handle);
  }
}

/* ----------------------------------------------------------------------- */

static SchedulerTime time_out(void *const handle,
  SchedulerTime const time_now, const volatile bool *const time_up)
{
  /* This function is called by the scheduler 30 seconds after we start
     a load operation (unless we cancel it in the interim). We use it to
     free up resources associated with stalled load operations. */
  LoadOpData *const load_op_data = handle;
  assert(load_op_data != NULL);
  NOT_USED(time_up);

  DEBUGF("Loader3: Load operation %p timed out\n", (void *)load_op_data);
  report_fail(load_op_data, NULL);
  destroy_op(load_op_data);

  return time_now; /* Return time doesn't actually matter because already
                      deregistered this function */
}

/* ----------------------------------------------------------------------- */

static bool op_has_ref(LinkedList *const list,
  LinkedListItem *const item, void *const arg)
{
  const int *const msg_ref = arg;
  const LoadOpData * const load_op_data = (LoadOpData *)item;

  assert(msg_ref != NULL);
  assert(load_op_data != NULL);
  NOT_USED(list);

  return load_op_data->last_message_ref == *msg_ref;
}

/* ----------------------------------------------------------------------- */

static LoadOpData *find_record(int msg_ref)
{
  DEBUGF("Loader3: Searching for operation awaiting reply to %d\n", msg_ref);
  if (!msg_ref)
    return NULL;

  LoadOpData *const load_op_data = (LoadOpData *)linkedlist_for_each(
    &load_op_data_list, op_has_ref, &msg_ref);

  if (load_op_data == NULL)
  {
    DEBUGF("Loader3: End of linked list (no match)\n");
  }
  else
  {
    DEBUGF("Loader3: Record %p has matching message ID\n",
      (void *)load_op_data);
  }
  return load_op_data;
}

/* ----------------------------------------------------------------------- */

static bool cancel_matching_op(LinkedList *const list,
  LinkedListItem *const item, void *const arg)
{
  LoadOpData * const load_op_data = (LoadOpData *)item;
  assert(load_op_data != NULL);
  NOT_USED(list);

  /* Check whether this data request is for delivery to the specified handle.
     NULL means cancel all. */
  if (arg == NULL || load_op_data->client_handle == arg)
  {
    report_fail(load_op_data, NULL);
    destroy_op(load_op_data);
  }
  return false; /* next item */
}

/* ----------------------------------------------------------------------- */

static CONST _kernel_oserror *send_msg(LoadOpData *const load_op_data,
  int const code, WimpMessage *const msg, int const handle)
{
  assert(load_op_data != NULL);
  assert(code == Wimp_EUserMessage || code == Wimp_EUserMessageRecorded);
  assert(msg != NULL);

  ON_ERR_RTN_E(wimp_send_message(code, msg, handle, 0, NULL));

  load_op_data->last_message_ref = msg->hdr.my_ref;
  load_op_data->last_message_type = msg->hdr.action_code;
  DEBUGF("Loader3: sent message with code %d and ref. %d in reply to %d\n",
         msg->hdr.action_code, msg->hdr.my_ref, msg->hdr.your_ref);

  return NULL;
}

/* ----------------------------------------------------------------------- */

static bool read_data(LoadOpData *const load_op_data, Reader *const reader)
{
  assert(load_op_data != NULL);
  assert(reader != NULL);
  assert(!reader_ferror(reader));

  bool success = true;
  if (load_op_data->read_method != NULL)
  {
    success = load_op_data->read_method(reader,
      load_op_data->bytes_received,
      load_op_data->datasave_msg.data.data_save.file_type,
      load_op_data->datasave_msg.data.data_save.leaf_name,
      load_op_data->client_handle);
  }
  return success;
}

/* ----------------------------------------------------------------------- */

static bool load_file(LoadOpData *const load_op_data,
  const char *const file_path)
{
  DEBUGF("Loader3: Loading %s\n", file_path);

  CONST _kernel_oserror *const e = get_file_size(file_path,
    &load_op_data->bytes_received);

  if (e != NULL)
  {
    report_fail(load_op_data, e);
    return false;
  }

  FILE *const f = fopen_inc(file_path, "rb"); /* open for reading */
  if (f == NULL)
  {
    report_fail(load_op_data, lookup_error("OpenInFail", file_path));
    return false;
  }

  Reader reader;
  reader_raw_init(&reader, f);
  bool const success = read_data(load_op_data, &reader);
  reader_destroy(&reader);
  fclose_dec(f);

  return success;
}

/* ----------------------------------------------------------------------- */

static CONST _kernel_oserror *send_datasaveack(LoadOpData *const load_op_data)
{
  assert(load_op_data != NULL);
  DEBUGF("Loader3: Replying to DataSave message ref. %d\n",
    load_op_data->datasave_msg.hdr.my_ref);

  /* Allocate (very) temporary buffer for a DataSaveAck message */
  int const msg_size = WORD_ALIGN(sizeof(load_op_data->datasave_msg.hdr) +
                        offsetof(WimpDataSaveAckMessage, leaf_name) +
                        sizeof("<Wimp$Scrap>"));

  WimpMessage *const data_save_ack = malloc(msg_size);
  if (data_save_ack == NULL)
  {
    return no_mem();
  }

  /* Populate header of DataSaveAck message */
  data_save_ack->hdr.size = msg_size;
  data_save_ack->hdr.your_ref = load_op_data->datasave_msg.hdr.my_ref;
  data_save_ack->hdr.action_code = Wimp_MDataSaveAck;

  /* Populate body of DataSaveAck message
     (mostly copied from the DataSave message) */
  data_save_ack->data.data_save_ack.destination_window =
    load_op_data->datasave_msg.data.data_save.destination_window;

  data_save_ack->data.data_save_ack.destination_icon =
    load_op_data->datasave_msg.data.data_save.destination_icon;

  data_save_ack->data.data_save_ack.destination_x =
    load_op_data->datasave_msg.data.data_save.destination_x;

  data_save_ack->data.data_save_ack.destination_y =
    load_op_data->datasave_msg.data.data_save.destination_y;

  data_save_ack->data.data_save_ack.estimated_size = DestinationUnsafe;

  data_save_ack->data.data_save_ack.file_type =
    load_op_data->datasave_msg.data.data_save.file_type;

  strcpy(data_save_ack->data.data_save_ack.leaf_name, "<Wimp$Scrap>");

  /* Send our reply to the sender of the DataSave message */
  CONST _kernel_oserror *const e = send_msg(load_op_data, Wimp_EUserMessage,
    data_save_ack, load_op_data->datasave_msg.hdr.sender);

  free(data_save_ack);
  return e;
}

/* ----------------------------------------------------------------------- */

static CONST _kernel_oserror *send_dataloadack(LoadOpData *const load_op_data,
  WimpMessage *const message)
{
  assert(load_op_data != NULL);
  assert(message != NULL);
  assert(message->hdr.action_code == Wimp_MDataLoad);

  /* Reply to the sender of the DataLoad message. */
  message->hdr.your_ref = message->hdr.my_ref;
  message->hdr.action_code = Wimp_MDataLoadAck;

  return send_msg(load_op_data, Wimp_EUserMessage, message, message->hdr.sender);
}

/* ----------------------------------------------------------------------- */

static CONST _kernel_oserror *send_ramfetch(
  LoadOpData *const load_op_data, WimpMessage const *const message)
{
  assert(load_op_data != NULL);
  assert(message != NULL);
  assert(message->hdr.action_code == Wimp_MRAMTransmit ||
         message->hdr.action_code == Wimp_MDataSave);

  DEBUGF("Loader3: Replying to message ref. %d\n", message->hdr.my_ref);

  /* Allocate (very) temporary buffer for a RAMFetch message */
  int const msg_size = sizeof(message->hdr) + sizeof(WimpRAMFetchMessage);
  WimpMessage *const ram_fetch = malloc(msg_size);
  if (ram_fetch == NULL)
  {
    return no_mem();
  }

  /* Populate header of RAMFetch message */
  ram_fetch->hdr.size = msg_size;
  ram_fetch->hdr.your_ref = message->hdr.my_ref;
  ram_fetch->hdr.action_code = Wimp_MRAMFetch;

  /* Populate body of RAMFetch message
     (tell them to write at the end of the data already received) */
  if (!load_op_data->no_flex_budge)
  {
    nobudge_register(PreExpandHeap); /* copy of flex anchor in message */
    load_op_data->no_flex_budge = true;
  }

  assert(load_op_data->RAM_buffer != NULL);
  ram_fetch->data.ram_fetch.buffer = (char *)load_op_data->RAM_buffer +
    load_op_data->bytes_received;

  ram_fetch->data.ram_fetch.buffer_size = flex_size(&load_op_data->RAM_buffer) -
    load_op_data->bytes_received;

  /* Send our reply to the sender of the RAMTransmit or DataSave message */
  CONST _kernel_oserror *const err = send_msg(load_op_data,
    Wimp_EUserMessageRecorded, ram_fetch, message->hdr.sender);

  free(ram_fetch);
  return err;
}

/* ----------------------------------------------------------------------- */

static CONST _kernel_oserror *receive_ram(LoadOpData *const load_op_data,
  WimpMessage *const message)
{
  assert(load_op_data != NULL);
  assert(message != NULL);
  assert(message->hdr.action_code == Wimp_MRAMTransmit);
  assert(load_op_data->bytes_received <= flex_size(&load_op_data->RAM_buffer));

  if (load_op_data->no_flex_budge)
  {
    nobudge_deregister(); /* no need to protect RAM buffer anymore */
    load_op_data->no_flex_budge = false;
  }

  load_op_data->RAM_capable = true;
  load_op_data->bytes_received += message->data.ram_transmit.nbytes;
  int buf_size = flex_size(&load_op_data->RAM_buffer);

  if (load_op_data->bytes_received > buf_size)
  {
    DEBUGF("Loader3: RAM transfer buffer overflow (error)\n");
    return lookup_error("BufOFlo", NULL);
  }

  if (load_op_data->bytes_received < buf_size)
  {
    /* A RAMTransmit that doesn't fill our buffer signals the
       end of the message protocol */
    DEBUGF("Loader3: Trimming RAM transfer buffer to %d bytes\n",
      load_op_data->bytes_received);

    if (!flex_extend(&load_op_data->RAM_buffer, load_op_data->bytes_received))
    {
      return no_mem();
    }

    Reader reader;
    reader_flex_init(&reader, &load_op_data->RAM_buffer);
    read_data(load_op_data, &reader);
    reader_destroy(&reader);
    destroy_op(load_op_data);
    return NULL;
  }

  /* Extend the buffer for more data */
  buf_size = (buf_size * BufExtendMul) / BufExtendDiv;
  DEBUGF("Loader3: Extending RAM transfer buffer to %d bytes\n", buf_size);

  if (!flex_extend(&load_op_data->RAM_buffer, buf_size))
  {
    return no_mem();
  }

  return send_ramfetch(load_op_data, message);
}

/* ----------------------------------------------------------------------- */

static void ram_fetch_bounce(LoadOpData *const load_op_data)
{
  DEBUGF("Loader3: No reply to RAMFetch\n");
  assert(load_op_data != NULL);

  if (load_op_data->no_flex_budge)
  {
    nobudge_deregister(); /* no need to protect RAM buffer anymore */
    load_op_data->no_flex_budge = false;
  }

  /* If RAM transfer broke in the middle then PRM says "No error should be
     generated because the other end will have already reported one." */
  CONST _kernel_oserror *e = NULL;

  if (!load_op_data->RAM_capable)
  {
    /* Use file transfer instead, by replying to the old DataSave message */
    if (load_op_data->RAM_buffer)
    {
      flex_free(&load_op_data->RAM_buffer);
    }

    e = send_datasaveack(load_op_data);
    if (e == NULL)
    {
      return; /* success */
    }
  }

  report_fail(load_op_data, e);
  destroy_op(load_op_data);
}

/* -----------------------------------------------------------------------
                        Wimp message handlers
*/

static int dataload_handler(WimpMessage *const message,
  void *const handle)
{
  /* This handler must receive DataLoad messages before the Loader
     component. We need to intercept replies to our DataSave message. */
  assert(message != NULL);
  NOT_USED(handle);

  DEBUGF("Loader3: Received a DataLoad message (ref. %d in reply to %d)\n",
        message->hdr.my_ref, message->hdr.your_ref);

  LoadOpData *const load_op_data = find_record(message->hdr.your_ref);
  if (load_op_data == NULL)
  {
    DEBUGF("Loader3: Unknown your_ref value\n");
    return 0; /* not a reply to our message */
  }

  if (load_op_data->last_message_type != Wimp_MDataSaveAck)
  {
    DEBUGF("Loader3: Bad your_ref value\n");
    return 0; /* not a reply to a DataSave message */
  }

  if (message->data.data_load.file_type !=
      load_op_data->datasave_msg.data.data_save.file_type)
  {
    DEBUGF("Loader3: file type mismatch\n");
    report_fail(load_op_data, NULL);
  }
  else
  {
    if (load_file(load_op_data, message->data.data_load.leaf_name))
    {
      remove(message->data.data_load.leaf_name);
      CONST _kernel_oserror *const e = send_dataloadack(load_op_data, message);
      if (e != NULL)
      {
        /* It's too late to report failure because the client has already
           loaded the data. */
        DEBUGF("Loader3: Failed to send DataLoadAck: 0x%x, %s\n",
          e->errnum, e->errmess);
      }
    }
  }
  destroy_op(load_op_data);
  return 1; /* claim message */
}

/* ----------------------------------------------------------------------- */

static int ramtransmit_handler(WimpMessage *const message,
  void *const handle)
{
  assert(message != NULL);
  assert(message->hdr.action_code == Wimp_MRAMTransmit);
  NOT_USED(handle);

  DEBUGF("Loader3: Received a RAMTransmit message (ref. %d in reply to %d)\n",
        message->hdr.my_ref, message->hdr.your_ref);

  LoadOpData *const load_op_data = find_record(message->hdr.your_ref);
  if (load_op_data == NULL)
  {
    DEBUGF("Loader3: Unknown your_ref value\n");
    return 0; /* not a reply to our message */
  }

  if (load_op_data->last_message_type != Wimp_MRAMFetch)
  {
    DEBUGF("Loader3: Bad your_ref value\n");
    return 0; /* not a reply to a RAMFetch message */
  }

  DEBUGF("Loader3: %d bytes transferred to buffer at %p\n",
        message->data.ram_transmit.nbytes, message->data.ram_transmit.buffer);

  CONST _kernel_oserror *const e = receive_ram(load_op_data, message);
  if (e != NULL)
  {
    report_fail(load_op_data, e);
    destroy_op(load_op_data);
  }

  return 1; /* claim message */
}

/* -----------------------------------------------------------------------
                        Wimp event handlers
*/

static int msg_bounce_handler(int const event_code,
  WimpPollBlock *const event, IdBlock *const id_block, void *const handle)
{
  /* This is a handler for bounced messages */
  NOT_USED(event_code);
  assert(event != NULL);
  NOT_USED(id_block);
  NOT_USED(handle);

  DEBUGF("Loader3: Received a bounced message (ref. %d)\n",
        event->user_message_acknowledge.hdr.my_ref);

  LoadOpData *const load_op_data = find_record(
    event->user_message_acknowledge.hdr.my_ref);

  if (load_op_data == NULL)
  {
    DEBUGF("Loader3: Unknown message ID\n");
    return 0; /* not the last message we sent */
  }

  switch (event->user_message_acknowledge.hdr.action_code)
  {
    case Wimp_MRAMFetch:
      ram_fetch_bounce(load_op_data);
      return 1; /* claim event */
  }
  return 0; /* pass on event */
}

/* -----------------------------------------------------------------------
                         Public library functions
*/

CONST _kernel_oserror *loader3_initialise(MessagesFD *const mfd)
{
  assert(!initialised);

  /* Store pointer to messages file descriptor and error-reporting function */
  DEBUGF("Loader3: initialising with messages file descriptor %p\n",
    (void *)mfd);
  desc = mfd;

  /* Initialise linked list */
  linkedlist_init(&load_op_data_list);

  /* Register Wimp message handlers for data transfer protocol */

  ON_ERR_RTN_E(event_register_message_handler(Wimp_MDataLoad,
                                              dataload_handler,
                                              NULL));

  ON_ERR_RTN_E(event_register_message_handler(Wimp_MRAMTransmit,
                                              ramtransmit_handler,
                                              NULL));

  /* Register handler for messages that return to us as wimp event 19 */
  ON_ERR_RTN_E(event_register_wimp_handler(-1,
                                           Wimp_EUserMessageAcknowledge,
                                           msg_bounce_handler,
                                           NULL));

  /* Ensure that messages are not masked */
  unsigned int mask;
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
CONST _kernel_oserror *loader3_finalise(void)
{
  CONST _kernel_oserror *return_error = NULL;

  assert(initialised);
  initialised = false;

  DEBUGF("Loader3: Cancelling outstanding operations\n");
  linkedlist_for_each(&load_op_data_list, cancel_matching_op, NULL);

  /* Deregister Wimp message handlers for data transfer protocol */

  MERGE_ERR(return_error,
            event_deregister_message_handler(Wimp_MDataLoad,
                                             dataload_handler,
                                             NULL));

  MERGE_ERR(return_error,
            event_deregister_message_handler(Wimp_MRAMTransmit,
                                             ramtransmit_handler,
                                             NULL));

  /* Deregister handler for messages that return to us as wimp event 19 */
  MERGE_ERR(return_error,
            event_deregister_wimp_handler(-1,
                                          Wimp_EUserMessageAcknowledge,
                                          msg_bounce_handler,
                                          NULL));

  return return_error;
}
#endif

/* ----------------------------------------------------------------------- */

CONST _kernel_oserror *loader3_receive_data(const WimpMessage *const message,
  Loader3ReadMethod *const read_method,
  Loader3FailedMethod *const failed_method, void *const client_handle)
{
  assert(initialised);
  assert(message != NULL);
  assert(message->hdr.action_code == Wimp_MDataSave);

  DEBUGF("Loader3: Processing a DataSave message (ref. %d in reply to %d)\n",
        message->hdr.my_ref, message->hdr.your_ref);

  DEBUGF("Loader3: To icon %d in window %d at coordinates %d,%d\n",
        message->data.data_save.destination_icon,
        message->data.data_save.destination_window,
        message->data.data_save.destination_x,
        message->data.data_save.destination_y);

  DEBUGF("Loader3: File type is &%x, name is '%s' and est. size is %d bytes\n",
        message->data.data_save.file_type, message->data.data_save.leaf_name,
        message->data.data_save.estimated_size);

  /* Allocate memory for a new load operation */
  DEBUGF("Loader3: Creating a record for a new load operation\n");
  LoadOpData *const load_op_data = malloc(sizeof(*load_op_data));
  if (load_op_data != NULL)
  {
    /* Initialise record for a new load operation */
    *load_op_data = (LoadOpData){
      .RAM_capable = false,
      .idle_function = false,
      .no_flex_budge = false,
      .RAM_buffer = NULL, /* no flex block here */
      .bytes_received = 0,
      .read_method = read_method,
      .failed_method = failed_method,
      .client_handle = client_handle,
    };
  };
  if (load_op_data == NULL)
  {
    return no_mem();
  }

  /* Initialise record for a new load operation */
  load_op_data->datasave_msg = *message,

  /* Add new record to head of linked list */
  linkedlist_insert(&load_op_data_list, NULL, &load_op_data->list_item);

  /* There may be an indeterminate delay between us sending DataSaveAck
     and other task responding with a DataLoad message. (Sending
     DataSaveAck as recorded delivery breaks the SaveAs module, for one.)
     To prevent us leaking memory, we abandon stalled load operations after
     30 seconds. */
  CONST _kernel_oserror *e = scheduler_register_delay(
    time_out, load_op_data, DataLoadWaitTime, SchedulerPriority_Min);

  if (e == NULL)
  {
    load_op_data->idle_function = true;

    /* Can try RAM transfer (see if they support it) */

    /* Use estimated file size as buffer size unless it is implausible
       but allocate one extra byte to try to avoid a second RAMFetch. */
    int const buf_size =
      (message->data.data_save.estimated_size <= 0 ? DefaultBufferSize :
      message->data.data_save.estimated_size + 1);

    DEBUGF("Loader3: Allocating RAM transfer buffer of %d bytes\n", buf_size);
    if (!flex_alloc(&load_op_data->RAM_buffer, buf_size))
    {
      e = no_mem();
    }
    else
    {
      e = send_ramfetch(load_op_data, message);
    }
  }

  if (e != NULL)
  {
    destroy_op(load_op_data);
  }

  return e;
}

/* ----------------------------------------------------------------------- */

void loader3_cancel_receives(void *const client_handle)
{
  /* Cancel any outstanding load operations for the specified client function
     and handle. Use when the destination has become invalid. */
  DEBUGF("Loader3: Cancelling all load operations with handle %p\n",
         client_handle);
  assert(client_handle != NULL);
  linkedlist_for_each(&load_op_data_list, cancel_matching_op,
                      client_handle);
}

/* ----------------------------------------------------------------------- */

bool loader3_load_file(const char *const file_name, int const file_type,
  Loader3ReadMethod *const read_method,
  Loader3FailedMethod *const failed_method, void *const client_handle)
{
  assert(file_name);
  assert(*file_name != '\0');
  bool success = false;

  LoadOpData load_op_data = {
    .RAM_capable = false,
    .idle_function = false,
    .no_flex_budge = false,
    .RAM_buffer = NULL, /* no flex block here */
    .bytes_received = 0,
    .read_method = read_method,
    .failed_method = failed_method,
    .client_handle = client_handle,
  };

  if (strlen(file_name) >=
      sizeof(load_op_data.datasave_msg.data.data_save.leaf_name))
  {
    report_fail(&load_op_data, lookup_error("StrOFlo", NULL));
  }
  else
  {
    strcpy(load_op_data.datasave_msg.data.data_save.leaf_name, file_name);
    load_op_data.datasave_msg.data.data_save.file_type = file_type;
    success = load_file(&load_op_data, file_name);
  }

  return success;
}
