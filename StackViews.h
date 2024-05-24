/*
 * CBLibrary: Show a Toolbox object at a position suitable for a new view
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

/* StackViews.h declares two functions and a macro that can be used by
   applications to fulfil the requirements for opening windows set out by
   the RISC OS Style Guide.

Dependencies: Acorn library kernel, Acorn's WIMP & toolbox libraries.
Message tokens: None.
History:
  CJB: 01-Mar-05: Created this header.
  CJB: 04-Mar-05: Added declaration of function StackViews_setdefault.
  CJB: 05-Mar-05: Added clib-style documentation and dependency information.
                  Renamed StackViews_setdefault as StackViews_configure.
  CJB: 28-May-05: Reformatted documentation for 80 column display.
                  Added declaration of function StackViews_open_get_bbox.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 30-Sep-09: Replaced macro value 'STACKVIEWS_AUTO' with an enumerated
                  constant 'StackViewsAuto'.
  CJB: 26-Jun-10: Made definition of deprecated constant name conditional
                  upon definition of CBLIB_OBSOLETE.
  CJB: 11-Dec-20: Deleted redundant uses of the 'extern' keyword.
*/

#ifndef StackViews_h
#define StackViews_h

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "toolbox.h"

/* Local headers */
#include "Macros.h"

/* For use with StackViews_configure */
enum
{
  StackViewsAuto = -1
};

void StackViews_configure(int /*xmin*/, int /*ymax*/, int /*width*/, int /*height*/, int /*xscroll*/, int /*yscroll*/);
   /* Controls the effect of subsequent calls to StackViews_open. The xmin and
    * ymax coordinates specify the top left of the starting window position, at
    * which the first window in a new stack will be opened. The width, height
    * and scroll offsets apply immediately to all windows subsequently shown
    * using StackViews_open. Any of these parameters may be specified as
    * StackViewsAuto, although the interpretation varies: For the top left
    * coordinates it means 'centre window'. For the dimensions and scroll
    * offsets it means 'use existing value' (i.e. from the object template, set
    * using ResEd). By default all are StackViewsAuto.
    */

CONST _kernel_oserror *StackViews_open(ObjectId /*id*/, ObjectId /*parent*/, ComponentId /*parent_component*/);
   /*
    * The first time this function is called it shows the specified Window
    * object at the stack's start position, and associates it with the given
    * parent object and component. The default start position is horizontally
    * and vertically centred on the screen but this can be modified via
    * StackViews_configure. Subsequent calls will open windows at an offset of
    * 48 OS units moving down the screen. When the next window would cause the
    * icon bar to be obscured the stack is restarted from the top. The
    * dimensions and scroll offsets of windows opened by this function may also
    * be altered by StackViews_configure.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

CONST _kernel_oserror *StackViews_open_get_bbox(ObjectId /*id*/, ObjectId /*parent*/, ComponentId /*parent_component*/, BBox */*bbox*/);
   /*
    * Equivalent to StackViews_open except that the visible area coordinates of
    * the Window just shown are written to the struct pointed to by 'bbox'
    * (unless that argument is NULL or an error occurred).
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

#ifdef CBLIB_OBSOLETE
/* Deprecated constant name */
#define STACKVIEWS_AUTO StackViewsAuto
#endif /* CBLIB_OBSOLETE */

#endif
