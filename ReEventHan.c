/*
 * CBLibrary: Deregister event handlers after deletion of a Toolbox object
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
  CJB: 07-Apr-05: Copied function remove_all_handlers() from source of SFeditor
                  and changed return value to claim event.
  CJB: 15-May-05: Renamed this file (was 'c.EventExtra').
  CJB: 15-Apr-07: Object ID is now shown as hexadecimal in debugging output.
                  Guard against wiping the recorded object ID where this differs
                  from that associated with the Toolbox event.
  CJB: 23-Dec-14: Apply Event library function calls.
  CJB: 18-Apr-16: Passed a pointer parameter of type void * to match %p.
  CJB: 01-Nov-18: Replaced DEBUG macro usage with DEBUGF.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
 */

/* Acorn C/C++ library headers */
#include "event.h"

/* Local headers */
#include "Err.h"
#include "EventExtra.h"
#include "Internal/CBMisc.h"

/* ----------------------------------------------------------------------- */
/*                         Public functions                                */

int remove_event_handlers_reset_id(int event_code, ToolboxEvent *event, IdBlock *id_block, void *handle)
{
  /* Generic event handler for deregistering any event handlers for the object
     in question and optionally also resetting a stored ObjectId (to which
     'handle' points) */
  ObjectId *recorded_id = handle;
  NOT_USED(event);
  NOT_USED(event_code);

  DEBUGF("Removing all event handlers for object 0x%x\n", id_block->self_id);

  ON_ERR_RPT(event_deregister_wimp_handlers_for_object(id_block->self_id));
  ON_ERR_RPT(event_deregister_toolbox_handlers_for_object(id_block->self_id));

  if (recorded_id != NULL)
  {
    /* If the recorded object ID doesn't match the event then we mustn't reset
       it (because it refers to some other object). We could make this an
       assertion, but that would prevent reuse of the storage location for a
       object ID until after delivery of an Toolbox_ObjectDeleted event. */
    if (*recorded_id != id_block->self_id)
    {
      DEBUGF("Will not wipe stored object ID 0x%x (doesn't match ID block)\n",
            *recorded_id);
    }
    else
    {
      DEBUGF("Wiping object id 0x%x stored at %p\n", *recorded_id, handle);
      *recorded_id = NULL_ObjectId;
    }
  }

  return 1; /* claim event */
}
