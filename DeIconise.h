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

/* DeIconise.h declares two functions that can be used to ensure that any
   iconiser application (e.g. Pinboard) is kept up to date when windows are
   opened or closed.

Dependencies: Acorn library kernel, Acorn's WIMP & toolbox libraries.
Message tokens: None.
History:
  CJB: 02-Jul-05: Created this header.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 11-Dec-20: Deleted redundant uses of the 'extern' keyword.
*/

#ifndef DeIconise_h
#define DeIconise_h

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "toolbox.h"

/* Local headers */
#include "Macros.h"

CONST _kernel_oserror *DeIconise_hide_object(unsigned int /*flags*/, ObjectId /*id*/);
   /*
    * If Toolbox object 'id' is currently showing then this function hides it
    * and broadcasts message &400CB to signal that any iconised representation
    * should be removed. The arguments are identical to toolbox_hide_object -
    * for details see the Toolbox documentation. No message is broadcast if the
    * object is not implemented using a Wimp window.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

CONST _kernel_oserror *DeIconise_show_object(unsigned int /*flags*/, ObjectId /*id*/, int /*show_type*/, void */*type*/, ObjectId /*parent*/, ComponentId /*parent_component*/);
   /*
    * Shows Toolbox object 'id' before broadcasting message &400CB to signal
    * that any iconised representation of that window should be removed. The
    * arguments are identical to toolbox_show_object - for details see the
    * Toolbox documentation. No message is broadcast if the object is not
    * implemented using a Wimp window or it was formerly hidden.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

#endif
