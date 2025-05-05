/*
 * CBLibrary: Begin the receiver's half of the data transfer message protocol
 * Copyright (C) 2003 Christopher Bazley
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
  CJB: 11-Sep-03: Changed _ldr_check_dropzone() so that NULL_ObjectId rather
                  than -1 is the marker of a listener that doesn't care which
                  object the file was dragged to. This changes the functional
                  definition of loader_register_listener() - sorry.
                  loader_buffer_file() now accepts const character strings as
                  file_path.
  CJB: 20-Feb-04: Now makes less use of strncpy() where unnecessary.
  CJB: 07-Mar-04: Updated to use the new macro names defined in h.Macros.
                  Now uses new CLONE_STR macro in place of custom code (should
                  behave the same).
  CJB: 28-Apr-04: Now uses standard library function remove() to delete a
                  temporary !Scrap file, rather than OS_FSControl 27.
  CJB: 13-Jun-04: Because all macro definitions are now expression statements,
                  have changed those invocations which omitted a trailing ';'.
                  The loader_canonicalise() function is now just an alternative
                  entrypoint for an external canonicalise() function.
  CJB: 13-Jan-05: Changed to use new msgs_error() function, hence no
                  longer requires external error block 'shared_err_block'.
                  datasave_leafname memory is now freed upon loader_finalise().
                  Replaced Andrew Hodgkinson's c.s.a.p. posting with summary.
  CJB: 25-Jan-05: Suggested title in absence of leaf name is now '<untitled>'
                  (as ROM Apps) rather than '<Untitled>'. Added new function
                  to remove any listeners belonging to a given object. Made
                  changes necessary to allow client intervention (via a
                  LoaderPreFilter function) prior to starting a data transfer.
  CJB: 27-Jan-05: Changed logic in _ldr_dataloadopen_listener to allow message
                  ref of 0 rather than -1 (may occur?) to be stored to prevent
                  wrong leaf name being picked up.
  CJB: 29-Jan-05: File names are now canonicalised before being passed to a
                  LoaderPreFilter function in order to facilitate rejection of
                  duplicates (e.g. file already loaded).
  CJB: 05-Mar-05: loader_register_listener and loader_deregister_listener now
                  use assert() rather than returning a BadListReg error.
  CJB: 01-Feb-06: Extended _ldr_dataloadopen_listener() and
                  _ldr_find_suitable_listener() to translate between different
                  indication of untyped files in DataSave and DataOpen messages
                  (-1 vs &3000; well done Acorn).
                  Modified _ldr_dataloadopen_listener() so that any client-
                  supplied LoaderFileHandler is not called for directories.
                  Modified _ldr_datasave_listener() so that it doesn't attempt
                  to use RAM transfer for directories!
                  Trimmed args for _ldr_find_suitable_listener(); permanence of
                  incoming data now indicated by whether file path is NULL.
                  _ldr_replyto_datasave() now uses WORD_ALIGN macro.
                  _ldr_find_suitable_listener() now calls any pre-filter for a
                  listener regardless of whether the drag destination matches
                  its drop zone. Now understands new return code 3 to allow a
                  pre-filter to reject data without implying file type is
                  invalid.
                  Fixed bug in _ldr_find_suitable_listener(), which treated
                  file type of data as unrecognised if 0 was returned by the
                  last pre-filter function called. Now records the file type as
                  recognised if any pre-filter returns code 3.
  CJB: 05-Feb-06: Substituted sizeof(message->hdr) for hardcoded 20's and
                  sizeof(size_t) for sizeof(int) in calculation of extra memory
                  required for sprite area.
                  Removed unnecessary casts to void pointer type.
                  Changed RAM_buffer from 'int *' to 'char *', hence no longer
                  need casts when calculating address of RAMFetch buffer. Added
                  cast to 'size_t *' when writing sprite area length.
  CJB: 06-Feb-06: Updated to use strdup() function instead of CLONE_STR macro.
                  Renamed various functions:
                    _ldr_ramfetch_bounce_handler -> _ldr_msg_bounce_handler
                    _ldr_ramtransmit_handler -> _ldr_ramtransmit_msg_handler
                    _ldr_dataloadopen_listener -> _ldr_dataloadopen_msg_handler
                    _ldr_datasave_listener -> _ldr_datasave_msg_handler
                  Moved registration of handlers for Wimp_MRAMTransmit and
                  Wimp_EUserMessageAcknowledge from _ldr_new_thread() to
                  loader_initialise() and deregistration from _ldr_kill_thread()
                  to loader_finalise(). This should fix crashes in Event library
                  caused by _ldr_msg_bounce_handler() deregistering itself.
                  Also _ldr_kill_thread() and _ldr_kill_listener() cannot fail
                  soft and therefore no longer return a _kernel_oserror pointer.
                  Added code at start of _ldr_ramtransmit_msg_handler() and
                  _ldr_msg_bounce_handler() to search linked list of RAM
                  transfer status blocks for relevant one (can no longer rely on
                  client handle passed by Event library).
                  If compiled with symbol USE_FILEPERC #define'd then
                  loader_buffer_file() will be just a veneer to
                  perc_operation().
  CJB: 03-Sep-06: No longer faults DataSave or DataOpen messages with non-zero
                  'your ref' (for compatibility with app note 241).
  CJB: 04-Sep-06: Now passes 'your ref' from DataSave/Load/Open message to the
                  client's LoaderFinishedHandler to allow it to associate the
                  data with an earlier DragClaim message.
  CJB: 11-Sep-06: Now passes the x & y coordinates from the DataSave/Load
                  message to the client's LoaderFinishedHandler for accurate
                  positioning of imported data. Additional debugging output.
                  Now uses generic symbol COPY_ARRAY_ARGS instead of old
                  NO_STATIC_GADGETS_LIST. Used assertions to prevent use when
                  not initialised.
                  Fixed a bug where loader_deregister_listeners_for_object()
                  read from a LoaderListenerBlk immediately after it had been
                  freed (only showed up with Fortify).
                  Fixed a heinous bug in _ldr_kill_listener()'s traversal of
                  the linked list of LoaderDialogueBlk's. When killing a
                  listener with open RAM transfer(s), it instead attempted to
                  deallocate the following LoaderDialogueBlk, probably causing
                  _ldr_kill_thread() to deference a NULL pointer.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 25-Oct-06: Too many changes to list. Broadly speaking, all the code to
                  handle an actual data transfer has been moved to c.Loader2.
  CJB: 28-Oct-06: Additional debugging output. Qualified the pointer to a
                  list of ComponentId's passed to loader_register_listener() as
                  'const', and fixed a bug where this function miscounted the
                  length of this list (our copy of it was unterminated). Fixed
                  a bug where _ldr_find_listener could return false positives
                  if the listener had no gadgets list or none was passed to this
                  function. Fixed a recently introduced bug where
                  _ldr_dataloadopen_msg_handler() was treating the input buffer
                  pointer, rather than its address, as a pointer to a pointer to
                  a sprite area (crashed if LISTENER_SPRITEAREAS flag set).
                  Coordinates passed to a LoaderFinishedHandler are now relative
                  to origin of destination window's work area.
  CJB: 30-Oct-06: Modified _ldr_finished to report load errors as non-fatal and
                  using the "LoadFail" prefix, rather than offering to quit.
  CJB: 27-Nov-06: Made compilation of this source file conditional upon pre-
                  processor symbol CBLIB_OBSOLETE (superceded by c.Loader2).
  CJB: 14-Apr-07: Modified loader_finalise() to soldier on if an error occurs,
                  using the new MERGE_ERR macro.
  CJB: 22-Jun-09: Bugfix: _ldr_kill_listener() now cancels outstanding data
                  transfers properly by using loader2_cancel_receives() instead
                  of calling _ldr_finished() directly. The Loader2 module would
                  previously have been left with a stale ExtraOpData pointer!
                  Use variable name rather than type with 'sizeof' operator,
                  removed unnecessary casts from 'void *' and tweaked spacing.
  CJB: 08-Sep-09: Stop using reserved identifiers '_LoaderListenerBlk' and
                  '_ExtraOpData' (start with an underscore followed by a
                  capital letter). Now iterates over an array of Wimp message
                  numbers and function pointers when registering or
                  deregistering message handlers (reduces code size and ensures
                  symmetry).
  CJB: 17-Dec-14: Updated to use the generic linked list implementation.
  CJB: 23-Dec-14: Apply Fortify to Toolbox, Event & Wimp library function calls.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 06-Apr-16: Modified a loop counter's type to avoid GNU C compiler
                  warnings about unsigned comparisons with signed integers.
  CJB: 18-Apr-16: Cast pointer parameters to void * to match %p. No longer
                  prints function pointers (no matching format specifier).
  CJB: 01-Nov-18: Replaced DEBUG macro usage with DEBUGF.
                  Get rid of nested #ifdef CBLIB_OBSOLETE blocks.
  CJB: 29-Aug-20: Deleted a redundant static function pre-declaration.
  CJB: 03-May-25: Fix #include filename case.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
*/

#ifdef CBLIB_OBSOLETE /* Use c.Loader2 instead */

/* ISO library headers */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "wimp.h"
#include "event.h"
#include "wimplib.h"
#include "flex.h"
#include "toolbox.h"
#include "iconbar.h"
#include "gadgets.h"
#include "window.h"

/* CBUtilLib headers */
#include "StrExtra.h"
#include "LinkedList.h"

/* CBOSLib headers */
#include "SprFormats.h"

/* Local headers */
#include "Err.h"
#include "msgtrans.h"
#include "Loader.h"
#include "Canonical.h"
#ifdef USE_FILEPERC
#include "FilePerc.h"
#endif
#include "Loader2.h"
#include "Internal/CBMisc.h"

#define ERR_BAD_OBJECT_ID    0x1b80cb02
#define ERR_BAD_COMPONENT_ID 0x1b80a914

#define LOADER_CLIENT_FILTER 16 /* internal flag only! */

#define PREVENT_TRANSFER (LoaderListenerBlk *)-1

/* Filetype/destination to listen for */
typedef struct
{
  union
  {
    int                     file_type;
    LoaderPreFilter        *filter_method;
  }
  pre_filter;
  ObjectId                  object;
  const ComponentId        *gadgets; /* malloc'd if COPY_ARRAY_ARGS defined */
}
LoaderListenerCriteria;

typedef struct LoaderListenerBlk
{
  LinkedListItem            list_item;
  int                       flags;
  LoaderListenerCriteria    criteria;
  /* Client participation */
  LoaderFileHandler        *loader_method;   /* optional */
  LoaderFinishedHandler    *finished_method; /* compulsary */
  void                     *client_handle; /* passed to finished_method */
}
LoaderListenerBlk;

/* The following structure holds state for a given load operation (additional
   to that stored internally by Loader2 in a _LoadOpData struct) */
typedef struct ExtraOpData
{
  LinkedListItem      list_item;
  LoaderListenerBlk  *listener;
  char               *leaf_name; /* malloc block */
  int                 drop_x;
  int                 drop_y;
}
ExtraOpData;

/* -----------------------------------------------------------------------
                        Internal function prototypes
*/

static WimpMessageHandler _ldr_datasave_msg_handler,
                          _ldr_dataloadopen_msg_handler;
static void _ldr_kill_listener(LoaderListenerBlk *kill_listener);
static LoaderListenerBlk *_ldr_find_broadcast_listener(const char *file_path, int file_type);
static LoaderListenerBlk *_ldr_find_listener(LoaderListenerCriteria *criteria);
static LoaderListenerBlk *_ldr_find_suitable_listener(const char *perma_file_path, int window, int icon, int file_type);
static Loader2FinishedHandler _ldr_finished;
static void _ldr_destroy_extra_op(ExtraOpData *extra_op_data);
static _Optional CONST _kernel_oserror *_ldr_abs_to_work_area(int window_handle, int *x, int *y);
static LinkedListCallbackFn _ldr_cancel_matching_op, _ldr_kill_listener_for_object, _ldr_listener_is_match;

/* -----------------------------------------------------------------------
                          Internal library data
*/

static LinkedList listener_list;
static LinkedList extra_op_data_list;
static unsigned int control_flags;
static bool initialised = false;
static const struct
{
  int                 msg_no;
  WimpMessageHandler *handler;
}
msg_handlers[] =
{
  {
    Wimp_MDataLoad,
    _ldr_dataloadopen_msg_handler
  },
  {
    Wimp_MDataSave,
    _ldr_datasave_msg_handler
  },
  {
    Wimp_MDataOpen,
    _ldr_dataloadopen_msg_handler
  }
};

/* -----------------------------------------------------------------------
                         Public library functions
*/

_Optional CONST _kernel_oserror *loader_initialise(unsigned int flags)
{
  unsigned int mask;

  assert(!initialised);

  /* Initialise linked lists */
  linkedlist_init(&listener_list);
  linkedlist_init(&extra_op_data_list);

  control_flags = flags;

  /* Register Wimp message handlers for data transfer protocol */
  for (size_t i = 0; i < ARRAY_SIZE(msg_handlers); i++)
  {
    if (msg_handlers[i].msg_no == Wimp_MDataOpen &&
        TEST_BITS(control_flags, LOADER_IGNOREBCASTS))
      continue; /* skip this one */

    ON_ERR_RTN_E(event_register_message_handler(msg_handlers[i].msg_no,
                                                msg_handlers[i].handler,
                                                NULL));
  }

  /* Ensure that messages are not masked */
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
_Optional CONST _kernel_oserror *loader_finalise(void)
{
  _Optional CONST _kernel_oserror *return_error = NULL;

  assert(initialised);
  initialised = false;

  /* Kill all listeners  */
  linkedlist_for_each(&listener_list, _ldr_kill_listener_for_object, NULL);

  /* Deregister Wimp message handlers for data transfer protocol */
  for (size_t i = 0; i < ARRAY_SIZE(msg_handlers); i++)
  {
    if (msg_handlers[i].msgs_no == Wimp_MDataOpen &&
        TEST_BITS(control_flags, LOADER_IGNOREBCASTS))
      continue; /* skip this one */

    MERGE_ERR(return_error,
              event_deregister_message_handler(msg_handlers[i].msgs_no,
                                               msg_handlers[i].handler,
                                               NULL));
  }

  return return_error;
}
#endif

/* ----------------------------------------------------------------------- */

_Optional CONST _kernel_oserror *loader_register_listener(unsigned int flags, int file_type, ObjectId drop_object, const ComponentId *drop_gadgets, LoaderFileHandler *loader_method, LoaderFinishedHandler *finished_method, void *client_handle)
{
  LoaderListenerBlk *newlistener;
  LoaderListenerCriteria criteria;

  assert(initialised);

  DEBUGF("Loader: Request to register listener with flags %u for files of type "
         "&%X dropped on object ID %d\n", flags, file_type, drop_object);

  /* Check that proposed key is unique */
  criteria.object = drop_object;
  criteria.pre_filter.file_type = file_type;
  criteria.gadgets = drop_gadgets;
  if (_ldr_find_listener(&criteria) != NULL) {
    assert(false); /* listener not found */
    return NULL;
  }

  /* Initialise new listener */
  newlistener = malloc(sizeof(*newlistener));
  if (newlistener == NULL)
    return msgs_error(DUMMY_ERRNO, "NoMem");

  DEBUGF("Loader: New listener data is at %p\n", (void *)newlistener);

  newlistener->flags = flags;

#ifdef COPY_ARRAY_ARGS
  /*
     Previously drop_gadgets had to be a static array
     Nowadays we make our own copy just to be on the safe side
  */
  if (drop_gadgets != NULL) {
    /* count number of gadgets in array */
    ComponentId *gadgets_copy;
    size_t array_len = 0;
    do {
      assert(array_len < 16); /* not certain but suggests a bug */
      DEBUGF("Loader: Array element %zu is gadget %d\n", array_len,
             drop_gadgets[array_len]);
    } while (drop_gadgets[array_len++] != NULL_ComponentId);

    /* Duplicate the array of gadgets */
    DEBUGF("Loader: About to clone array of %zu elements\n", array_len);
    gadgets_copy = malloc(array_len * sizeof(*gadgets_copy));
    if (gadgets_copy == NULL) {
      free(newlistener);
      return msgs_error(DUMMY_ERRNO, "NoMem");
    }
    memcpy(gadgets_copy,
           drop_gadgets,
           array_len * sizeof(*criteria.gadgets));
    criteria.gadgets = gadgets_copy;
  }
#endif

  newlistener->criteria = criteria;

  /* Set client handlers */
  newlistener->loader_method = loader_method;
  newlistener->finished_method = finished_method;
  newlistener->client_handle = client_handle;

  /* Link this Listener onto front of list */
  linkedlist_insert(&listener_list, NULL, &newlistener->list_item);

  return NULL; /* success */
}

/* ----------------------------------------------------------------------- */

_Optional CONST _kernel_oserror *loader_deregister_listeners_for_object(ObjectId drop_object)
{
  /* Kill all listeners for a given object */
  DEBUGF("Loader: Removing all listeners for object %d\n", drop_object);

  assert(initialised);

  linkedlist_for_each(&listener_list, _ldr_kill_listener_for_object, &drop_object);

  return NULL; /* success */
}

/* ----------------------------------------------------------------------- */

_Optional CONST _kernel_oserror *loader_deregister_listener(int file_type, ObjectId drop_object, const ComponentId *drop_gadgets)
{
  /* Kill a specified listener */
  LoaderListenerBlk *find_it;
  LoaderListenerCriteria criteria;

  assert(initialised);

  criteria.object = drop_object;
  criteria.pre_filter.file_type = file_type;
  criteria.gadgets = drop_gadgets;

  find_it = _ldr_find_listener(&criteria);
  assert(find_it != NULL);
  if (find_it != NULL)
    _ldr_kill_listener(find_it); /* remove the stinking thing */

  return NULL; /* success */
}

/* ----------------------------------------------------------------------- */

/* The following function is deprecated; use canonicalise(). */
_Optional CONST _kernel_oserror *loader_canonicalise(_Optional char **buffer, _Optional const char *path_var, _Optional const char *path_string, const char *file_path)
{
  return canonicalise(buffer, path_var, path_string, file_path);
}

/* ----------------------------------------------------------------------- */

/* The following function is deprecated; use loader2_buffer_file(). */
_Optional CONST _kernel_oserror *loader_buffer_file(const char *file_path, flex_ptr buffer, bool sprite_file)
{
  DEBUGF("Loader: will load %sfile '%s' into a flex block anchored at %p\n",
         sprite_file ? "sprite " : "", file_path, (void *)buffer);

  /* Allocate buffer and load raw data into it... */
  ON_ERR_RTN_E(loader2_buffer_file(file_path, buffer));

  if (sprite_file && buffer != NULL) {
    spriteareaheader **area = (spriteareaheader **)buffer;
    if (!flex_midextend(buffer, 0, sizeof((*area)->size))) {
      /* Failed to extend input buffer at start */
      flex_free(buffer);
      return msgs_error(DUMMY_ERRNO, "NoMem");
    }
    /* Write sprite area size as first word */
    (*area)->size = flex_size(buffer);
  }

  return NULL; /* success */
}

/* -----------------------------------------------------------------------
                        Wimp message handlers
*/

static int _ldr_datasave_msg_handler(WimpMessage *message, void *handle)
{
  /* This is a handler for DataSave messages. It must be registered early or
     else it will intercept messages intended for the Loader2 or Entity library
     components). */
  _Optional CONST _kernel_oserror *e;
  LoaderListenerBlk *listener;
  ExtraOpData *extra_op_data;
  NOT_USED(handle);

  /* Are any listeners interested?
     According to the RISC OS 3 PRM a file type value of &ffffffff in a
     DataSave message (and by extension DataLoad) means file is untyped */
  listener = _ldr_find_suitable_listener(
                       NULL,
                       message->data.data_save.destination_window,
                       message->data.data_save.destination_icon,
                       message->data.data_save.file_type == -1 ? FILETYPE_NONE :
                       message->data.data_save.file_type);
  if (listener == PREVENT_TRANSFER || listener == NULL)
    return 0; /* message not handled */

  /* Allocate data block for new save operation and link it into the list */
  DEBUGF("Loader: Creating a record for a load operation\n");
  extra_op_data = malloc(sizeof(*extra_op_data));
  if (extra_op_data == NULL) {
    WARN_GLOB("NoMem");
    return 1; /* claim message */
  }

  /* Initialise record for a new save operation */
  extra_op_data->listener = listener;
  extra_op_data->leaf_name = strdup(message->data.data_save.leaf_name);
  if (extra_op_data->leaf_name == NULL) {
    WARN_GLOB("NoMem");
    free(extra_op_data);
    return 1; /* claim message */
  }
  extra_op_data->drop_x = message->data.data_save.destination_x;
  extra_op_data->drop_y = message->data.data_save.destination_y;

  /* Convert destination from absolute screen coordinates
     to window work area coordinates */
  if (E(_ldr_abs_to_work_area(message->data.data_save.destination_window,
                              &extra_op_data->drop_x,
                              &extra_op_data->drop_y))) {
    free(extra_op_data);
    return 1; /* claim message */
  }

  /* Add new record to head of linked list */
  linkedlist_insert(&extra_op_data_list, NULL, &extra_op_data->list_item);
  DEBUGF("Loader: New record is at %p\n", (void *)extra_op_data);

  e = loader2_receive_data(message,
                           listener->loader_method,
                           _ldr_finished,
                           extra_op_data);
  if (e != NULL) {
    _ldr_destroy_extra_op(extra_op_data);
    err_check_rep(e);
  }

  return 1; /* claim message */
}

/* ----------------------------------------------------------------------- */

static int _ldr_dataloadopen_msg_handler(WimpMessage *message, void *handle)
{
  /* This is a handler for DataLoad and DataOpen messages. It must be
     registered early or else it will intercept messages intended for the
     Loader2 or Entity library components). */
  _Optional CONST _kernel_oserror *e;
  _Optional char *full_path = NULL;
  LoaderListenerBlk *found_listener;
  int file_type, drop_x, drop_y;
  NOT_USED(handle);

  DEBUGF("Loader: Received a Data%s message (ref. %d in reply to %d)\n",
        message->hdr.action_code == Wimp_MDataLoad ? "Load" : "Open",
        message->hdr.my_ref, message->hdr.your_ref);

  if (message->hdr.action_code == Wimp_MDataOpen) {
    /* It's a DataOpen message (broadcast when the user double-clicks a file) */
    ON_ERR_RPT_RTN_V(canonicalise(&full_path, NULL, NULL,
                                  message->data.data_open.path_name), 0);

    file_type = message->data.data_open.file_type;
    drop_x = drop_y = -1; /* file not received via drag and drop */

    found_listener = _ldr_find_broadcast_listener(full_path, file_type);
  }
  else {
    /* It's a DataLoad message (sent to request us to load a file) */
    assert(message->hdr.action_code == Wimp_MDataLoad);

    ON_ERR_RPT_RTN_V(canonicalise(&full_path, NULL, NULL,
                                  message->data.data_load.leaf_name), 0);

    /* According to the RISC OS 3 PRM a file type value of &ffffffff in a
       DataSave message (and by extension DataLoad) means file is untyped */
    file_type = (message->data.data_load.file_type == -1 ? FILETYPE_NONE :
                message->data.data_load.file_type);

    drop_x = message->data.data_load.destination_x;
    drop_y = message->data.data_load.destination_y;

    found_listener = _ldr_find_suitable_listener(full_path,
                     message->data.data_load.destination_window,
                     message->data.data_load.destination_icon, file_type);
  }

  if (found_listener == NULL) {
    DEBUGF("Loader: no suitable listener found\n");
    free(full_path);
    return 0; /* message not handled */
  }

  if (message->hdr.action_code == Wimp_MDataLoad) {
    /* Convert destination from absolute screen coordinates
       to window work area coordinates */
    if (E(_ldr_abs_to_work_area(message->data.data_load.destination_window,
                                &drop_x,
                                &drop_y))) {
      free(full_path);
      return 1; /* claim message */
    }
  }

  if (message->hdr.action_code == Wimp_MDataOpen ||
      found_listener == PREVENT_TRANSFER)
  {
    /* Acknowledge immediately */
    message->hdr.your_ref = message->hdr.my_ref;
    message->hdr.action_code = Wimp_MDataLoadAck;
    if (E(wimp_send_message(Wimp_EUserMessage,message, message->hdr.sender, 0,
    NULL))) {
      free(full_path);
      return 1; /* error - claim message */
    }
    DEBUGF("Loader: Have sent DataLoadAck message (ref. %d)\n",
          message->hdr.my_ref);

    if (found_listener == PREVENT_TRANSFER) {
      free(full_path);
      return 1; /* nothing more to do - claim message */
    }
  }
  DEBUGF("Loader: Listener data is at %p, flags are %u\n", (void *)found_listener,
        found_listener->flags);

  /* Call file loader */
  void *input_buffer = NULL;
  if (message->data.data_load.file_type != FILETYPE_APP &&
  message->data.data_load.file_type != FILETYPE_DIR) {
    if (found_listener->loader_method == NULL) {
      /* Use standard file loader */
      DEBUGF("Loader: using standard file loader\n");
      e = loader2_buffer_file(message->data.data_load.leaf_name, &input_buffer);
    } else {
      /* Use client's custom file loader */
      DEBUGF("Loader: using client's file loader\n");
      e = found_listener->loader_method(message->data.data_load.leaf_name,
                                        &input_buffer);
    }
    if (e != NULL) {
      /* Loading error */
      err_report(e->errnum, msgs_lookupsubn("LoadFail", 1, e->errmess));
      free(full_path);
      return 1; /* claim message */
    }
  }

  if (message->hdr.action_code == Wimp_MDataLoad) {
    /* Acknowledge loaded successfully (just a courtesy message,
       so we don't expect a reply) */
    message->hdr.your_ref = message->hdr.my_ref;
    message->hdr.action_code = Wimp_MDataLoadAck;
    if (E(wimp_send_message(Wimp_EUserMessage, message, message->hdr.sender, 0,
    NULL))) {
      free(full_path);
      if (input_buffer != NULL)
        flex_free(&input_buffer);
      return 1; /* claim message */
    }
    DEBUGF("Loader: Have sent DataLoadAck message (ref. %d)\n",
          message->hdr.my_ref);
  }

  /* Pre-pend sprite area size if the listener has the relevant flags bit set */
  if (input_buffer != NULL && TEST_BITS(found_listener->flags,
  LISTENER_SPRITEAREAS) && message->data.data_load.file_type == FILETYPE_SPRITE)
  {
    DEBUGF("Loader: Pre-pending sprite area size\n");
    spriteareaheader **area = (spriteareaheader **)&input_buffer;
    if (!flex_midextend(&input_buffer, 0, sizeof((*area)->size))) {
      /* Failed to extend input buffer at start */
      WARN_GLOB("NoMem");
      free(full_path);
      flex_free(&input_buffer);
      return 1; /* claim message */
    }
    /* Write sprite area size as first word */
    (*area)->size = flex_size(&input_buffer);
  }

  /* File loaded successfully - call finished handler */
  if (found_listener->finished_method != NULL) {
    DEBUGF("Loader: Calling client function with handle %p\n",
          found_listener->client_handle);
    found_listener->finished_method(drop_x, drop_y, full_path, true,
                                    &input_buffer, file_type,
                                    found_listener->client_handle);
  } else {
    DEBUGF("Loader: Listener has no client function\n");
    if (input_buffer != NULL)
      flex_free(&input_buffer);
  }
  free(full_path);
  return 1; /* claim message */
}

/* -----------------------------------------------------------------------
                         Miscellaneous internal functions
*/

static void _ldr_kill_listener(LoaderListenerBlk *kill_listener)
{
  DEBUGF("Loader: Killing listener %p\n", (void *)kill_listener);

  /* Cancel any outstanding load operations started by the dying listener. */
  DEBUGF("Loader: Cancelling outstanding load operations\n");
  linkedlist_for_each(&extra_op_data_list, _ldr_cancel_matching_op, kill_listener);

  /* De-link Listener from list */
  linkedlist_remove(&listener_list, &kill_listener->list_item);

#ifdef COPY_ARRAY_ARGS
  /* Remove array of gadget ids */
  DEBUGF("Loader: Freeing component IDs array at %p\n", (void *)kill_listener->criteria.gadgets);
  free((void *)kill_listener->criteria.gadgets); /* cast away const-qualifier */
#endif

  /* Destroy it */
  free(kill_listener);
}

/* ----------------------------------------------------------------------- */

static LoaderListenerBlk *_ldr_find_broadcast_listener(const char *file_path, int file_type)
{
  /* Search linked list to find if any Listeners are willing to start a thread
     to load this file type */
  LoaderListenerBlk *scan_list;

  for (scan_list = (LoaderListenerBlk *)linkedlist_get_head(&listener_list);
       scan_list != NULL;
       scan_list = (LoaderListenerBlk *)linkedlist_get_next(&scan_list->list_item))
  {
    if (TEST_BITS(scan_list->flags, LISTENER_CLAIM))
    {
      if (TEST_BITS(scan_list->flags, LISTENER_FILTER))
      {
        loader_pf_result act = scan_list->criteria.pre_filter.filter_method(file_path,
                               file_type, true, scan_list->client_handle);
        switch (act)
        {
          case LOADER_PREFILTER_REJECT:
            return PREVENT_TRANSFER; /* reject data transfer request */

          case LOADER_PREFILTER_CLAIM:
            return scan_list; /* found a willing listener */

          default:
            break; /* Assume LOADER_PREFILTER_IGNORE or ..._BADTYPE */
        }
      }
      else
      {
        if (scan_list->criteria.pre_filter.file_type == file_type ||
            scan_list->criteria.pre_filter.file_type == FILETYPE_ALL)
          return scan_list; /* found a listener */
      }
    }
  }
  return NULL; /* failure */
}

/* ----------------------------------------------------------------------- */

static _Optional LoaderListenerBlk *_ldr_find_listener(LoaderListenerCriteria *criteria)
{
  return (_Optional LoaderListenerBlk *)linkedlist_for_each(
         &listener_list, _ldr_listener_is_match, criteria);
}

/* ----------------------------------------------------------------------- */

static bool _ldr_listener_is_match(LinkedList *list, LinkedListItem *item, void *arg)
{
  LoaderListenerBlk * const listener = (LoaderListenerBlk *)item;
  const LoaderListenerCriteria * const criteria = arg;

  assert(listener != NULL);
  assert(criteria != NULL);
  NOT_USED(list);

#ifdef COPY_ARRAY_ARGS
  if (listener->criteria.object == criteria->object &&
      listener->criteria.pre_filter.file_type == criteria->pre_filter.file_type)
  {
    if (criteria->gadgets == NULL && listener->criteria.gadgets == NULL) {
      DEBUGF("Loader: Matched with gadgetless listener %p\n", (void *)listener);
      return true;
    }

    if (criteria->gadgets != NULL && listener->criteria.gadgets != NULL) {
      /* Scan arrays until a pair of gadget Ids does not match */
      for (unsigned int i = 0; criteria->gadgets[i] == listener->criteria.gadgets[i]; i++) {
        assert(i < 16); /* not definite but this would suggest a bug */
        DEBUGF("Loader: List item %u is gadget %d\n", i, criteria->gadgets[i]);
        if (criteria->gadgets[i] == NULL_ComponentId) {
          assert(listener->gadgets[i] == NULL_ComponentId);
          DEBUGF("Loader: Matched to end of list for listener %p\n", (void *)listener);
          return true;
        }
      }
    }
  }
  return false;
#else
  return (listener->criteria.object == criteria->object &&
          listener->criteria.pre_filter.file_type == criteria->pre_filter.file_type &&
          listener->criteria.gadgets == criteria->gadgets);
#endif
}

/* ----------------------------------------------------------------------- */

static bool _ldr_check_dropzone(ObjectId object, const ComponentId *gadgets, int window_handle, int icon_number)
{
  /* Check the dropzone for this listener */
  DEBUGF("Loader: Checking whether window %d and icon %d are within drop zone\n",
        window_handle, icon_number);
  DEBUGF("Loader: Object Id is %d and gadget list is at %p\n", object, (void *)gadgets);

  if (object == NULL_ObjectId) {
    DEBUGF("Loader: Match with any object\n");
    return true; /* they don't care which object */
  }

  /* Is it a Window object or an Iconbar object? */
  {
    ObjectClass objclass;
    {
      _Optional _kernel_oserror *errptr;
      errptr=toolbox_get_object_class(0, object, &objclass);
      if (errptr != NULL) {
        if (errptr->errnum != ERR_BAD_OBJECT_ID)
          err_complain(errptr->errnum, errptr->errmess);
        return false; /* ignore listener if bad object ID */
      }
    }
    DEBUGF("Loader: Class of object %d is &%X\n", object, objclass);

    if (objclass == Iconbar_ObjectClass) {
      /* It is an Iconbar object */
      DEBUGF("Loader: It is an Iconbar object\n");
      if (window_handle != -2)
        return false; /* not iconbar drop */

      {
        int ls_iconh;
        ON_ERR_RPT_RTN_V(iconbar_get_icon_handle(0, object, &ls_iconh), false);
        if (icon_number != ls_iconh)
          return false; /* wrong icon */
      }
      return true; /* drop on right iconbar icon */
    }
  }

  /* It is a Window object - get Wimp window handle */
  {
    int ls_windowh;
    ON_ERR_RPT_RTN_V(window_get_wimp_handle(0, object, &ls_windowh), false);
    DEBUGF("Loader: Wimp handle of Window object is %d\n", ls_windowh);
    if (window_handle != ls_windowh)
      return false; /* wrong window handle */
  }
  DEBUGF("Loader: Match with Window object\n");

  if (gadgets == NULL) {
    DEBUGF("Loader: Match with any gadget\n");
    return true; /* they don't care what gadget */
  }

  for (int j = 0; gadgets[j] != NULL_ComponentId; j++) {
    DEBUGF("Loader: Gadget %d is at %d in list\n", gadgets[j], j);

    /* get list size */
    int nbytes;
    _Optional _kernel_oserror *errptr = gadget_get_icon_list(0, object, gadgets[j], NULL,
                              0, &nbytes);
    if (errptr != NULL) {
      /* ignore gadget if bad gadget ID */
      if (errptr->errnum != ERR_BAD_COMPONENT_ID)
        err_complain(errptr->errnum, errptr->errmess);
    }
    else {
      /* allocate buffer for list */
      int *icons_list = malloc(nbytes);
      if (icons_list == NULL) {
        WARN_GLOB("NoMem");
        return false;
      }

      /* read icon numbers into buffer */
      if (E(gadget_get_icon_list(0, object, gadgets[j], icons_list, nbytes,
      &nbytes))) {
        free(icons_list);
        return false;
      }
      for (size_t i = 0; i < nbytes / sizeof(icons_list[0]); i++) {
        DEBUGF("Loader: Icon %zu of gadget %d has handle %d\n", i, gadgets[j],
              icons_list[i]);
        if (icon_number == icons_list[i]) {
          free(icons_list);
          DEBUGF("Loader: Matched icon handle with gadget\n");
          return true; /* dropped in right place */
        }
      }
      free(icons_list);
    }
  }
  DEBUGF("Loader: No match with gadget\n");
  return false; /* none of those icons */
}

/* ----------------------------------------------------------------------- */

static LoaderListenerBlk *_ldr_find_suitable_listener(const char *perma_file_path, int window, int icon, int file_type)
{
  /* Search linked list to find if any Listeners are willing to start a thread
     to load this drop zone/file type/source. If the incoming data transfer is
     direct from another application then perma_file_path will be NULL.
     Returns a) Pointer to listener data
          or b) NULL (meaning no listener found)
          or c) PREVENT_TRANSFER (meaning neither request nor load any data)
      */
  DEBUGF("Loader: searching for listener willing to accept file of type &%X "
         "dropped on window %d and icon %d\n", file_type, window, icon);

  bool file_type_handled = false, dropzone_handled = false;
  {
    LoaderListenerBlk *scan_list;

    for (scan_list = (LoaderListenerBlk *)linkedlist_get_head(&listener_list);
         scan_list != NULL;
         scan_list = (LoaderListenerBlk *)linkedlist_get_next(&scan_list->list_item))
    {
      bool good_dropzone = _ldr_check_dropzone(scan_list->criteria.object,
                           scan_list->criteria.gadgets, window, icon);
      DEBUGF("Loader: It is%s within the drop zone for listener %p\n",
             good_dropzone ? "" : " not", (void *)scan_list);

      if (TEST_BITS(scan_list->flags, LISTENER_FILTER))
      {
        /* Use client supplied function to determine whether file name & type
           are acceptable */
        DEBUGF("Loader: Calling client filter function with handle %p\n",
               scan_list->client_handle);

        loader_pf_result act = scan_list->criteria.pre_filter.filter_method(
                               perma_file_path, file_type, good_dropzone,
                               scan_list->client_handle);
        switch (act)
        {
          case LOADER_PREFILTER_REJECT:
            return PREVENT_TRANSFER; /* reject data transfer request */

          case LOADER_PREFILTER_CLAIM:
            return scan_list; /* found a willing listener */

          case LOADER_PREFILTER_IGNORE:
            file_type_handled = true;
            break;

          default:
            break; /* Assume LOADER_PREFILTER_BADTYPE */
        }
      }
      else
      {
        /* Check the file type accepted by this listener */
        DEBUGF("Loader: No client filter function\n");
        if (file_type == scan_list->criteria.pre_filter.file_type ||
            scan_list->criteria.pre_filter.file_type == FILETYPE_ALL)
        {
          DEBUGF("Loader: Listener %p can handle this file type\n", (void *)scan_list);
          file_type_handled = true;

          if (good_dropzone)
          {
            /* We have found a listener for the dropzone */
            if (!TEST_BITS(scan_list->flags, LISTENER_FILEONLY) ||
                perma_file_path != NULL)
            {
              /* Permanent file or else willing to accept direct transfer */
              DEBUGF("Loader: Will use listener %p\n", (void *)scan_list);
              return scan_list; /* have found suitable listener */
            }

            /* Not willing to accept file from another application */
            if (!TEST_BITS(control_flags, LOADER_QUIET_NOTPERM))
              WARN("FileNotPerm");

            DEBUGF("Loader: Listener requires permanent file path\n");
            return NULL; /* suitable listener not found */
          }
        }
        else
        {
          DEBUGF("Loader: Listener %p can't handle this file type\n", (void *)scan_list);
        }
      }
      if (good_dropzone)
        dropzone_handled = true; /* At least one listener likes it */
    }
  }

  DEBUGF("Loader: No suitable listener found\n");

  /* Oh dear, what went wrong?... */
  if (!file_type_handled)
  {
    if (!TEST_BITS(control_flags,LOADER_QUIET_BADTYPE))
      WARN("NoSuchFileType");
    return NULL; /* suitable listener not found */
  }

  /* -> There exist zones that handle that file type */
  if (!dropzone_handled)
  {
    if (!TEST_BITS(control_flags, LOADER_QUIET_BADDROP))
      WARN("NoSuchDropZone");
    return NULL; /* suitable listener not found */
  }

  /* -> File dropped on valid zone but not necessarily for that file type */
  if (!TEST_BITS(control_flags, LOADER_QUIET_BADDROP))
    WARN("WrongZone");

  return NULL; /* suitable listener not found */
}

/* ----------------------------------------------------------------------- */

static void _ldr_finished(_Optional CONST _kernel_oserror *load_error, int file_type, flex_ptr buffer, void *client_handle)
{
  /* This function is called when a load operation has finished
     (whether successful or not) */
  ExtraOpData *extra_op_data = client_handle;
  LoaderListenerBlk *parent_listener = extra_op_data->listener;

  assert(file_type == -1 || load_error == NULL);

  DEBUGF("Loader: Load operation %p finished %ssuccessfully\n", (void *)extra_op_data,
        file_type == -1 ? "un" : "");

  if (buffer != NULL)
    DEBUGF("Loader: Current address of flex block is %p\n", *buffer);

  if (file_type != -1) {
    if (parent_listener->finished_method != NULL) {
      /* Call the client-supplied function to notify it that the load
         operation is complete. */
      DEBUGF("Loader: Calling client function with handle %p\n",
            parent_listener->client_handle);
      parent_listener->finished_method(extra_op_data->drop_x,
                                       extra_op_data->drop_y,
                                       extra_op_data->leaf_name,
                                       false,
                                       buffer,
                                       file_type,
                                       parent_listener->client_handle);
    } else {
      DEBUGF("Loader: listener has no client function\n");
    }
  } else {
    DEBUGF("Loader: Load operation failed\n");
    if (load_error != NULL) {
      err_report(load_error->errnum, msgs_lookupsubn("LoadFail", 1,
                 load_error->errmess));
    }
    if (buffer != NULL)
      flex_free(buffer);
  }

  /* Free data block for this request and de-link it from the list*/
  _ldr_destroy_extra_op(extra_op_data);
}

/* ----------------------------------------------------------------------- */

static void _ldr_destroy_extra_op(ExtraOpData *extra_op_data)
{
  DEBUGF("Loader: Removing record of load operation %p\n", (void *)extra_op_data);
  assert(extra_op_data != NULL);

  linkedlist_remove(&extra_op_data_list, &extra_op_data->list_item);
  free(extra_op_data->leaf_name);
  free(extra_op_data);
}

/* ----------------------------------------------------------------------- */

static _Optional CONST _kernel_oserror *_ldr_abs_to_work_area(int window_handle, int *x, int *y)
{
  WimpGetWindowStateBlock state;
  state.window_handle = window_handle;
  ON_ERR_RTN_E(wimp_get_window_state(&state));

  if (x != NULL)
    *x -= state.visible_area.xmin - state.xscroll;

  if (y != NULL)
    *y -= state.visible_area.ymax - state.yscroll;

  return NULL; /* success */
}

/* ----------------------------------------------------------------------- */

static bool _ldr_cancel_matching_op(LinkedList *list, LinkedListItem *item, void *arg)
{
  ExtraOpData * const extra_op_data = (ExtraOpData *)item;
  const LoaderListenerBlk * const kill_listener = arg;

  assert(extra_op_data != NULL);
  assert(kill_listener != NULL);
  NOT_USED(list);

  /* Check whether this load operation belongs to the specified listener. */
  if (extra_op_data->listener == kill_listener)
  {
    /* A callback to _ldr_finished() will invalidate 'extra_op_data' */
    loader2_cancel_receives(_ldr_finished, extra_op_data);
  }

  return false; /* next item */
}

/* ----------------------------------------------------------------------- */

static bool _ldr_kill_listener_for_object(LinkedList *list, LinkedListItem *item, void *arg)
{
  LoaderListenerBlk * const listener = (LoaderListenerBlk *)item;
  const ObjectId * const object_id = arg;

  assert(listener != NULL);
  NOT_USED(list);

  /* Kill the listener if it belongs to a given Toolbox object.
     NULL instead of a pointer to an object ID means kill all listeners. */
  if (object_id == NULL || listener->criteria.object == *object_id)
    _ldr_kill_listener(listener);

  return false; /* next item */
}

#else /* CBLIB_OBSOLETE */
#error Source file Loader.c is deprecated
#endif /* CBLIB_OBSOLETE */
