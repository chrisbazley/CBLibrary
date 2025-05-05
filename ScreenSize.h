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

/* ScreenSize.h declares a function to get the dimensions of the current
   screen mode, in OS units

Dependencies: Acorn library kernel.
Message tokens: None.
History:
  CJB: 23-Aug-09: Created this header file from scratch.
  CJB: 11-Dec-20: Deleted redundant uses of the 'extern' keyword.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
*/

#ifndef ScreenSize_h
#define ScreenSize_h

/* Acorn C/C++ library headers */
#include "kernel.h"

/* Local headers */
#include "Macros.h"

#if !defined(USE_OPTIONAL) && !defined(_Optional)
#define _Optional
#endif

_Optional CONST _kernel_oserror *get_screen_size(int *width, int *height);
   /*
    * Gets the width and/or height of the current screen mode, in OS units.
    * Either or both of the arguments may be NULL, in which case the
    * corresponding dimension(s) will not be output.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

#endif
