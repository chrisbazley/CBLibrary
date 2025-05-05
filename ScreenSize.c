/*
 * CBLibrary: Get the dimensions of the current screen mode
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
  CJB: 23-Aug-09: Adapted from the getscreencentre function common to both
                  SFToSpr and FednetCmp.
  CJB: 04-Oct-09: Updated to use the new library function os_read_vdu_variables
                  and named constants instead of 'magic' values. Fixed a bug
                  where returned dimensions were too short by one pixel.
  CJB: 11-Dec-20: Prefer to declare variable with initializer.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
 */

/* ISO library headers */
#include <stddef.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "swis.h"

/* CBOSLib headers */
#include "OSVDU.h"

/* Local headers */
#include "ScreenSize.h"
#include "Internal/CBMisc.h"

/* ----------------------------------------------------------------------- */
/*                         Public functions                                */

_Optional CONST _kernel_oserror *get_screen_size(int *width, int *height)
{
  /* Keep this enumeration synchronised with mode_vars[] */
  enum
  {
    VarIndex_XWindLimit,
    VarIndex_YWindLimit,
    VarIndex_XEigFactor,
    VarIndex_YEigFactor,
    VarIndex_LAST
  };
  /* Keep this array synchronised with the enumeration above */
  static const VDUVar mode_vars[VarIndex_LAST + 1] =
  {
    (VDUVar)ModeVar_XWindLimit,
    (VDUVar)ModeVar_YWindLimit,
    (VDUVar)ModeVar_XEigFactor,
    (VDUVar)ModeVar_YEigFactor,
    VDUVar_EndOfList
  };
  int var_vals[VarIndex_LAST];

  _Optional CONST _kernel_oserror *e = os_read_vdu_variables(mode_vars, var_vals);
  if (e == NULL)
  {
    /* Convert screen dimensions to external graphics units */
    if (width != NULL)
    {
      *width = (var_vals[VarIndex_XWindLimit] + 1) <<
                var_vals[VarIndex_XEigFactor];
    }
    if (height != NULL)
    {
      *height = (var_vals[VarIndex_YWindLimit] + 1) <<
                 var_vals[VarIndex_YEigFactor];
    }
  }
  return e;
}
