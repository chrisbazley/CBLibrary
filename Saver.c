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

/* History:
  CJB: 09-Aug-06: Created this source file from scratch.
  CJB: 13-Sep-06: First release version.
  CJB: 25-Sep-06: Added support for global clipboard.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 17-Oct-06: Too many changes to list. Broadly speaking, the code
                  to handle drag and drop has been moved to c.Drag and the
                  code to handle the global clipboard has moved to c.Entity.
  CJB: 14-Apr-07: Modified saver_finalise() to soldier on if an error occurs,
                  using the new MERGE_ERR macro.
  CJB: 27-Jan-08: Reduced the number of arguments to saver_send_data by
                  passing a pointer to a WimpMessage structure instead of
                  individually specifying the file type, leaf name, drag
                  destination and message ID to reply to.
                  Fixed a memory leak in _svr_datasaveack_msg_handler, when
                  when compiled with SLOW_TEST defined; the SaveOpData was
                  never destroyed if scheduler_register_delay failed.
  CJB: 22-Jun-09: Use variable name rather than type with 'sizeof' operator,
                  removed unnecessary casts from 'void *' and tweaked spacing.
                  Corrected some erroneous uses of field specifier '%d' instead
                  of '%u' for unsigned values.
  CJB: 09-Sep-09: Stop using reserved identifier '_SaveOpData' (starts with an
                  underscore followed by a capital letter).
                  Now iterates over an array of Wimp message numbers and
                  function pointers when registering or deregistering message
                  handlers (reduces code size and ensures symmetry).
  CJB: 14-Oct-09: Titivated formatting and replaced 'magic' values with named
                  constants. Removed dependency on MsgTrans by storing a
                  pointer to a messages file descriptor upon initialisation.
                  Additional assertions. Use 'for' loops in preference to
                  'while' loops.
  CJB: 05-May-12: Added support for stress-testing failure of _kernel_osfile.
  CJB: 17-Dec-14: Updated to use the generic linked list implementation.
  CJB: 23-Dec-14: Apply Fortify to Event & Wimp library function calls.
  CJB: 26-Dec-14: Modified to use fwrite instead of _kernel_osfile when not
                  compiled with USE_FILEPERC defined.
  CJB: 01-Jan-15: Apply Fortify to standard library I/O function calls.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 09-Apr-16: Modified assertions to avoid GNU C compiler warnings about
                  unsigned comparisons with signed integers.
  CJB: 18-Apr-16: Cast pointer parameters to void * to match %p. No longer
                  prints function pointers (no matching format specifier).
                  Used size_t for loop counters to match type of ARRAY_SIZE.
  CJB: 01-Nov-18: Replaced DEBUG macro usage with DEBUGF.
  CJB: 28-Sep-19: Flex budge is now restored immediately upon return from
                  wimp_transfer_block.
  CJB: 06-Nov-19: Fixed failure to check the return value of fclose_dec()
                  and called _kernel_last_oserror() to reset the error
                  trap before writing to file in _svr_save_as_file().
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

/* CBUtilLib headers */
#include "StrExtra.h"
#include "LinkedList.h"

/* CBOSLib headers */
#include "MessTrans.h"
#include "Hourglass.h"
#include "WimpExtra.h"

/* Local headers */
#include "Internal/CBMisc.h"
#include "Saver.h"
#include "NoBudge.h"
#include "FileUtils.h"
#ifdef USE_FILEPERC
#include "FilePerc.h"
#endif
#ifdef SLOW_TEST
#include "Scheduler.h"
#endif
#include "FOpenCount.h"

typedef struct
{
  SaverFinishedHandler *funct; /* may be NULL */
  void                 *arg;
}
SaveOpCallback;

/* The following structure holds all the state for a given save operation */
typedef struct
{
  LinkedListItem        list_item;
  int                   last_message_ref;
  int                   datasave_msg_ref;
  bool                  destination_safe;
  flex_ptr              client_data;
  unsigned int          start_offset;
  unsigned int          end_offset;
  SaverFileHandler     *saver_funct;
  SaveOpCallback        callback;
#ifdef SLOW_TEST
  WimpMessage           delayed_message;
#endif
}
SaveOpData;

/* Constant numeric values */
enum
{
  DataLoadDelay               = 500, /* Artificial delay in centiseconds before
                                        replying to DataSaveAck with DataLoad */
  DestinationUnsafe           = -1,  /* Estimated size value to indicate unsafe
                                        destination */
  PreExpandHeap               = BUFSIZ /* Number of bytes to pre-allocate before
                                          disabling flex budging */
};

/* -----------------------------------------------------------------------
                        Internal function prototypes
*/

static WimpMessageHandler _svr_datasaveack_msg_handler, _svr_dataloadack_msg_handler, _svr_ramfetch_msg_handler;
static WimpEventHandler _svr_msg_bounce_handler;
static void _svr_finished(SaveOpData *save_op_data, bool success, CONST _kernel_oserror *e, const char *file_path);
static SaveOpData *_svr_find_record(int msg_ref);
static void _svr_destroy_op(SaveOpData *save_op_data);
#ifdef SLOW_TEST
static SchedulerIdleFunction _svr_send_dataload;
#endif
static void _svr_save_as_file(SaveOpData *save_op_data, WimpMessage *message);
static CONST _kernel_oserror *lookup_error(const char *token, const char *param);
static LinkedListCallbackFn _svr_cancel_matching_op, _svr_op_has_ref;

/* -----------------------------------------------------------------------
                          Internal library data
*/

static bool initialised;
static int  client_task;
static LinkedList save_op_data_list;
static const struct
{
  int                 msg_no;
  WimpMessageHandler *handler;
}
msg_handlers[] =
{
  {
    Wimp_MDataSaveAck,
    _svr_datasaveack_msg_handler
  },
  {
    Wimp_MDataLoadAck,
    _svr_dataloadack_msg_handler
  },
  {
    Wimp_MRAMFetch,
    _svr_ramfetch_msg_handler
  }
};
static MessagesFD *desc;

/* -----------------------------------------------------------------------
                         Public library functions
*/

CONST _kernel_oserror *saver_initialise(int task_handle, MessagesFD *mfd)
{
  unsigned int mask;

  DEBUGF("Saver: initialising with task handle 0x%x and messages file "
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
                                           _svr_msg_bounce_handler,
                                           NULL));

  /* Ensure that messages are not masked */
  event_get_mask(&mask);
  CLEAR_BITS(mask, Wimp_Poll_UserMessageMask |
                   Wimp_Poll_UserMessageRecordedMask |
                   Wimp_Poll_UserMessageAcknowledgeMask);
  event_set_mask(mask);

#ifdef USE_FILEPERC
  /* Ensure that subsidiary modules have also been initialised */
  ON_ERR_RTN_E(file_perc_initialise(mfd));
#endif /* USE_FILEPERC */

  initialised = true;

  return NULL; /* success */
}

/* ----------------------------------------------------------------------- */

#ifdef INCLUDE_FINALISATION_CODE
CONST _kernel_oserror *saver_finalise(void)
{
  CONST _kernel_oserror *return_error = NULL;

  assert(initialised);
  initialised = false;

  /* Cancel any outstanding save operations */
  DEBUGF("Saver: Cancelling outstanding operations\n");
  linkedlist_for_each(&save_op_data_list, _svr_cancel_matching_op, NULL);

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
                                          _svr_msg_bounce_handler,
                                          NULL));

  return return_error;
}
#endif

/* ----------------------------------------------------------------------- */

CONST _kernel_oserror *saver_send_data(int task_handle, WimpMessage *message, flex_ptr data, unsigned int start_offset, unsigned int end_offset, SaverFileHandler *save_method, SaverFinishedHandler *finished_method, void *client_handle)
{
  SaveOpData *save_op_data;

  DEBUGF("Saver: Request to send bytes %u-%u of block anchored at %p (%p) "
        "to task %d\n", start_offset, end_offset, (void *)data, *data, task_handle);

  DEBUGF("Saver: File type is &%x\n", message->data.data_save.file_type);

  DEBUGF("Saver: Coordinates are %d,%d (icon %d in window %d)\n",
        message->data.data_save.destination_x,
        message->data.data_save.destination_y,
        message->data.data_save.destination_icon,
        message->data.data_save.destination_window);

  assert(start_offset <= end_offset);
  assert(data != NULL && *data != NULL);
  assert(flex_size(data) >= 0);
  assert(end_offset <= (unsigned int)flex_size(data));

  /* Allocate data block for new save operation and link it into the list */
  DEBUGF("Saver: Creating a record for a new save operation\n");
  save_op_data = malloc(sizeof(*save_op_data));
  if (save_op_data == NULL)
    return lookup_error("NoMem", NULL); /* Memory couldn't be claimed */

  /* Initialise record for a new save operation */
  save_op_data->destination_safe = false; /* not yet known */
  save_op_data->client_data = data;
  save_op_data->start_offset = start_offset;
  save_op_data->end_offset = end_offset;
  save_op_data->callback.funct = finished_method; /* may be NULL */
  save_op_data->saver_funct = save_method; /* should be NULL */
  save_op_data->callback.arg = client_handle;

  /* Add new record to head of linked list */
  linkedlist_insert(&save_op_data_list, NULL, &save_op_data->list_item);
  DEBUGF("Saver: New record is at %p\n", (void *)save_op_data);

  {
    _kernel_oserror *e;

    /* Populate a few fields of the DataSave message automatically */
    message->hdr.size = WORD_ALIGN(sizeof(message->hdr) +
                        offsetof(WimpDataSaveMessage, leaf_name) +
                        strlen(message->data.data_save.leaf_name) + 1);
    message->hdr.action_code = Wimp_MDataSave;
    message->data.data_save.estimated_size = end_offset - start_offset;

    /* Send DataSave message to task handle, or if none specified then send
       it to the window handle instead (recorded delivery) */
    e = wimp_send_message(Wimp_EUserMessageRecorded,
                          message,
                          task_handle ? task_handle :
                          message->data.data_save.destination_window,
                          task_handle ? 0 :
                          message->data.data_save.destination_icon,
                          NULL);
    if (e != NULL)
    {
      _svr_destroy_op(save_op_data);
      return e;
    }
    DEBUGF("Saver: sent DataSave message (ref. %d in reply to %d)\n",
          message->hdr.my_ref, message->hdr.your_ref);

    /* Record ID of message just sent and free buffer */
    save_op_data->last_message_ref = message->hdr.my_ref;
    save_op_data->datasave_msg_ref = message->hdr.my_ref;
  }

  return NULL; /* success */
}

/* ----------------------------------------------------------------------- */

void saver_cancel_sends(flex_ptr data)
{
  /* Cancel any outstanding save operations using the specified flex anchor.
     Use when the source data has become invalid. */
  DEBUGF("Saver: Cancelling all send operations on block anchored at %p (%p)\n",
        (void *)data, *data);
  linkedlist_for_each(&save_op_data_list, _svr_cancel_matching_op, data);
}

/* -----------------------------------------------------------------------
                        Wimp message handlers
*/

static int _svr_datasaveack_msg_handler(WimpMessage *message, void *handle)
{
  /* This is a handler for DataSaveAck messages */
  SaveOpData *save_op_data;

  assert(message != NULL);
  NOT_USED(handle);

  DEBUGF("Saver: Received a DataSaveAck message (ref. %d in reply to %d)\n",
        message->hdr.my_ref, message->hdr.your_ref);

  if (message->hdr.your_ref == 0 ||
      (save_op_data = _svr_find_record(message->hdr.your_ref)) == NULL)
  {
    DEBUGF("Saver: Bad your_ref value\n");
    return 0; /* not a reply to our message */
  }

  DEBUGF("Saver: Request to save data to file '%s' of type &%x\n",
        message->data.data_save_ack.leaf_name,
        message->data.data_save_ack.file_type);

  save_op_data->destination_safe = (message->data.data_save_ack.estimated_size
                                   != DestinationUnsafe);
  DEBUGF("Saver: Destination %s safe\n", save_op_data->destination_safe ? "is" :
        "is not");

  assert(save_op_data->client_data != NULL);

#ifdef SLOW_TEST
  save_op_data->delayed_message = *message;
  if (scheduler_register_delay(_svr_send_dataload,
                               save_op_data,
                               DataLoadDelay,
                               SchedulerPriority_Min) != NULL)
#endif /* SLOW_TEST */
    _svr_save_as_file(save_op_data, message);

  return 1; /* claim message */
}

/* ----------------------------------------------------------------------- */

static int _svr_dataloadack_msg_handler(WimpMessage *message, void *handle)
{
  /* This is a handler for DataLoadAck messages */
  SaveOpData *save_op_data;

  assert(message != NULL);
  NOT_USED(handle);

  DEBUGF("Saver: Received a DataLoadAck message (ref. %d in reply to %d)\n",
        message->hdr.my_ref, message->hdr.your_ref);

  if (message->hdr.your_ref == 0 ||
      (save_op_data = _svr_find_record(message->hdr.your_ref)) == NULL)
  {
    DEBUGF("Saver: Bad your_ref value\n");
    return 0; /* not a reply to our message */
  }

  DEBUGF("Saver: Receiver loaded our file '%s' of type &%x\n",
        message->data.data_load_ack.leaf_name,
        message->data.data_load_ack.file_type);

  _svr_finished(save_op_data,
                true,
                NULL,
                save_op_data->destination_safe ?
                  message->data.data_load_ack.leaf_name : NULL);

  return 1; /* claim message */
}

/* ----------------------------------------------------------------------- */

static int _svr_ramfetch_msg_handler(WimpMessage *message, void *handle)
{
  /* This is a handler for RAMFetch messages */
  int transfer_size, bytes_remaining, event_code;
  SaveOpData *save_op_data;
  _kernel_oserror *err;

  assert(message != NULL);
  NOT_USED(handle);

  DEBUGF("Saver: Received a RAMFetch message (ref. %d in reply to %d)\n",
        message->hdr.my_ref, message->hdr.your_ref);

  if (message->hdr.your_ref == 0 ||
      (save_op_data = _svr_find_record(message->hdr.your_ref)) == NULL)
  {
    DEBUGF("Saver: Unknown your_ref value\n");
    return 0; /* not a reply to our message */
  }

  DEBUGF("Saver: Request %d bytes be written to buffer at %p\n",
        message->data.ram_fetch.buffer_size, message->data.ram_fetch.buffer);

  /* We can't use RAM transfer if the client has supplied a save function */
  if (save_op_data->saver_funct != NULL)
  {
    DEBUGF("Saver: Awaiting DataSaveAck (client save function)\n");
    return 0; /* ignore message */
  }
#ifdef SLOW_TEST
  DEBUGF("Saver: Pretending we don't support RAM transfer\n");
  return 0; /* pass on message */
#endif

  bytes_remaining = save_op_data->end_offset - save_op_data->start_offset;
  if (bytes_remaining < message->data.ram_fetch.buffer_size)
  {
    /* We can fit all our remaining data in the proffered buffer, hence
       this is the final RAMTransmit message (we don't expect a reply) */
    transfer_size = bytes_remaining;
    event_code = Wimp_EUserMessage;
  }
  else
  {
    /* Will require further RAMFetch/RAMTransmit exchanges to transfer
       all of our client's data to the other task */
    transfer_size = message->data.ram_fetch.buffer_size;
    event_code = Wimp_EUserMessageRecorded;
  }

  assert(save_op_data->client_data != NULL);
  nobudge_register(PreExpandHeap); /* protect dereference of pointer to flex block */
  DEBUGF("Saver: transfering %d bytes from address %p in task %d to address %p"
         " in task %d\n", transfer_size, (char *)*save_op_data->client_data +
         save_op_data->start_offset, client_task,
         message->data.ram_fetch.buffer, message->hdr.sender);

  err = wimp_transfer_block(client_task,
                            (char *)*save_op_data->client_data +
                                    save_op_data->start_offset,
                            message->hdr.sender,
                            message->data.ram_fetch.buffer,
                            transfer_size);
  nobudge_deregister();

  if (err != NULL)
  {
    _svr_finished(save_op_data, false, err, NULL);
    return 1; /* claim message */
  }
  save_op_data->start_offset += transfer_size;

  message->hdr.your_ref = message->hdr.my_ref;
  message->hdr.action_code = Wimp_MRAMTransmit;
  message->data.ram_transmit.buffer = message->data.ram_fetch.buffer;
  message->data.ram_transmit.nbytes = transfer_size;

  err = wimp_send_message(event_code, message, message->hdr.sender, 0, NULL);
  if (err != NULL)
  {
    _svr_finished(save_op_data, false, err, NULL);
    return 1; /* claim message */
  }
  DEBUGF("Saver: Sent RAMTransmit message (ref. %d in reply to %d)\n",
        message->hdr.my_ref, message->hdr.your_ref);
  if (event_code == Wimp_EUserMessageRecorded)
  {
    /* Record ID of message just sent to match against next incoming RAMFetch */
    save_op_data->last_message_ref = message->hdr.my_ref;
  }
  else
  {
    /* All data has been successfully transferred */
    _svr_finished(save_op_data, true, NULL, NULL);
  }

  return 1; /* claim message */
}

/* -----------------------------------------------------------------------
                        Wimp event handlers
*/

static int _svr_msg_bounce_handler(int event_code, WimpPollBlock *event, IdBlock *id_block, void *handle)
{
  /* This is a handler for bounced messages */
  SaveOpData *save_op_data;

  NOT_USED(event_code);
  assert(event != NULL);
  NOT_USED(id_block);
  NOT_USED(handle);

  DEBUGF("Saver: Received a bounced message (ref. %d)\n",
        event->user_message_acknowledge.hdr.my_ref);

  save_op_data = _svr_find_record(event->user_message_acknowledge.hdr.my_ref);
  if (save_op_data == NULL)
  {
    DEBUGF("Saver: Unknown message ID\n");
    return 0; /* not the last message we sent */
  }

  switch (event->user_message_acknowledge.hdr.action_code)
  {
    case Wimp_MDataLoad:
      DEBUGF("Saver: It is a bounced DataLoad message\n");
      /* Delete the file we saved if the destination was unsafe */
      if (!save_op_data->destination_safe)
      {
        DEBUGF("Saver: Deleting temporary file '%s'\n",
              event->user_message_acknowledge.data.data_load.leaf_name);
        remove(event->user_message_acknowledge.data.data_load.leaf_name);
      }
      _svr_finished(save_op_data, false, lookup_error("RecDied", NULL), NULL);
      return 1; /* claim event */

    case Wimp_MRAMTransmit:
      DEBUGF("Saver: It is a bounced RAMTransmit message\n");
      _svr_finished(save_op_data, false, lookup_error("RecDied", NULL), NULL);
      return 1; /* claim event */

    case Wimp_MDataSave:
      DEBUGF("Saver: It is a bounced DataSave message\n");
      _svr_finished(save_op_data, false, NULL, NULL);
      return 1; /* claim event */

  }
  return 0; /* pass on event */
}

/* -----------------------------------------------------------------------
                         Miscellaneous internal functions
*/

static CONST _kernel_oserror *lookup_error(const char *token, const char *param)
{
  /* Look up error message from the token, outputting to an internal buffer */
  return messagetrans_error_lookup(desc, DUMMY_ERRNO, token, 1, param);
}

/* ----------------------------------------------------------------------- */

static void _svr_finished(SaveOpData *save_op_data, bool success, CONST _kernel_oserror *e, const char *file_path)
{
  /* Call the client-supplied function (if any) to notify it that the save
     operation is complete. */
  assert(save_op_data != NULL);
  assert(!success || e == NULL);

  if (save_op_data->callback.funct != NULL)
  {
    DEBUGF("Saver: Save finished, calling client function with arg %p\n",
          save_op_data->callback.arg);

    save_op_data->callback.funct(success,
                                 e,
                                 file_path,
                                 save_op_data->datasave_msg_ref,
                                 save_op_data->callback.arg);
  }

  /* Free data block for a save operation and de-link it from the list */
  _svr_destroy_op(save_op_data);
}

/* ----------------------------------------------------------------------- */

static SaveOpData *_svr_find_record(int msg_ref)
{
  SaveOpData *save_op_data;

  DEBUGF("Saver: Searching for operation awaiting reply to %d\n", msg_ref);
  save_op_data = (SaveOpData *)linkedlist_for_each(
                 &save_op_data_list, _svr_op_has_ref, &msg_ref);

  if (save_op_data == NULL)
  {
    DEBUGF("Saver: End of linked list (no match)\n");
  }
  else
  {
    DEBUGF("Saver: Record %p has matching message ID\n", (void *)save_op_data);
  }
  return save_op_data;
}

/* ----------------------------------------------------------------------- */

static void _svr_destroy_op(SaveOpData *save_op_data)
{
  DEBUGF("Saver: Removing record of save operation %p\n", (void *)save_op_data);
  assert(save_op_data != NULL);

  linkedlist_remove(&save_op_data_list, &save_op_data->list_item);
  free(save_op_data);
}

/* ----------------------------------------------------------------------- */

#ifdef SLOW_TEST
static SchedulerTime _svr_send_dataload(void *handle, SchedulerTime time_now, const volatile bool *time_up)
{
  SaveOpData *save_op_data = handle;

  assert(handle != NULL);
  NOT_USED(time_up);

  _svr_save_as_file(save_op_data, &save_op_data->delayed_message);

  scheduler_deregister(_svr_send_dataload, handle);
  return time_now;
}
#endif

/* ----------------------------------------------------------------------- */

static void _svr_save_as_file(SaveOpData *save_op_data, WimpMessage *message)
{
  CONST _kernel_oserror *err;

  assert(save_op_data != NULL);
  assert(message != NULL);
  assert(message->hdr.action_code == Wimp_MDataSaveAck);

  DEBUGF("Saver: Acting upon message code %d received for operation %p\n",
        message->hdr.action_code, (void *)save_op_data);

  hourglass_on();
  if (save_op_data->saver_funct != NULL)
  {
    /* Use a client-supplied function to save the data */
    err = save_op_data->saver_funct(message->data.data_save_ack.leaf_name,
                                    save_op_data->client_data,
                                    save_op_data->start_offset,
                                    save_op_data->end_offset);
  }
  else
  {
    /* Use standard method to save the data */
#ifdef USE_FILEPERC
    err = file_perc_save(FilePercOp_Save,
                         message->data.data_save_ack.leaf_name,
                         message->data.data_save_ack.file_type,
                         save_op_data->client_data,
                         save_op_data->start_offset,
                         save_op_data->end_offset);
#else
    _kernel_last_oserror(); /* reset */

    size_t n = 0;
    FILE *f = fopen_inc(message->data.data_save_ack.leaf_name, "wb");
    if (f != NULL)
    {
      nobudge_register(PreExpandHeap); /* protect dereference of flex pointer */

      n = fwrite((char *)*save_op_data->client_data + save_op_data->start_offset,
                 save_op_data->end_offset - save_op_data->start_offset,
                 1,
                 f);

      nobudge_deregister();

      if (fclose_dec(f))
      {
        n = 0;
      }
    }

    if (n != 1)
    {
      err = _kernel_last_oserror(); /* return any OS error */
      if (err == NULL)
        err = lookup_error(f == NULL ? "OpenOutFail" : "WriteFail",
                           message->data.data_save_ack.leaf_name);
    }
    else
    {
      err = set_file_type(message->data.data_save_ack.leaf_name,
                          message->data.data_save_ack.file_type);
    }
#endif
  }
  hourglass_off();
  if (err != NULL)
  {
    _svr_finished(save_op_data, false, err, NULL);
    return;
  }

  /* Amend header of DataSaveAck message to change it into a DataLoad */
  message->hdr.your_ref = message->hdr.my_ref;
  message->hdr.action_code = Wimp_MDataLoad;

  /* Send our reply to the sender of the DataSaveAck message
     (recorded delivery) */
  err = wimp_send_message(Wimp_EUserMessageRecorded,
                          message,
                          message->hdr.sender,
                          0,
                          NULL);
  if (err != NULL)
  {
    _svr_finished(save_op_data, false, err, NULL);
    return;
  }
  DEBUGF("Saver: Sent DataLoad message (ref. %d in reply to %d)\n",
        message->hdr.my_ref, message->hdr.your_ref);

  /* Record ID of message just sent */
  save_op_data->last_message_ref = message->hdr.my_ref;
}


/* ----------------------------------------------------------------------- */

static bool _svr_cancel_matching_op(LinkedList *list, LinkedListItem *item, void *arg)
{
  SaveOpData * const save_op_data = (SaveOpData *)item;
  const flex_ptr data = arg;

  assert(save_op_data != NULL);
  NOT_USED(list);

  /* Check whether this save operation uses the specified flex anchor.
     NULL means cancel all. */
  if (data == NULL || save_op_data->client_data == data)
    _svr_finished(save_op_data, false, NULL, NULL);

  return false; /* next item */
}

/* ----------------------------------------------------------------------- */

static bool _svr_op_has_ref(LinkedList *list, LinkedListItem *item, void *arg)
{
  const int * const msg_ref = arg;
  const SaveOpData * const save_op_data = (SaveOpData *)item;

  assert(msg_ref != NULL);
  assert(save_op_data != NULL);
  NOT_USED(list);

  return (save_op_data->last_message_ref == *msg_ref);
}
