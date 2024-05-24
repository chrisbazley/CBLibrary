/*
 * CBLibrary: Toolbox event handler utility functions
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

/* EventExtra.h declares utility functions for use in programs that use
   Acorn's toolbox and event libraries.

Dependencies: Acorn's event library.
Message tokens: None.
History:
  CJB: 07-Apr-05: Created this header.
  CJB: 15-May-05: Added declaration of delete_object_on_event.
  CJB: 11-Dec-20: Deleted redundant uses of the 'extern' keyword.
  CJB: 20-Dec-20: Added declaration of remove_event_handlers_delete.
 */

#ifndef EventExtra_h
#define EventExtra_h

/* Acorn C/C++ library headers */
#include "toolbox.h"
#include "event.h"

/* Local headers */
#include "Macros.h"

ToolboxEventHandler remove_event_handlers_reset_id;
   /*
    * Removes any Toolbox and Wimp event handlers that have been registered
    * for the object specified by the self_id field of the passed id block.
    * Additionally, if the passed client handle is non-NULL then it will be
    * treated as a pointer to an ObjectId to be set to NULL_ObjectId.
    * Normal usage would be to register this function as a handler for
    * Toolbox_ObjectDeleted events on an object that requires minimal tidying
    * up upon deletion.
    * Returns: 1 (i.e. claim event).
    */


ToolboxEventHandler delete_object_on_event;
   /*
    * Deletes the object specified by the self_id field of the passed id block.
    * Normal usage would be to register this function as a handler for some
    * Toolbox event that should always result in deletion of the object in
    * question; for example SaveAs_DialogueCompleted or Window_HasBeenHidden.
    * Any associated tidying up must then be done on subsequent receipt of a
    * Toolbox_ObjectDeleted event.
    * Returns: 1 (i.e. claim event).
    */

CONST _kernel_oserror *remove_event_handlers_delete(ObjectId object_id);
   /*
    * Removes any Toolbox and Wimp event handlers that have been registered
    * for the object specified by the passed object_id, then deletes the
    * object.
    */

#endif
