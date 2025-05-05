/*
 * CBLibrary: Hide or show a Toolbox gadget by moving it out of sight
 * Copyright (C) 2009 Christopher Bazley
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
  CJB: 23-Aug-09: Adapted from separate hide/show gadget functions in my
                  FednetCmp and SFToSpr applications.
  CJB: 03-Apr-16: Added brackets to avoid GNU C compiler warnings.
  CJB: 01-Nov-18: Replaced DEBUG macro usage with DEBUGF.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
 */

/* ISO library headers */
#include <stdbool.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "toolbox.h"
#include "window.h"
#include "gadgets.h"
#include "wimp.h"

/* Local headers */
#include "GadgetUtil.h"
#include "Internal/CBMisc.h"

/* ----------------------------------------------------------------------- */
/*                         Public functions                                */

_Optional CONST _kernel_oserror *set_gadget_hidden(ObjectId    window,
                                                   ComponentId gadget,
                                                   bool        hide)
{
  BBox pos, extent;
  int bottom_of_gadget, top_of_work_area;

  /* Rather than assuming that the work area coordinate origin (0,0) is at
     the top left corner, it is safer to read the window's extent */
  ON_ERR_RTN_E(window_get_extent(0, window, &extent));

  /* Get the bounding box of the gadget to be hidden, in work area coordinates
   */
  ON_ERR_RTN_E(gadget_get_bbox(0, window, gadget, &pos));

  /* Can't hide a gadget that is already hidden or show a gadget that is
     already showing */
  bottom_of_gadget = pos.ymin;
  top_of_work_area = extent.ymax;
  if ((hide && bottom_of_gadget < top_of_work_area) ||
      (!hide && bottom_of_gadget > top_of_work_area))
  {
    /* Move the gadget above the window's work area extent */
    int new_ymin, new_ymax;

    new_ymin = top_of_work_area + (top_of_work_area - bottom_of_gadget);
    new_ymax = new_ymin + (pos.ymax - bottom_of_gadget);

    DEBUGF("GadgetHide: Moving component 0x%x of window 0x%x vertically "
          "from %d...%d to %d...%d\n",
          gadget, window, pos.ymin, pos.ymax, new_ymin, new_ymax);

    pos.ymin = new_ymin;
    pos.ymax = new_ymax;
    ON_ERR_RTN_E(gadget_move_gadget(0, window, gadget, &pos));
  }
  else
  {
    DEBUGF("GadgetHide: Component 0x%x of window 0x%x is already %s\n",
          gadget, window, hide ? "hidden" : "showing");
  }

  return NULL; /* success */
}
