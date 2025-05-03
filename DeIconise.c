/*
 * CBLibrary: Show/hide windows whilst keeping any iconiser up-to-date
 * Copyright (C) 2005 Christopher Bazley
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
  CJB: 02-Jul-05: Created this file based on SFskyedit's hide_deiconise()
                  function and ViewsMenu_show_object().
  CJB: 07-Aug-05: Substituted sizeof(msg_block.data.words[0]) for sizeof(int)
                  in calculation of message block size.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 09-Sep-09: Stop using reserved identifier '_DeIconise_notify' (starts
                  with an underscore followed by a capital letter).
  CJB: 23-Dec-14: Apply Fortify to Wimp & Toolbox library function calls.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 17-Jul-22: Deleted a bad assertion in notify_closed (failed for
                  unknown UI object classes, including Menu).
  CJB: 03-May-25: Fix #include filename case.
 */


/* ISO library headers */
#include <stddef.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "wimplib.h"
#include "wimp.h"
#include "toolbox.h"
#include "saveas.h"
#include "proginfo.h"
#include "scale.h"
#include "fontdbox.h"
#include "quit.h"
#include "dcs.h"
#include "printdbox.h"
#include "colourdbox.h"
#include "fileinfo.h"
#include "window.h"

/* Local headers */
#include "Internal/CBMisc.h"
#include "Err.h"
#include "msgtrans.h"
#include "DeIconise.h"

/* ----------------------------------------------------------------------- */
/*                       Function prototypes                               */

static CONST _kernel_oserror *notify_closed(ObjectId id);

/* ----------------------------------------------------------------------- */
/*                         Public functions                                */

CONST _kernel_oserror *DeIconise_hide_object(unsigned int flags, ObjectId id)
{
  unsigned int state;

  /* First check that window object is actually showing */
  ON_ERR_RTN_E(toolbox_get_object_state(0, id, &state));
  if (!TEST_BITS(state, Toolbox_GetObjectState_Showing))
    return NULL; /* nothing to do */

  /* Hide given object */
  ON_ERR_RTN_E(toolbox_hide_object(flags, id));

  /* Notify iconiser that any representation of window should be removed */
  return notify_closed(id);
}

/* ----------------------------------------------------------------------- */

CONST _kernel_oserror *DeIconise_show_object(unsigned int flags, ObjectId id, int show_type, void *type, ObjectId parent, ComponentId parent_component)
{
  /* First check whether window object is hidden */
  unsigned int state;
  ON_ERR_RTN_E(toolbox_get_object_state(0, id, &state));

  /* Show given object on screen using specified method */
  ON_ERR_RTN_E(toolbox_show_object(flags,
                                   id,
                                   show_type,
                                   type,
                                   parent,
                                   parent_component));

  if (TEST_BITS(state, Toolbox_GetObjectState_Showing))
  {
    /* If window was not formerly hidden then it may have been iconised -
       notify iconiser that any representation should be removed */
    return notify_closed(id);
  }
  else
  {
    return NULL; /* success */
  }
}

/* ----------------------------------------------------------------------- */
/*                         Private functions                               */

static CONST _kernel_oserror *notify_closed(ObjectId id)
{
  WimpMessage msg_block;
  ObjectClass obj_class;
  CONST _kernel_oserror *e;

  /* Determine toolbox ID of underlying Window object (if any) */
  ON_ERR_RTN_E(toolbox_get_object_class(0, id, &obj_class));

  switch (obj_class)
  {
    case Window_ObjectClass:
      e = NULL;
      break;

    case SaveAs_ObjectClass:
      e = saveas_get_window_id(0, id, &id);
      break;

    case FileInfo_ObjectClass:
      e = fileinfo_get_window_id(0, id, &id);
      break;

    case ProgInfo_ObjectClass:
      e = proginfo_get_window_id(0, id, &id);
      break;

    case Scale_ObjectClass:
      e = scale_get_window_id(0, id, &id);
      break;

    case FontDbox_ObjectClass:
      e = fontdbox_get_window_id(0, id, &id);
      break;

    case Quit_ObjectClass:
      e = quit_get_window_id(0, id, &id);
      break;

    case DCS_ObjectClass:
      e = dcs_get_window_id(0, id, &id);
      break;

    case PrintDbox_ObjectClass:
      e = printdbox_get_window_id(0, id, &id);
      break;

    case ColourDbox_ObjectClass:
      /* Underlying window is not controlled by the toolbox */
      e = colourdbox_get_wimp_handle(0, id, &msg_block.data.words[0]);
      break;

    default:
      return NULL; /* unknown class of object */
  }
  ON_ERR_RTN_E(e);

  /* Get wimp handle of underlying window */
  if (obj_class != ColourDbox_ObjectClass)
    ON_ERR_RTN_E(window_get_wimp_handle(0, id, &msg_block.data.words[0]));

  /* Broadcast message &400CB (window closed) in case it was iconised */
  msg_block.hdr.size = sizeof(msg_block.hdr) + sizeof(msg_block.data.words[0]);
  msg_block.hdr.your_ref = 0;
  msg_block.hdr.action_code = Wimp_MWindowClosed;
  return wimp_send_message(Wimp_EUserMessage, &msg_block, 0, 0, NULL);
}
