/*
 * CBLibrary: Macros relating to bugs in the Acorn Toolbox
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

/* Build switches for bugs in the Toolbox, and a macro */

#ifndef TboxBugs_h
#define TboxBugs_h

/*
  Current versions of the SaveAs object have a bug where the dbox is
  closed on a successful save regardless of whether the user clicked
  the 'Save' button with ADJUST.
  Given this, there is little point in including code to update the
  'default' dbox state (used to reset the dbox when 'Cancel' is
  ADJUST-clicked)
*/
#define SAVEAS_CRAP

/*
  There is a bug concerning RISC OS 4 !Help (a toolbox client) and
  delivery of Window_HasBeenHidden events for transient windows. The
  event for our window, which should be sent on receipt of the Wimp's
  MenusDeleted message, is instead sent prematurely to !Help - with our
  window's ObjectId!
  In other words, we can't rely on receiving Window_HasBeenHidden events
  for transient windows.
*/
#define HELP_CRAP

/*
  The input focus is not automatically restored when
  background-input-focus transient windows are closed.
*/
#define FOCUS_CRAP

/*
  One of the Window SWI veneer functions is missing from Castle's build
  of the toolbox veneers library.
*/
#include "swis.h"
#include "toolbox.h"
#include "window.h"
#define window_set_help_message2(flags, window, message_text) \
  _swix( \
     Toolbox_ObjectMiscOp, \
     _INR(0,3), \
     flags, \
     window, \
     Window_SetHelpMessage, \
     message_text \
   )

#endif
