/*
 * CBLibrary: Restore input focus when a transient dialogue box is hidden
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
  CJB: 07-Mar-04: Updated to use the new macro names defined in h.Macros.
  CJB: 13-Jun-04: Because all macro definitions are now expression statements,
                  have changed those invocations which omitted a trailing ';'.
  CJB: 15-Jan-05: Changed to use new DEBUG macro rather than ugly in-line code.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 29-Aug-09: InputFocus_recordcaretpos now gives up if it can't get the
                  window's handle, and no longer records the window handle to
                  be matched with Message_MenusDeleted if it fails to record
                  the caret position.
  CJB: 09-Sep-09: Stop using reserved identifier '_InputFocus_menusdeleted'
                  (starts with an underscore followed by a capital letter).
  CJB: 23-Dec-14: Apply Fortify to Toolbox, Event & Wimp library function calls.
  CJB: 02-Jan-15: Got rid of goto statements.
  CJB: 01-Nov-18: Replaced DEBUG macro usage with DEBUGF.
 */

/* ISO library headers */
#include <stddef.h>

/* Acorn C/C++ library headers */
#include "wimp.h"
#include "wimplib.h"
#include "event.h"
#include "window.h"
#include "menu.h"
#include "DCS.h"
#include "Quit.h"

/* Local headers */
#include "Internal/CBMisc.h"
#include "InputFocus.h"
#include "Err.h"

/* N.B. there can be only one transient dbox open at a time */
static WimpCaret caret_store;
static int window_handle = -1; /* not a valid window handle */

/* ----------------------------------------------------------------------- */
/*                       Function prototypes                               */

static WimpMessageHandler menus_deleted_handler;

/* ----------------------------------------------------------------------- */
/*                         Public functions                                */

CONST _kernel_oserror *InputFocus_initialise(void)
{
  return event_register_message_handler(Wimp_MMenusDeleted,
                                        menus_deleted_handler,
                                        NULL);
}

/* ----------------------------------------------------------------------- */

int InputFocus_recordcaretpos(int event_code, ToolboxEvent *event, IdBlock *id_block, void *handle)
{
  /* Can only handle those classes for which we know how to get window
  handle. Also, the AboutToBeShown event blocks for these 3 have a
  common structure */
  ObjectClass parent_class;
  ObjectId window_id = NULL_ObjectId;
  CONST _kernel_oserror *e = NULL;
  int wh;

  NOT_USED(wh);
  NOT_USED(handle);

  switch (event_code)
  {
    case DCS_AboutToBeShown:
      e = dcs_get_window_id(0, id_block->self_id, &window_id);
      break;

    case Quit_AboutToBeShown:
      e = quit_get_window_id(0, id_block->self_id, &window_id);
      break;

    case Window_AboutToBeShown:
      window_id = id_block->self_id;
      break;
  }

  if (e == NULL && window_id != NULL_ObjectId)
  {
    /* Check that we are being shown transiently - otherwise we might let
       ourselves in for a whole heap of trouble */
    if (TEST_BITS(event->hdr.flags, Toolbox_ShowObject_AsMenu))
    {
      /* Is this dbox hanging off a menu tree? */
      parent_class = 0;
      if (id_block->parent_id != NULL_ObjectId)
      {
        /* parent may have been deleted, so ignore error return */
        (void)toolbox_get_object_class(0, id_block->parent_id, &parent_class);
      }

      if (parent_class == Menu_ObjectClass)
      {
        /* Yes - don't attempt to match window handle */
        wh = -2;
      }
      else
      {
        /* No - record window handle to compare against MenusDeleted message */
        e = window_get_wimp_handle(0, window_id, &wh);
      }

      if (e == NULL)
        e = wimp_get_caret_position(&caret_store);

      if (e == NULL)
      {
        DEBUGF("InputFocus: Recording caret pos: %d, %d, %d, %d, %d, %d "
              "prior to opening window %d (wimp handle %d)\n",
              caret_store.window_handle, caret_store.icon_handle,
              caret_store.xoffset, caret_store.yoffset,
              caret_store.height, caret_store.index,
              id_block->self_id, wh);

        window_handle = wh; /* to be matched with Message_MenusDeleted */
      }
    }
  }

  ON_ERR_RPT(e);
  return 0; /* don't claim event */
}

/* ----------------------------------------------------------------------- */

CONST _kernel_oserror *InputFocus_restorecaret(void)
{
  /* Find current caret position */
  WimpGetCaretPositionBlock now_pos;
  ON_ERR_RTN_E(wimp_get_caret_position(&now_pos));

  DEBUGF("InputFocus: post-close caret pos: %d, %d, %d, %d, %d, %d\n",
        now_pos.window_handle, now_pos.icon_handle,
        now_pos.xoffset, now_pos.yoffset,
        now_pos.height, now_pos.index);

  /* Attempt to restore the input focus (if no window has claimed it)
     Ignore any error, cos it probably means previous owner is dead */
  if (now_pos.window_handle == -1)
    wimp_set_caret_position(caret_store.window_handle,
                            caret_store.icon_handle,
                            caret_store.xoffset,
                            caret_store.yoffset,
                            caret_store.height,
                            caret_store.index);
  return NULL; /* success */
}

/* ----------------------------------------------------------------------- */
/*                         Private functions                               */

int menus_deleted_handler(WimpMessage *message, void *handle)
{
  NOT_USED(handle);

  DEBUGF("InputFocus: Message_MenusDeleted received for %d (our handle %d)\n",
        message->data.words[0], window_handle);

  if (message->data.words[0] == window_handle || window_handle == -2)
  {
    /* Our dbox has just closed or we don't care about handle */
    window_handle = -1; /* not a valid window handle */
    ON_ERR_RPT(InputFocus_restorecaret());
  }

  return 0; /* don't claim event */
}
