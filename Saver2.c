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

/* History:
  CJB: 24-Sep-19: Created this source file from the previous Saver.
  CJB: 28-Oct-19: First released version.
  CJB: 06-Nov-19: Fixed failure to check the return value of fclose_dec()
                  in save_file().
  CJB: 01-Nov-20: Assign a compound literal to initialise a save operation.
*/

/* ISO library headers */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "wimp.h"
#include "event.h"
#include "wimplib.h"
#include "flex.h"
#include "toolbox.h"

/* StreamLib headers */
#include "WriterRaw.h"
#include "WriterFlex.h"

/* CBUtilLib headers */
#include "LinkedList.h"

/* CBOSLib headers */
#include "MessTrans.h"
#include "OSFile.h"

/* Local headers */
#include "Internal/CBMisc.h"
#include "Saver2.h"
#include "NoBudge.h"
#ifdef SLOW_TEST
#include "Scheduler.h"
#endif
#include "FOpenCount.h"

/* The following structure holds all the state for a given save operation */
typedef struct
{
  LinkedListItem list_item;
  int   last_message_ref;
  int   bytes_sent;
  bool  destination_safe;
  void *RAM_buffer;
  Saver2WriteMethod    *write_method;
  Saver2CompleteMethod *complete_method;
  Saver2FailedMethod   *failed_method;
  void                 *client_handle;
#ifdef SLOW_TEST
  WimpMessage datasaveack_msg;
#endif
  WimpMessage datasave_msg;
}
SaveOpData;

/* Constant numeric values */
enum
{
  DefaultBufferSize = BUFSIZ,
  DataLoadDelay     = 500, /* Artificial delay in centiseconds before
                              replying to DataSaveAck with DataLoad */
  DestinationUnsafe = -1,  /* Estimated size value to indicate unsafe
                              destination */
  PreExpandHeap     = BUFSIZ /* Number of bytes to pre-allocate before
                                disabling flex budging */
};

/* -----------------------------------------------------------------------
                          Internal library data
*/

static bool initialised;
static int  client_task;
static LinkedList save_op_data_list;
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

static bool op_has_ref(LinkedList *const list,
  LinkedListItem *const item, void *const arg)
{
  const int *const msg_ref = arg;
  const SaveOpData * const save_op_data = (SaveOpData *)item;

  assert(msg_ref != NULL);
  assert(save_op_data != NULL);
  NOT_USED(list);

  return save_op_data->last_message_ref == *msg_ref;
}

/* ----------------------------------------------------------------------- */

static SaveOpData *find_record(int msg_ref)
{
  DEBUGF("Saver2: Searching for operation awaiting reply to %d\n", msg_ref);
  if (!msg_ref)
    return NULL;

  SaveOpData *const save_op_data = (SaveOpData *)linkedlist_for_each(
    &save_op_data_list, op_has_ref, &msg_ref);

  if (save_op_data == NULL)
  {
    DEBUGF("Saver2: End of linked list (no match)\n");
  }
  else
  {
    DEBUGF("Saver2: Record %p has matching message ID\n",
      (void *)save_op_data);
  }
  return save_op_data;
}

/* ----------------------------------------------------------------------- */

static void destroy_op(SaveOpData *const save_op_data)
{
  DEBUGF("Saver2: Removing record of save operation %p\n", (void *)save_op_data);
  assert(save_op_data != NULL);

  linkedlist_remove(&save_op_data_list, &save_op_data->list_item);
  if (save_op_data->RAM_buffer)
  {
    flex_free(&save_op_data->RAM_buffer);
  }
  free(save_op_data);
}

/* ----------------------------------------------------------------------- */

static void finished(SaveOpData *const save_op_data,
  const char *const file_path)
{
  assert(save_op_data != NULL);

  if (save_op_data->complete_method != NULL)
  {
    DEBUGF("Saver2: calling complete function with arg %p\n",
          save_op_data->client_handle);

    save_op_data->complete_method(
      save_op_data->datasave_msg.data.data_save.file_type,
      save_op_data->destination_safe ? file_path : NULL,
      save_op_data->datasave_msg.hdr.my_ref,
      save_op_data->client_handle);
  }
}

/* ----------------------------------------------------------------------- */

static void failed(SaveOpData *const save_op_data,
  CONST _kernel_oserror *const e)
{
  assert(save_op_data != NULL);

  if (e != NULL)
  {
    DEBUGF("Saver2: Error 0x%x, %s\n", e->errnum, e->errmess);
  }

  if (save_op_data->failed_method != NULL)
  {
    DEBUGF("Saver2: calling failed function with arg %p\n",
           save_op_data->client_handle);

    save_op_data->failed_method(e, save_op_data->client_handle);
  }
}

/* ----------------------------------------------------------------------- */

static bool write_and_destroy(
  SaveOpData *const save_op_data, Writer *const writer,
  const char *const filename)
{
  assert(save_op_data != NULL);
  assert(writer != NULL);
  assert(!writer_ferror(writer));
  assert(filename != NULL);

  bool success = true;

  if (save_op_data->write_method != NULL)
  {
    success = save_op_data->write_method(writer,
      save_op_data->datasave_msg.data.data_save.file_type,
      filename, save_op_data->client_handle);
  }

  /* Destroying a writer can fail because it flushes buffered output. */
  long int const nbytes = writer_destroy(writer);
  if (!success)
  {
    /* Client should have already reported any error */
    failed(save_op_data, NULL);
  }
  else if (nbytes < 0)
  {
    failed(save_op_data, lookup_error("WriteFail", filename));
    success = false;
  }

  return success;
}

/* ----------------------------------------------------------------------- */

static bool save_file(SaveOpData *const save_op_data,
  const char *const file_path)
{
  FILE *const f = fopen_inc(file_path, "wb");
  if (f == NULL)
  {
    failed(save_op_data, lookup_error("OpenOutFail", file_path));
    return false;
  }

  Writer writer;
  writer_raw_init(&writer, f);
  bool success = write_and_destroy(save_op_data, &writer, file_path);

  if (fclose_dec(f) && success)
  {
    failed(save_op_data, lookup_error("WriteFail", file_path));
    success = false;
  }

  return success;
}

/* ----------------------------------------------------------------------- */

static CONST _kernel_oserror *send_msg(SaveOpData *const save_op_data,
  int const code, WimpMessage *const msg, int const handle, int const icon)
{
  assert(code == Wimp_EUserMessage || code == Wimp_EUserMessageRecorded);
  assert(msg != NULL);
  assert(save_op_data != NULL);

  ON_ERR_RTN_E(wimp_send_message(code, msg, handle, icon, NULL));
  save_op_data->last_message_ref = msg->hdr.my_ref;
  DEBUGF("Saver2: sent message with code %d and ref. %d\n",
         msg->hdr.action_code, msg->hdr.my_ref);

  return NULL;
}

/* ----------------------------------------------------------------------- */

static bool send_dataload(SaveOpData *const save_op_data,
  WimpMessage *const message)
{
  assert(save_op_data != NULL);
  assert(message != NULL);
  assert(message->hdr.action_code == Wimp_MDataSaveAck);

  DEBUGF("Saver2: Acting upon message code %d received for operation %p\n",
        message->hdr.action_code, (void *)save_op_data);

  if (!save_file(save_op_data, message->data.data_save_ack.leaf_name))
  {
    return false;
  }

  CONST _kernel_oserror *e =
    os_file_set_type(message->data.data_save_ack.leaf_name,
                     message->data.data_save_ack.file_type);

  if (e == NULL)
  {
    /* Amend header of DataSaveAck message to change it into a DataLoad */
    message->hdr.your_ref = message->hdr.my_ref;
    message->hdr.action_code = Wimp_MDataLoad;

    /* Send DataLoad to the sender of the DataSaveAck message */
    e = send_msg(save_op_data, Wimp_EUserMessageRecorded,
                 message, message->hdr.sender, 0);
  }

  if (e != NULL)
  {
    failed(save_op_data, e);
    remove(message->data.data_save_ack.leaf_name);
    return false;
  }

  return true;
}

/* ----------------------------------------------------------------------- */

#ifdef SLOW_TEST
static SchedulerTime delayed_dataload(void *const handle,
  SchedulerTime time_now, const volatile bool *const time_up)
{
  SaveOpData *const save_op_data = handle;
  assert(handle != NULL);
  NOT_USED(time_up);

  scheduler_deregister(delayed_dataload, handle);

  if (!send_dataload(save_op_data, &save_op_data->datasaveack_msg))
  {
    destroy_op(save_op_data);
  }

  return time_now;
}
#endif

/* ----------------------------------------------------------------------- */

static bool cancel_matching_op(LinkedList *const list,
  LinkedListItem *const item, void *const arg)
{
  SaveOpData * const save_op_data = (SaveOpData *)item;
  assert(save_op_data != NULL);
  NOT_USED(list);

  /* Check whether this save operation uses the specified handle.
     NULL means cancel all. */
  if (arg == NULL || save_op_data->client_handle == arg)
  {
    failed(save_op_data, NULL);
    destroy_op(save_op_data);
  }
  return false; /* next item */
}

/* ----------------------------------------------------------------------- */

static bool ram_transmit(SaveOpData *const save_op_data,
  WimpMessage *const message)
{
  assert(save_op_data != NULL);
  assert(message != NULL);

  if (save_op_data->RAM_buffer == NULL)
  {
    /* First call to this function fills the buffer. This isn't ideal
       but it avoids concerns about event library reentrancy.
       Use estimated file size as buffer size unless it is implausible. */
    int const buf_size =
      (save_op_data->datasave_msg.data.data_save.estimated_size <= 0 ?
        DefaultBufferSize :
        save_op_data->datasave_msg.data.data_save.estimated_size);

    DEBUGF("Saver2: Allocating RAM transfer buffer of %d bytes\n", buf_size);
    if (!flex_alloc(&save_op_data->RAM_buffer, buf_size))
    {
      failed(save_op_data, no_mem());
      return false;
    }

    Writer writer;
    writer_flex_init(&writer, &save_op_data->RAM_buffer);
    if (!write_and_destroy(save_op_data, &writer,
          save_op_data->datasave_msg.data.data_save.leaf_name))
    {
      return false;
    }
  }

  int event_code = 0, nbytes = 0;
  int const rem =
    flex_size(&save_op_data->RAM_buffer) - save_op_data->bytes_sent;

  if (rem < message->data.ram_fetch.buffer_size)
  {
    /* We can fit all our remaining data in the proffered buffer, hence
       this is the final RAMTransmit message (we don't expect a reply) */
    nbytes = rem;
    event_code = Wimp_EUserMessage;
  }
  else
  {
    /* Will require further RAMFetch/RAMTransmit exchanges to transfer
       all of our client's data to the other task */
    nbytes = message->data.ram_fetch.buffer_size;
    event_code = Wimp_EUserMessageRecorded;
  }

  void *const dbuf = message->data.ram_fetch.buffer;
  nobudge_register(PreExpandHeap); /* dereference of pointer to flex */
  void *const sbuf = (char *)save_op_data->RAM_buffer +
                     save_op_data->bytes_sent;

  DEBUGF("Saver2: transfering %d bytes from address %p in task %d to addr %p"
         " in task %d\n", nbytes, sbuf, client_task, dbuf,
         message->hdr.sender);

  CONST _kernel_oserror *e = wimp_transfer_block(client_task, sbuf,
    message->hdr.sender, dbuf, nbytes);

  nobudge_deregister();

  if (e == NULL)
  {
    message->hdr.your_ref = message->hdr.my_ref;
    message->hdr.action_code = Wimp_MRAMTransmit;
    message->data.ram_transmit.buffer = dbuf;
    message->data.ram_transmit.nbytes = nbytes;

    /* Send RAMTransmit to the sender of the RAMFetch message */
    e = send_msg(save_op_data, event_code, message, message->hdr.sender, 0);
  }

  if (e != NULL)
  {
    failed(save_op_data, e);
    return false;
  }

  if (event_code == Wimp_EUserMessage)
  {
    /* All data has been successfully transferred */
    finished(save_op_data, NULL);
    destroy_op(save_op_data);
  }
  else
  {
    save_op_data->bytes_sent += nbytes;
  }

  return true;
}

/* ----------------------------------------------------------------------- */

static void ramtransmit_bounce(SaveOpData *const save_op_data)
{
  DEBUGF("Saver2: no reply to RAMTransmit\n");
  failed(save_op_data, lookup_error("RecDied", NULL));
  destroy_op(save_op_data);
}

/* ----------------------------------------------------------------------- */

static void datasave_bounce(SaveOpData *const save_op_data)
{
  DEBUGF("Saver2: no reply to DataSave\n");
  failed(save_op_data, NULL);
  destroy_op(save_op_data);
}

/* ----------------------------------------------------------------------- */

static void dataload_bounce(SaveOpData *const save_op_data,
  const char *const file_path)
{
  DEBUGF("Saver2: no reply to DataLoad\n");
  assert(save_op_data != NULL);

  /* Delete the file we saved if the destination was unsafe */
  if (!save_op_data->destination_safe)
  {
    DEBUGF("Saver2: Deleting temporary file '%s'\n", file_path);
    remove(file_path);
  }

  failed(save_op_data, lookup_error("RecDied", NULL));
  destroy_op(save_op_data);
}

/* -----------------------------------------------------------------------
                        Wimp message handlers
*/

static int datasaveack_handler(WimpMessage *const message,
  void *const handle)
{
  assert(message != NULL);
  assert(message->hdr.action_code == Wimp_MDataSaveAck);
  NOT_USED(handle);

  DEBUGF("Saver2: Received a DataSaveAck message (ref. %d in reply to %d)\n",
        message->hdr.my_ref, message->hdr.your_ref);

  SaveOpData *const save_op_data = find_record(message->hdr.your_ref);
  if (save_op_data == NULL)
  {
    DEBUGF("Saver2: Bad your_ref value\n");
    return 0; /* not a reply to our message */
  }

  DEBUGF("Saver2: Request to save data to file '%s' of type &%x\n",
        message->data.data_save_ack.leaf_name,
        message->data.data_save_ack.file_type);

  if (message->data.data_save_ack.estimated_size != DestinationUnsafe)
  {
    save_op_data->destination_safe = true;
  }

  DEBUGF("Saver2: Destination %s safe\n", save_op_data->destination_safe ?
         "is" : "is not");

#ifdef SLOW_TEST
  save_op_data->datasaveack_msg = *message;
  if (scheduler_register_delay(delayed_dataload,
                               save_op_data,
                               DataLoadDelay,
                               SchedulerPriority_Min) != NULL)
#endif /* SLOW_TEST */
  {
    if (!send_dataload(save_op_data, message))
    {
      destroy_op(save_op_data);
    }
  }

  return 1; /* claim message */
}

/* ----------------------------------------------------------------------- */

static int dataloadack_handler(WimpMessage *const message,
  void *const handle)
{
  assert(message != NULL);
  assert(message->hdr.action_code == Wimp_MDataLoadAck);
  NOT_USED(handle);

  DEBUGF("Saver2: Received a DataLoadAck message (ref. %d in reply to %d)\n",
        message->hdr.my_ref, message->hdr.your_ref);

  SaveOpData *const save_op_data = find_record(message->hdr.your_ref);
  if (save_op_data == NULL)
  {
    DEBUGF("Saver2: Bad your_ref value\n");
    return 0; /* not a reply to our message */
  }

  DEBUGF("Saver2: Receiver loaded our file '%s' of type &%x\n",
        message->data.data_load_ack.leaf_name,
        message->data.data_load_ack.file_type);

  finished(save_op_data, message->data.data_load_ack.leaf_name);
  destroy_op(save_op_data);
  return 1; /* claim message */
}

/* ----------------------------------------------------------------------- */

static int ramfetch_handler(WimpMessage *const message,
  void *const handle)
{
  assert(message != NULL);
  assert(message->hdr.action_code == Wimp_MRAMFetch);
  NOT_USED(handle);

  DEBUGF("Saver2: Received a RAMFetch message (ref. %d in reply to %d)\n",
        message->hdr.my_ref, message->hdr.your_ref);

  SaveOpData *const save_op_data = find_record(message->hdr.your_ref);
  if (save_op_data == NULL)
  {
    DEBUGF("Saver2: Unknown your_ref value\n");
    return 0; /* not a reply to our message */
  }

  DEBUGF("Saver2: Request %d bytes be written to buffer at %p\n",
         message->data.ram_fetch.buffer_size, message->data.ram_fetch.buffer);

  if (!ram_transmit(save_op_data, message))
  {
    destroy_op(save_op_data);
  }

  return 1; /* claim message */
}

/* ----------------------------------------------------------------------- */

static const struct
{
  int                 msg_no;
  WimpMessageHandler *handler;
}
msg_handlers[] =
{
  {
    Wimp_MDataSaveAck,
    datasaveack_handler
  },
  {
    Wimp_MDataLoadAck,
    dataloadack_handler
  },
  {
    Wimp_MRAMFetch,
    ramfetch_handler
  }
};

/* -----------------------------------------------------------------------
                        Wimp event handlers
*/

static int msg_bounce_handler(int const event_code,
  WimpPollBlock *const event, IdBlock *const id_block, void *const handle)
{
  assert(event_code == Wimp_EUserMessageAcknowledge);
  NOT_USED(event_code);
  assert(event != NULL);
  NOT_USED(id_block);
  NOT_USED(handle);

  DEBUGF("Saver2: Received a bounced message (ref. %d)\n",
        event->user_message_acknowledge.hdr.my_ref);

  SaveOpData *const save_op_data = find_record(
    event->user_message_acknowledge.hdr.my_ref);

  if (save_op_data == NULL)
  {
    DEBUGF("Saver2: Unknown message ID\n");
    return 0; /* not the last message we sent */
  }

  switch (event->user_message_acknowledge.hdr.action_code)
  {
    case Wimp_MDataLoad:
      dataload_bounce(save_op_data,
        event->user_message_acknowledge.data.data_load.leaf_name);
      return 1; /* claim event */

    case Wimp_MRAMTransmit:
      ramtransmit_bounce(save_op_data);
      return 1; /* claim event */

    case Wimp_MDataSave:
      datasave_bounce(save_op_data);
      return 1; /* claim event */
  }
  return 0; /* pass on event */
}

/* -----------------------------------------------------------------------
                         Public library functions
*/

CONST _kernel_oserror *saver2_initialise(int const task_handle,
  MessagesFD *const mfd)
{
  DEBUGF("Saver2: initialising with task handle 0x%x and messages file "
         "descriptor %p\n", task_handle, (void *)mfd);
  assert(!initialised);

  /* Store client's task handle and a pointer to its messages file descriptor */
  client_task = task_handle;
  desc = mfd;

  linkedlist_init(&save_op_data_list);

  /* Register Wimp message handlers for data transfer protocol */
  for (size_t i = 0; i < ARRAY_SIZE(msg_handlers); i++)
  {
    ON_ERR_RTN_E(event_register_message_handler(msg_handlers[i].msg_no,
                                                msg_handlers[i].handler,
                                                NULL));
  }

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
CONST _kernel_oserror *saver2_finalise(void)
{
  CONST _kernel_oserror *return_error = NULL;

  assert(initialised);
  initialised = false;

  /* Cancel any outstanding save operations */
  DEBUGF("Saver2: Cancelling outstanding operations\n");
  linkedlist_for_each(&save_op_data_list, cancel_matching_op, NULL);

  /* Deregister Wimp message handlers for data transfer protocol */
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
                                          msg_bounce_handler,
                                          NULL));

  return return_error;
}
#endif

/* ----------------------------------------------------------------------- */

CONST _kernel_oserror *saver2_send_data(int const task_handle,
  WimpMessage *const message, Saver2WriteMethod *const write_method,
  Saver2CompleteMethod *const complete_method,
  Saver2FailedMethod *const failed_method, void *const client_handle)
{
  DEBUGF("Saver2: Request to send data to task %d\n", task_handle);
  DEBUGF("Saver2: File type is &%x\n", message->data.data_save.file_type);
  DEBUGF("Saver2: Coordinates are %d,%d (icon %d in window %d)\n",
        message->data.data_save.destination_x,
        message->data.data_save.destination_y,
        message->data.data_save.destination_icon,
        message->data.data_save.destination_window);

  /* Populate a few fields of the DataSave message automatically */
  message->hdr.size = WORD_ALIGN(sizeof(message->hdr) +
                      offsetof(WimpDataSaveMessage, leaf_name) +
                      strlen(message->data.data_save.leaf_name) + 1);
  message->hdr.action_code = Wimp_MDataSave;

  /* Allocate data block for new save operation and link it into the list */
  DEBUGF("Saver2: Creating a record for a new save operation\n");
  SaveOpData *const save_op_data = malloc(sizeof(*save_op_data));
  if (save_op_data == NULL)
  {
    return no_mem();
  }

  /* Initialise record for a new save operation */
  *save_op_data = (SaveOpData){
    .datasave_msg = *message,
    .destination_safe = false,
    .RAM_buffer = NULL,
    .bytes_sent = 0,
    .write_method = write_method,
    .complete_method = complete_method,
    .failed_method = failed_method,
    .client_handle = client_handle,
  };

  /* Add new record to head of linked list */
  linkedlist_insert(&save_op_data_list, NULL, &save_op_data->list_item);
  DEBUGF("Saver2: New record is at %p\n", (void *)save_op_data);

  /* Send DataSave message to task handle, or if none specified then send
     it to the window handle instead (recorded delivery) */
  CONST _kernel_oserror *const err = send_msg(save_op_data,
    Wimp_EUserMessageRecorded, message,
    task_handle ? task_handle : message->data.data_save.destination_window,
    task_handle ? 0 : message->data.data_save.destination_icon);

  if (err != NULL)
  {
    destroy_op(save_op_data);
  }

  return err;
}

/* ----------------------------------------------------------------------- */

void saver2_cancel_sends(void *const client_handle)
{
  /* Cancel any outstanding save operations using the specified flex anchor.
     Use when the source data has become invalid. */
  DEBUGF("Saver2: Cancelling all send operations on %p\n", client_handle);
  assert(client_handle != NULL);
  linkedlist_for_each(&save_op_data_list, cancel_matching_op,
                      client_handle);
}
