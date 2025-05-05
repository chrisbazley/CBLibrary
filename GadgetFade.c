/*
 * CBLibrary: Fade or unfade a Toolbox gadget
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
  CJB: 24-Aug-09: Adapted from a similarly-named function that was common to
                  several of my applications.
  CJB: 31-Aug-15: Replaced 'const' in the function definition with the macro
                  CONST to ensure that it matches the function declaration.
  CJB: 23-May-21: Added debugging output.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
 */

/* ISO library headers */
#include <stdbool.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "toolbox.h"
#include "gadgets.h"

/* Local headers */
#include "GadgetUtil.h"
#include "Internal/CBMisc.h"

/* ----------------------------------------------------------------------- */
/*                         Public functions                                */

_Optional CONST _kernel_oserror *set_gadget_faded(ObjectId    window,
                                                  ComponentId gadget,
                                                  bool        fade)
{
  unsigned int flags;

  ON_ERR_RTN_E(gadget_get_flags(0, window, gadget, &flags));

  if (fade)
  {
    if (TEST_BITS(flags, Gadget_Faded))
      return NULL; /* success */

    flags |= Gadget_Faded;
    DEBUGF("Disabling gadget 0x%x in window 0x%x\n", gadget, window);
  }
  else
  {
    if (!TEST_BITS(flags, Gadget_Faded))
      return NULL; /* success */

    flags &= ~Gadget_Faded;
    DEBUGF("Enabling gadget 0x%x in window 0x%x\n", gadget, window);
  }

  return gadget_set_flags(0, window, gadget, flags);
}
