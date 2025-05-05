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

/* InputFocus.h declares several functions that can be used to work around a
   deficiency in the Toolbox or Wimp: The input focus is not automatically
   restored after a Window object with default input focus in its background
   has been shown as a transient dialogue box and then hidden.

Dependencies: Acorn's WIMP, toolbox and event libraries.
Message tokens: None.
History:
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 25-Oct-06: Added proper clib-style documentation.
  CJB: 11-Dec-20: Deleted redundant uses of the 'extern' keyword.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
*/

#ifndef InputFocus_h
#define InputFocus_h

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "event.h"

/* Local headers */
#include "Macros.h"

#if !defined(USE_OPTIONAL) && !defined(_Optional)
#define _Optional
#endif

_Optional CONST _kernel_oserror *InputFocus_initialise(void);
   /*
    * Initialises the InputFocus component by registering a handler for
    * MenusDeleted messages.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

ToolboxEventHandler InputFocus_recordcaretpos;
   /*
    * You should register this event handler to be called when your task
    * receives a Window_AboutToBeShown, DCS_AboutToBeShown or
    * Quit_AboutToBeShown event. Currently no other object classes are
    * supported. If the event code is recognised and the object is about to be
    * be shown transiently then this function will record the current position
    * of the Wimp caret.
    * Returns: 0 (event not claimed)
    */

_Optional CONST _kernel_oserror *InputFocus_restorecaret(void);
   /*
    * If no window owns the caret then this function will attempt to restore the
    * caret position recorded by InputFocus_recordcaretpos(). No error will be
    * returned if the caret cannot be restored. You may wish to call this
    * function directly if you have a transient window open and your task is
    * about to quit.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

#endif
