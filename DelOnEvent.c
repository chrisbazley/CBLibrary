/*
 * CBLibrary: Delete an object for which a Toolbox event was delivered
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
  CJB: 15-May-05: Copied function delete_object_handler() from source of
                  SFtoSpr/FednetCmp and renamed it as below.
  CJB: 15-Apr-07: Object ID is now shown as hexadecimal in debugging output.
  CJB: 23-Dec-14: Apply Fortify to Toolbox library function calls.
  CJB: 01-Nov-18: Replaced DEBUG macro usage with DEBUGF.
 */

/* Acorn C/C++ library headers */
#include "event.h"
#include "toolbox.h"

/* Local headers */
#include "Internal/CBMisc.h"
#include "Err.h"
#include "EventExtra.h"

/* ----------------------------------------------------------------------- */
/*                         Public functions                                */

int delete_object_on_event(int event_code, ToolboxEvent *event, IdBlock *id_block, void *handle)
{
  /* Generic event handler for deleting the object in question */
  NOT_USED(event);
  NOT_USED(handle);

  DEBUGF("Deleting object 0x%x in response to event 0x%x\n", id_block->self_id,
        event_code);

  ON_ERR_RPT(toolbox_delete_object(0, id_block->self_id));

  return 1; /* claim event */
}
