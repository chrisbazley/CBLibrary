/*
 * CBLibrary: Utility functions relating to Toolbox gadgets
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

/* GadgetUtil.h declares generic functions relating to Toolbox gadgets

Dependencies: Acorn's toolbox library.
Message tokens: None.
History:
  CJB: 23-Aug-09: Created this header file from scratch.
  CJB: 11-Dec-20: Deleted redundant uses of the 'extern' keyword.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
*/

#ifndef GadgetUtil_h
#define GadgetUtil_h

/* ISO library headers */
#include <stdbool.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "toolbox.h"

/* Local headers */
#include "Macros.h"

#if !defined(USE_OPTIONAL) && !defined(_Optional)
#define _Optional
#endif

_Optional CONST _kernel_oserror *set_gadget_hidden(ObjectId    window,
                                                   ComponentId gadget,
                                                   bool        hide);
   /*
    * Show or hide a specified gadget in a given window by moving it below or
    * above the upper extent of the window's work area, unless it is already in
    * the requested state.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

_Optional CONST _kernel_oserror *set_gadget_faded(ObjectId    window,
                                                  ComponentId gadget,
                                                  bool        fade);
   /*
    * Fade or unfade a specified gadget in a given window, unless it is already
    * in the requested state.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

#endif
