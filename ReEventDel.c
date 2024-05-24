/*
 * CBLibrary: Deregister event handlers and delete a Toolbox object
 * Copyright (C) 2020 Christopher Bazley
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
  CJB: 11-Dec-20: Created this source file.
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

CONST _kernel_oserror *remove_event_handlers_delete(ObjectId const object_id)
{
  assert(object_id != NULL_ObjectId);
  DEBUGF("Removing all event handlers and deleting object 0x%x\n",
         object_id);

  CONST _kernel_oserror *e = event_deregister_wimp_handlers_for_object(
                               object_id);
  MERGE_ERR(e, event_deregister_toolbox_handlers_for_object(object_id));
  MERGE_ERR(e, toolbox_delete_object(0, object_id));
  return e;
}
