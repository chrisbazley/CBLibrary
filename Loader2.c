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

/* History:
  CJB: 12-Oct-06: Created this source file by stripping down c.Loader.
  CJB: 24-Oct-06: First public release version.
  CJB: 18-Nov-06: Reduced the size of the heap pre-expansion done before
                  disabling flex budging from 1024 to 512 bytes.
  CJB: 25-Nov-06: Removed overzealous assertion that only a flex_ptr to NULL
                  was accepted by loader2_buffer_file().
  CJB: 14-Apr-07: Modified loader2_finalise() to soldier on if an error
                  occurs, using the new MERGE_ERR macro.
  CJB: 27-Jan-08: Fixed a bug where _ldr2_decapitate_list tried to
                  'flex_free' the RAM fetch buffer instead of a 'flex_ptr' to
                  its anchor. Caused the Flex library to blow up if
                  loader2_receive_data allocated this buffer and then failed
                  to create or send the RAMFetch message.
  CJB: 22-Jun-09: Use variable name rather than type with 'sizeof' operator,
                  removed unnecessary casts from 'void *' and tweaked spacing.
  CJB: 08-Sep-09: Stop using reserved identifier '_LoadOpData' (starts with an
                  underscore followed by a capital letter).
  CJB: 14-Oct-09: Titivated formatting, replaced 'magic' values with named
                  constants and macro values with 'enum'. Removed dependency on
                  MsgTrans by storing a pointer to a messages file descriptor
                  upon initialisation. _ldr2_replyto_datasave now always
                  outputs the sent message ID. Additional assertions. Use 'for'
                  loops in preference to 'while' loops.
  CJB: 05-May-12: Added support for stress-testing failure of _kernel_osfile.
  CJB: 17-Dec-14: Updated to use the generic linked list implementation and
                  os_file_read_cat_no_path instead of _kernel_osfile.
                  Made the arguments to loader2_initialise conditional upon
                  CBLIB_OBSOLETE.
  CJB: 23-Dec-14: Apply Fortify to Event & Wimp library function calls.
  CJB: 31-Dec-14: A DataLoad message should never be sent in reply to a
                  RAMFetch; the action code of the last message sent is now
                  stored with its reference no. to guard against that
                  eventuality. (Previously, the buffer allocated for incoming
                  data was leaked.)
  CJB: 01-Jan-15: Apply Fortify to standard library I/O function calls.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 18-Apr-16: Cast pointer parameters to void * to match %p. No longer
                  prints function pointers (no matching format specifier).
  CJB: 01-Nov-18: Replaced DEBUG macro usage with DEBUGF.
  CJB: 06-Nov-19: Call _kernel_last_oserror() to reset the error trap before
                  reading from file.
  CJB: 10-Nov-19: Allocate RAM transfer buffers one byte longer than requested
                  to try to avoid having to send a second RAMFetch message.
                  Modified loader2_buffer_file() to use get_file_size().
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

/* CBUtilLib headers */
#include "LinkedList.h"

/* CBOSLib headers */
#include "MessTrans.h"
#include "Hourglass.h"

/* Local headers */
#include "Internal/CBMisc.h"
#include "Loader2.h"
#include "NoBudge.h"
#include "scheduler.h"
#ifdef USE_FILEPERC
#include "FilePerc.h"
#else
#include "FileUtils.h"
#endif
#include "FOpenCount.h"
#ifdef CBLIB_OBSOLETE
#include "msgtrans.h"
#endif /* CBLIB_OBSOLETE */

typedef struct
{
  Loader2FinishedHandler *funct; /* may be NULL */
  void                   *arg;
}
LoadOpCallback;

/* The following structure holds all the state for a given load operation */
typedef struct
{
  LinkedListItem          list_item;
  int                     last_message_ref;
  int                     last_message_type;
  bool                    RAM_capable;
  bool                    idle_function;
  bool                    no_flex_budge;
  int                     file_type;
  WimpRAMFetchMessage     ram_fetch; /* includes flex anchor */
  WimpMessage            *datasave_msg; /* heap block */
  Loader2FileHandler     *loader_funct;
  LoadOpCallback          callback;
}
LoadOpData;

/* Constant numeric values */
enum
{
  DefaultBufferSize = 256,
  BufferExtend      = 256,
  DataLoadWaitTime  = 3000, /* Centiseconds to wait for a DataLoad
                               in reply to our DataSaveAck */
  PreExpandHeap     = 512 /* Number of bytes to pre-allocate
                             before disabling flex budging */
};

/* -----------------------------------------------------------------------
                        Internal function prototypes
*/

static CONST _kernel_oserror *_ldr2_replyto_datasave(const WimpMessage *reply_to, LoadOpData *load_op_data);
static WimpMessageHandler _ldr2_dataload_msg_handler,
                          _ldr2_ramtransmit_msg_handler;
static WimpEventHandler _ldr2_msg_bounce_handler;
static void _ldr2_finished(LoadOpData *load_op_data, bool success, CONST _kernel_oserror *e);
static LoadOpData *_ldr2_find_record(int msg_ref);
static void _ldr2_destroy_op(LoadOpData *load_op_data);
static SchedulerIdleFunction _ldr2_time_out;
static CONST _kernel_oserror *lookup_error(const char *token, const char *param);
static LinkedListCallbackFn _ldr2_cancel_matching_op, _ldr2_op_has_ref;
static void _ldr2_retain_my_ref(LoadOpData *load_op_data, const WimpMessage *msg);

/* -----------------------------------------------------------------------
                          Internal library data
*/

static bool initialised;
static LinkedList load_op_data_list;
static MessagesFD *desc;

/* -----------------------------------------------------------------------
                         Public library functions
*/

#ifdef CBLIB_OBSOLETE
CONST _kernel_oserror *loader2_initialise(void)
#else
CONST _kernel_oserror *loader2_initialise(MessagesFD *mfd)
#endif
{
  unsigned int mask;

  assert(!initialised);

  /* Store pointer to messages file descriptor */
#ifdef CBLIB_OBSOLETE
  desc = msgs_get_descriptor();
#else
  DEBUGF("Loader2: initialising with messages file descriptor %p\n", (void *)mfd);
  desc = mfd;
#endif

  /* Initialise linked list */
  linkedlist_init(&load_op_data_list);

  /* Register Wimp message handlers for data transfer protocol */

  ON_ERR_RTN_E(event_register_message_handler(Wimp_MDataLoad,
                                              _ldr2_dataload_msg_handler,
                                              NULL));

  ON_ERR_RTN_E(event_register_message_handler(Wimp_MRAMTransmit,
                                              _ldr2_ramtransmit_msg_handler,
                                              NULL));

  /* Register handler for messages that return to us as wimp event 19 */
  ON_ERR_RTN_E(event_register_wimp_handler(-1,
                                           Wimp_EUserMessageAcknowledge,
                                           _ldr2_msg_bounce_handler,
                                           NULL));

  /* Ensure that messages are not masked */
  event_get_mask(&mask);
  CLEAR_BITS(mask, Wimp_Poll_UserMessageMask |
                   Wimp_Poll_UserMessageRecordedMask |
                   Wimp_Poll_UserMessageAcknowledgeMask);
  event_set_mask(mask);

#ifdef USE_FILEPERC
  /* Ensure that subsidiary modules have also been initialised */
  ON_ERR_RTN_E(file_perc_initialise(desc));
#endif /* USE_FILEPERC */

  initialised = true;

  return NULL; /* success */
}

/* ----------------------------------------------------------------------- */

#ifdef INCLUDE_FINALISATION_CODE
CONST _kernel_oserror *loader2_finalise(void)
{
  CONST _kernel_oserror *return_error = NULL;

  assert(initialised);
  initialised = false;

  /* Cancel any outstanding save operations */
  DEBUGF("Loader2: Cancelling outstanding operations\n");
  linkedlist_for_each(&load_op_data_list, _ldr2_cancel_matching_op, NULL);

  /* Deregister Wimp message handlers for data transfer protocol */

  MERGE_ERR(return_error,
            event_deregister_message_handler(Wimp_MDataLoad,
                                             _ldr2_dataload_msg_handler,
                                             NULL));

  MERGE_ERR(return_error,
            event_deregister_message_handler(Wimp_MRAMTransmit,
                                             _ldr2_ramtransmit_msg_handler,
                                             NULL));

  /* Deregister handler for messages that return to us as wimp event 19 */
  MERGE_ERR(return_error,
            event_deregister_wimp_handler(-1,
                                          Wimp_EUserMessageAcknowledge,
                                          _ldr2_msg_bounce_handler,
                                          NULL));

  return return_error;
}
#endif

/* ----------------------------------------------------------------------- */

CONST _kernel_oserror *loader2_receive_data(const WimpMessage *message, Loader2FileHandler *load_method, Loader2FinishedHandler *finished_method, void *client_handle)
{
  LoadOpData *load_op_data;
  CONST _kernel_oserror *e = NULL;

  assert(initialised);
  assert(message != NULL);
  assert(message->hdr.action_code == Wimp_MDataSave);

  DEBUGF("Loader2: Processing a DataSave message (ref. %d in reply to %d)\n",
        message->hdr.my_ref, message->hdr.your_ref);

  DEBUGF("Loader2: To icon %d in window %d at coordinates %d,%d\n",
        message->data.data_save.destination_icon,
        message->data.data_save.destination_window,
        message->data.data_save.destination_x,
        message->data.data_save.destination_y);

  DEBUGF("Loader2: File type is &%x, name is '%s' and est. size is %d bytes\n",
        message->data.data_save.file_type, message->data.data_save.leaf_name,
        message->data.data_save.estimated_size);

  /* Allocate memory for a new load operation */
  DEBUGF("Loader2: Creating a record for a new load operation\n");
  load_op_data = malloc(sizeof(*load_op_data));
  if (load_op_data == NULL)
    return lookup_error("NoMem", NULL);

  /* Initialise record for a new load operation */
  load_op_data->RAM_capable = false;
  load_op_data->idle_function = false;
  load_op_data->no_flex_budge = false;
  /* According to the RISC OS 3 PRM a file type value of &ffffffff in a
     DataSave message (and by extension DataLoad) means file is untyped */
  if (message->data.data_save.file_type == FileType_Null)
    load_op_data->file_type = FileType_None;
  else
    load_op_data->file_type = message->data.data_save.file_type;

  load_op_data->ram_fetch.buffer = NULL; /* no flex block here */
  load_op_data->ram_fetch.buffer_size = 0;
  load_op_data->datasave_msg = NULL; /* no heap block here */
  load_op_data->callback.funct = finished_method; /* may be NULL */
  load_op_data->loader_funct = load_method; /* should be NULL */
  load_op_data->callback.arg = client_handle;

  /* Add new record to head of linked list */
  linkedlist_insert(&load_op_data_list, NULL, &load_op_data->list_item);

  /* There may be an indeterminate delay between us sending DataSaveAck
     and other task responding with a DataLoad message. (Sending
     DataSaveAck as recorded delivery breaks the SaveAs module, for one.)
     To prevent us leaking memory, we abandon stalled load operations after
     30 seconds. */
  e = scheduler_register_delay(_ldr2_time_out,
                               load_op_data,
                               DataLoadWaitTime,
                               SchedulerPriority_Min);

  if (e != NULL)
  {
    _ldr2_destroy_op(load_op_data);
    return e;
  }
  load_op_data->idle_function = true;

  /* How shall we reply (is RAM transfer allowed?) */
  if (message->data.data_save.file_type != FileType_Directory &&
      message->data.data_save.file_type != FileType_Application &&
      load_method == NULL)
  {
    /* Can try RAM transfer (see if they support it) */

    /* Calculate minimum size of the DataSave message, which may be less than
       message->hdr.size depending on the sophistication of its sender */
    int msg_size = WORD_ALIGN(sizeof(message->hdr) +
                   offsetof(WimpDataSaveMessage, leaf_name) +
                   strlen(message->data.data_save.leaf_name) + 1);

    assert(msg_size <= message->hdr.size);
    if (msg_size > message->hdr.size) /* paranoia */
      msg_size = message->hdr.size;

    /* Copy incoming DataSave message in case we need to reply to it later */
    load_op_data->datasave_msg = malloc(msg_size);
    if (load_op_data->datasave_msg == NULL)
    {
      /* De-link new record from head of linked list and scrap it */
      _ldr2_destroy_op(load_op_data);
      return lookup_error("NoMem", NULL); /* Could not claim memory */
    }

    DEBUGF("Loader2: Copying DataSave message to %p\n",
          (void *)load_op_data->datasave_msg);

    memcpy(load_op_data->datasave_msg, message, msg_size);
    load_op_data->datasave_msg->hdr.size = msg_size;

    /* Use estimated file size as buffer size unless it is implausible
       but allocate one extra byte to try to avoid a second RAMFetch. */
    load_op_data->ram_fetch.buffer_size =
      (message->data.data_save.estimated_size <= 0 ? DefaultBufferSize :
      message->data.data_save.estimated_size + 1);

    /* Attempt to allocate buffer for incoming data */
    DEBUGF("Loader2: Allocating RAM transfer buffer of %d bytes\n",
          load_op_data->ram_fetch.buffer_size);
    if (!flex_alloc(&load_op_data->ram_fetch.buffer,
                    load_op_data->ram_fetch.buffer_size))
    {
      _ldr2_destroy_op(load_op_data);
      return lookup_error("NoMem", NULL); /* Could not claim memory */
    }

    { /* Allocate (very) temporary buffer for a RAMFetch message */
      WimpMessage *reply = malloc(sizeof(message->hdr) +
                                  sizeof(WimpRAMFetchMessage));
      if (reply == NULL)
      {
        _ldr2_destroy_op(load_op_data);
        return lookup_error("NoMem", NULL); /* Could not claim memory */
      }

      /* Populate header of RAMFetch message */
      reply->hdr.size = sizeof(reply->hdr) + sizeof(WimpRAMFetchMessage);
      reply->hdr.your_ref = message->hdr.my_ref;
      reply->hdr.action_code = Wimp_MRAMFetch;

      /* Populate body of RAMFetch message */
      if (!load_op_data->no_flex_budge)
      {
        nobudge_register(PreExpandHeap); /* Protect copy of flex anchor in message */
        load_op_data->no_flex_budge = true;
      }
      reply->data.ram_fetch = load_op_data->ram_fetch;

      /* Send our reply to the sender of the DataSave message (recorded
         delivery) */
      e = wimp_send_message(Wimp_EUserMessageRecorded,
                            reply,
                            message->hdr.sender,
                            0,
                            NULL);
      if (e != NULL)
        _ldr2_destroy_op(load_op_data);
      else
        _ldr2_retain_my_ref(load_op_data, reply);

      free(reply);
    }
  }
  else
  {
    /* Must use Scrap transfer protocol */
    e = _ldr2_replyto_datasave(message, load_op_data);
    if (e != NULL)
      _ldr2_destroy_op(load_op_data);
  }

  return e;
}

/* ----------------------------------------------------------------------- */

void loader2_cancel_receives(Loader2FinishedHandler *finished_method, void *client_handle)
{
  /* Cancel any outstanding load operations for the specified client function
     and handle. Use when the destination has become invalid. */
  LoadOpCallback callback;

  DEBUGF("Loader2: Cancelling all load operations with handle %p\n",
        client_handle);

  callback.funct = finished_method;
  callback.arg = client_handle;

  linkedlist_for_each(&load_op_data_list, _ldr2_cancel_matching_op, &callback);
}

/* ----------------------------------------------------------------------- */

CONST _kernel_oserror *loader2_buffer_file(const char *file_path, flex_ptr buffer)
{
  assert(file_path != NULL);
  assert(buffer != NULL);
  DEBUGF("Loader2: will load '%s' into a flex block anchored at %p\n", file_path,
        (void *)buffer);

  /* Allocate buffer and load raw data into it... */
#ifdef USE_FILEPERC
  return file_perc_load(FilePercOp_Load, file_path, buffer);
#else
  /* Get size of file */
  int size;
  ON_ERR_RTN_E(get_file_size(file_path, &size));

  /* Allocate buffer for data */
  DEBUGF("Loader2: Allocating %ld bytes\n", size);
  if (size < 0 || !flex_alloc(buffer, size))
    return lookup_error("NoMem", NULL);

  /* Load file */
  _kernel_last_oserror(); /* reset */
  hourglass_on();
  FILE *const f = fopen_inc(file_path, "rb"); /* open for reading */
  size_t n = 0;
  if (f != NULL)
  {
    nobudge_register(PreExpandHeap); /* protect dereference of flex pointer */
    n = fread(*buffer, (size_t)size, 1, f);
    nobudge_deregister();

    fclose_dec(f);
  }
  hourglass_off();

  if (n != 1)
  {
    flex_free(buffer);
    ON_ERR_RTN_E(_kernel_last_oserror()); /* return any OS error */
    return lookup_error(f == NULL ? "OpenInFail" : "ReadFail", file_path);
  }
  else
  {
    return NULL; /* success */
  }
#endif
}

/* -----------------------------------------------------------------------
                        Wimp message handlers
*/

static int _ldr2_dataload_msg_handler(WimpMessage *message, void *handle)
{
  /* This handler must receive DataLoad messages before the Loader
     component. We need to intercept replies to our DataSave message. */
  LoadOpData *load_op_data;
  CONST _kernel_oserror *e;

  assert(message != NULL);
  NOT_USED(handle);

  DEBUGF("Loader2: Received a DataLoad message (ref. %d in reply to %d)\n",
        message->hdr.my_ref, message->hdr.your_ref);

  if (message->hdr.your_ref == 0 ||
      (load_op_data = _ldr2_find_record(message->hdr.your_ref)) == NULL)
  {
    DEBUGF("Loader2: Unknown your_ref value\n");
    return 0; /* not a reply to our message */
  }

  if (load_op_data->last_message_type != Wimp_MDataSaveAck)
  {
    DEBUGF("Loader2: Bad your_ref value\n");
    return 0; /* not a reply to a DataSave message */
  }

  if (load_op_data->idle_function) {
    scheduler_deregister(_ldr2_time_out, load_op_data);
    load_op_data->idle_function = false;
  }

  /* This should 'never' happen because unwonted DataLoad messages are
     rejected (see above) but free any allocated buffer just in case. */
  if (load_op_data->ram_fetch.buffer != NULL)
    flex_free(&load_op_data->ram_fetch.buffer);

  /* Update the file type associated with this load operation. Normally this
     should be the same as in the DataSave message, but we don't want to risk
     conflict with badly behaved applications that change their mind. */
  if (message->data.data_load.file_type == FileType_Null)
    load_op_data->file_type = FileType_None;
  else
    load_op_data->file_type = message->data.data_load.file_type;

  /* We cannot load the object unless it is a file */
  if (message->data.data_load.file_type != FileType_Application &&
      message->data.data_load.file_type != FileType_Directory)
  {
    if (load_op_data->loader_funct == NULL)
    {
      /* Use standard file loader */
      DEBUGF("Loader2: Using standard file loader\n");
      e = loader2_buffer_file(message->data.data_load.leaf_name,
                             &load_op_data->ram_fetch.buffer);
    }
    else
    {
      /* Use client's custom file loader */
      DEBUGF("Loader2: Using client's file loader\n");
      e = load_op_data->loader_funct(message->data.data_load.leaf_name,
                                     &load_op_data->ram_fetch.buffer);
    }
    if (e != NULL)
    {
      _ldr2_finished(load_op_data, false, e);
      return 1; /* claim message */
    }
  }

  /* Amend header of DataLoad message to change it into a DataLoadAck */
  message->hdr.your_ref = message->hdr.my_ref;
  message->hdr.action_code = Wimp_MDataLoadAck;

  /* Send our reply to the sender of the DataLoad message */
  e = wimp_send_message(Wimp_EUserMessage,
                        message,
                        message->hdr.sender,
                        0,
                        NULL);
  if (e != NULL)
  {
    _ldr2_finished(load_op_data, false, e);
    return 1; /* claim message */
  }
  DEBUGF("Loader2: Have sent DataLoadAck message (ref. %d)\n",
        message->hdr.my_ref);

  /* Delete the temporary file (our responsibility now that
     we have replied to the DataLoad message)  */
  remove(message->data.data_load.leaf_name);

  /* Data transfer completed successfully */
  _ldr2_finished(load_op_data, true, NULL);

  return 1; /* claim message */
}

/* ----------------------------------------------------------------------- */

static int _ldr2_ramtransmit_msg_handler(WimpMessage *message, void *handle)
{
  /* This is a handler for RAMTransmit messages */
  LoadOpData *load_op_data;
  CONST _kernel_oserror *e;

  assert(message != NULL);
  NOT_USED(handle);

  DEBUGF("Loader2: Received a RAMTransmit message (ref. %d in reply to %d)\n",
        message->hdr.my_ref, message->hdr.your_ref);

  if (message->hdr.your_ref == 0 ||
      (load_op_data = _ldr2_find_record(message->hdr.your_ref)) == NULL)
  {
    DEBUGF("Loader2: Unknown your_ref value\n");
    return 0; /* not a reply to our message */
  }

  if (load_op_data->last_message_type != Wimp_MRAMFetch)
  {
    DEBUGF("Loader2: Bad your_ref value\n");
    return 0; /* not a reply to a RAMFetch message */
  }

  DEBUGF("Loader2: %d bytes transferred to buffer at %p\n",
        message->data.ram_transmit.nbytes, message->data.ram_transmit.buffer);

  if (load_op_data->no_flex_budge)
  {
    nobudge_deregister(); /* no need to protect RAM buffer anymore */
    load_op_data->no_flex_budge = false;
  }

  /* Record the fact that the data saving task supports RAM transfer */
  if (!load_op_data->RAM_capable)
    load_op_data->RAM_capable = true;

  /* Has our input buffer been filled? */
  if (message->data.ram_transmit.nbytes < load_op_data->ram_fetch.buffer_size)
  {
    DEBUGF("Loader2: RAM transfer buffer not filled (finished)\n");

    /* Trim off any excess buffer that wasn't written to */
    if (!flex_extend(&load_op_data->ram_fetch.buffer,
                     flex_size(&load_op_data->ram_fetch.buffer) -
                       load_op_data->ram_fetch.buffer_size +
                       message->data.ram_transmit.nbytes))
    {
      /* Failed to change size of flex block */
      _ldr2_finished(load_op_data, false, lookup_error("NoMem", NULL));
      return 1; /* claim message */
    }

    /* Data transfer completed successfully */
    _ldr2_finished(load_op_data, true, NULL);
  }
  else
  {
    WimpMessage *reply;
    DEBUGF("Loader2: RAM transfer buffer filled (unfinished)\n");

    /* Extend the buffer for more data */
    if (!flex_extend(&load_op_data->ram_fetch.buffer,
                     flex_size(&load_op_data->ram_fetch.buffer) +
                     BufferExtend))
    {
      /* Failed to change size of flex block */
      _ldr2_finished(load_op_data, false, lookup_error("NoMem", NULL));
      return 1; /* claim message */
    }
    load_op_data->ram_fetch.buffer_size = BufferExtend;

    /* Allocate (very) temporary buffer for a RAMFetch message */
    reply = malloc(sizeof(message->hdr) + sizeof(WimpRAMFetchMessage));
    if (reply == NULL)
    {
      _ldr2_finished(load_op_data, false, lookup_error("NoMem", NULL));
      return 1; /* claim message */
    }

    /* Populate header of RAMFetch message */
    reply->hdr.size = sizeof(reply->hdr) + sizeof(WimpRAMFetchMessage);
    reply->hdr.your_ref = message->hdr.my_ref;
    reply->hdr.action_code = Wimp_MRAMFetch;

    /* Populate body of RAMFetch message
       (tell them to write at the end of the data already received) */
    if (!load_op_data->no_flex_budge)
    {
      nobudge_register(PreExpandHeap); /* Protect copy of flex anchor in message */
      load_op_data->no_flex_budge = true;
    }
    reply->data.ram_fetch.buffer = (char *)load_op_data->ram_fetch.buffer +
                                   flex_size(&load_op_data->ram_fetch.buffer) -
                                   BufferExtend;
    reply->data.ram_fetch.buffer_size = load_op_data->ram_fetch.buffer_size;

    /* Send our reply to the sender of the RAMTransmit message (recorded
       delivery) */
    e = wimp_send_message(Wimp_EUserMessageRecorded,
                          reply,
                          message->hdr.sender,
                          0,
                          NULL);
    if (e != NULL)
      _ldr2_finished(load_op_data, false, e);
    else
      _ldr2_retain_my_ref(load_op_data, reply);

    free(reply);
  } /* was not last bufferful */

  return 1; /* claim message */
}

/* -----------------------------------------------------------------------
                        Wimp event handlers
*/

static int _ldr2_msg_bounce_handler(int event_code, WimpPollBlock *event, IdBlock *id_block, void *handle)
{
  /* This is a handler for bounced messages */
  LoadOpData *load_op_data;
  CONST _kernel_oserror *e;

  NOT_USED(event_code);
  assert(event != NULL);
  NOT_USED(id_block);
  NOT_USED(handle);

  DEBUGF("Loader2: Received a bounced message (ref. %d)\n",
        event->user_message_acknowledge.hdr.my_ref);

  load_op_data = _ldr2_find_record(event->user_message_acknowledge.hdr.my_ref);
  if (load_op_data == NULL)
  {
    DEBUGF("Loader2: Unknown message ID\n");
    return 0; /* not the last message we sent */
  }

  switch (event->user_message_acknowledge.hdr.action_code)
  {
    case Wimp_MRAMFetch:
      DEBUGF("Loader2: It is a bounced RAMFetch message\n");
      if (load_op_data->no_flex_budge)
      {
        nobudge_deregister(); /* no need to protect RAM buffer anymore */
        load_op_data->no_flex_budge = false;
      }

      if (!load_op_data->RAM_capable)
      {
        /* Use file transfer instead, by replying to the old DataSave message */
        flex_free(&load_op_data->ram_fetch.buffer);

        e = _ldr2_replyto_datasave(load_op_data->datasave_msg, load_op_data);

        free(load_op_data->datasave_msg); /* no longer require copy of DataSave */
        load_op_data->datasave_msg = NULL;
        if (e != NULL)
          _ldr2_finished(load_op_data, false, e);
      }
      else
      {
        /* RAM transfer broke in the middle. PRM says "No error should be
           generated because the other end will have already reported one." */
        _ldr2_finished(load_op_data, false, NULL);
      }
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

static void _ldr2_finished(LoadOpData *load_op_data, bool success, CONST _kernel_oserror *e)
{
  DEBUGF("Loader2: Operation %p finished %ssuccessfully\n", (void *)load_op_data,
        success ? "" : "un");
  assert(!success || e == NULL);
  assert(load_op_data != NULL);

  DEBUGF("Loader2: Current address of flex block is %p\n",
        load_op_data->ram_fetch.buffer);

  /* If the load failed or the client supplied no handler function
     then release any memory allocated for the input buffer */
  if (load_op_data->ram_fetch.buffer != NULL &&
      (load_op_data->callback.funct == NULL || !success))
  {
    DEBUGF("Loader2: Freeing loaded data\n");
    flex_free(&load_op_data->ram_fetch.buffer);
  }

  if (load_op_data->callback.funct != NULL)
  {
    /* Call the client-supplied function to notify it that the load
       operation is complete. */
    DEBUGF("Loader2: Calling completion function with %p\n", load_op_data->callback.arg);
    load_op_data->callback.funct(e,
                                 success ?
                                   load_op_data->file_type : FileType_Null,
                                 load_op_data->ram_fetch.buffer == NULL ?
                                   NULL : &load_op_data->ram_fetch.buffer,
                                 load_op_data->callback.arg);
  }

  /* Free data block for a save operation and de-link it from the list*/
  _ldr2_destroy_op(load_op_data);
}

/* ----------------------------------------------------------------------- */

static LoadOpData *_ldr2_find_record(int msg_ref)
{
  LoadOpData *load_op_data;

  DEBUGF("Loader2: Searching for operation awaiting reply to %d\n", msg_ref);
  load_op_data = (LoadOpData *)linkedlist_for_each(
                 &load_op_data_list, _ldr2_op_has_ref, &msg_ref);

  if (load_op_data == NULL)
  {
    DEBUGF("Loader2: End of linked list (no match)\n");
  }
  else
  {
    DEBUGF("Loader2: Record %p has matching message ID\n", (void *)load_op_data);
  }
  return load_op_data;
}

/* ----------------------------------------------------------------------- */

static CONST _kernel_oserror *_ldr2_replyto_datasave(const WimpMessage *reply_to, LoadOpData *load_op_data)
{
  CONST _kernel_oserror *e;
  WimpMessage *reply;
  int msg_size;

  assert(reply_to != NULL);
  assert(load_op_data != NULL);
  DEBUGF("Loader2: Replying to DataSave message ref. %d\n", reply_to->hdr.my_ref);

  /* Allocate (very) temporary buffer for a DataSaveAck message */
  msg_size = WORD_ALIGN(sizeof(reply->hdr) +
                        offsetof(WimpDataSaveAckMessage, leaf_name) +
                        sizeof("<Wimp$Scrap>"));

  reply = malloc(msg_size);
  if (reply == NULL)
    return lookup_error("NoMem", NULL); /* Memory couldn't be claimed */

  /* Populate header of DataSaveAck message */
  reply->hdr.size = msg_size;
  reply->hdr.your_ref = reply_to->hdr.my_ref;
  reply->hdr.action_code = Wimp_MDataSaveAck;

  /* Populate body of DataSaveAck message
     (mostly copied from the DataSave message) */
  reply->data.data_save_ack.destination_window =
    reply_to->data.data_save.destination_window;

  reply->data.data_save_ack.destination_icon =
    reply_to->data.data_save.destination_icon;

  reply->data.data_save_ack.destination_x =
    reply_to->data.data_save.destination_x;

  reply->data.data_save_ack.destination_y =
    reply_to->data.data_save.destination_y;

  reply->data.data_save_ack.estimated_size = -1; /* not a safe destination */
  reply->data.data_save_ack.file_type = reply_to->data.data_save.file_type;
  strcpy(reply->data.data_save_ack.leaf_name, "<Wimp$Scrap>");

  /* Send our reply to the sender of the DataSave message */
  e = wimp_send_message(Wimp_EUserMessage,
                        reply,
                        reply_to->hdr.sender,
                        0,
                        NULL);

  if (e == NULL)
    _ldr2_retain_my_ref(load_op_data, reply);

  free(reply);

  return e; /* success */
}

/* ----------------------------------------------------------------------- */

static void _ldr2_destroy_op(LoadOpData *load_op_data)
{
  DEBUGF("Loader2: Removing record of operation %p\n", (void *)load_op_data);
  assert(load_op_data != NULL);

  if (load_op_data->no_flex_budge)
    nobudge_deregister();

  if (load_op_data->idle_function)
    scheduler_deregister(_ldr2_time_out, load_op_data);

  free(load_op_data->datasave_msg);

  if (load_op_data->ram_fetch.buffer != NULL)
    flex_free(&load_op_data->ram_fetch.buffer);

  linkedlist_remove(&load_op_data_list, &load_op_data->list_item);

  free(load_op_data);
}

/* ----------------------------------------------------------------------- */

static SchedulerTime _ldr2_time_out(void *handle, SchedulerTime time_now, const volatile bool *time_up)
{
  /* This function is called by the scheduler 30 seconds after we start
     a load operation (unless we cancel it in the interim). We use it to
     free up resources associated with stalled load operations. */
  LoadOpData *load_op_data = handle;

  assert(handle != NULL);
  NOT_USED(time_up);

  DEBUGF("Loader2: Load operation %p timed out\n", (void *)load_op_data);
  _ldr2_finished(load_op_data, false, NULL);

  return time_now; /* Return time doesn't actually matter because
                      _ldr2_finished() deregisters this function */
}

/* ----------------------------------------------------------------------- */

static bool _ldr2_cancel_matching_op(LinkedList *list, LinkedListItem *item, void *arg)
{
  LoadOpData * const load_op_data = (LoadOpData *)item;
  const LoadOpCallback * const callback = arg;

  assert(load_op_data != NULL);
  NOT_USED(list);

  /* Check whether this data request is for delivery to the specified
     function with the specified handle. NULL means cancel all. */
  if (callback == NULL ||
      (load_op_data->callback.funct == callback->funct &&
       load_op_data->callback.arg == callback->arg))
  {
    _ldr2_finished(load_op_data, false, NULL);
  }

  return false; /* next item */
}

/* ----------------------------------------------------------------------- */

static bool _ldr2_op_has_ref(LinkedList *list, LinkedListItem *item, void *arg)
{
  const int *msg_ref = arg;
  const LoadOpData * const load_op_data = (LoadOpData *)item;

  assert(msg_ref != NULL);
  assert(load_op_data != NULL);
  NOT_USED(list);

  return (load_op_data->last_message_ref == *msg_ref);
}

/* ----------------------------------------------------------------------- */

static void _ldr2_retain_my_ref(LoadOpData *load_op_data, const WimpMessage *msg)
{
  assert(load_op_data != NULL);
  assert(msg != NULL);

  load_op_data->last_message_ref = msg->hdr.my_ref;
  load_op_data->last_message_type = msg->hdr.action_code;

  DEBUGF("Loader2: sent message with code %d and ref. %d in reply to %d\n",
        msg->hdr.action_code, msg->hdr.my_ref, msg->hdr.your_ref);
}
