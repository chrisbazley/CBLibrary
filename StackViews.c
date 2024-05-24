/*
 * CBLibrary: Show a Toolbox object at a position suitable for a new view
 * Copyright (C) 2003 Christopher Bazley
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
  CJB: 01-Mar-05: Commenced implementation.
  CJB: 04-Mar-05: Added function StackViews_setdefault to give client more
                  control over default window position and size.
  CJB: 05-Mar-05: Renamed StackViews_setdefault as StackViews_configure.
  CJB: 06-Mar-05: Added experimental code to make leaving a gap for the icon
                  bar conditional on the Wimp's toggle size configuration.
  CJB: 28-May-05: Removed 'extern' qualifier from definition of
                  StackViews_configure.
                  Added function StackViews_open_return_bbox to allow client to
                  know the bounding box of window just opened.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 25-Nov-06: Updated debugging text.
  CJB: 29-Aug-09: Modified the StackViews_open function to use the new
                  get_screen_size function. No longer misuses the screen mode's
                  X eigen factor to calculate the height of a single-pixel
                  bottom border.
  CJB: 04-Oct-09: Replaced macro value with enumerated constant. Updated to use
                  new library function os_read_mode_variable.
  CJB: 26-Jun-10: Updated to use new constant name.
  CJB: 05-May-12: Added support for stress-testing failure of _kernel_osbyte.
                  Replaced magic values with enumerated constants.
  CJB: 23-Dec-14: Apply Fortify to Toolbox & Wimp library function calls.
  CJB: 31-Jan-16: Substituted _kernel_swi for _swix because it's easier to
                  intercept for stress testing.
  CJB: 03-Apr-16: Added missing WindowShowObjectBlock initializers to avoid
                  GNU C compiler warnings.
  CJB: 01-Nov-18: Replaced DEBUG macro usage with DEBUGF.
 */

/* ISO library headers */
#include <limits.h>
#include <stdbool.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "swis.h"
#include "toolbox.h"
#include "window.h"
#include "wimp.h"
#include "wimplib.h"

/* CBOSLib headers */
#include "OSVDU.h"

/* Local headers */
#include "Internal/CBMisc.h"
#include "StackViews.h"
#include "Err.h"
#include "ScreenSize.h"

/* The height of the gap at the bottom of the screen within which windows
   will not be opened. Should be the same as that left by the Wimp when a
   window is toggled to partial full size (i.e. overlapping the icon bar
   only slightly). */
enum
{
  IconBarGap = 128,
  OSByte_ReadNVRAM = 161, /* _kernel_osbyte reason code */
  OSByteR2ResultShift = 8, /* To decode return value of _kernel_osbyte */
  NVRAMMiscFlags = 28, /* Truncate/DragASprite/FilerAction/Dither */
  NVRAMMiscFlags_NoObscureIconBar = 1<<4 /* Flag bit to stop windows
                                            overlapping the icon bar when
                                            toggled to full size */
};

#undef PEEK_WIMP_CMOS

static int initial_width = StackViewsAuto, initial_height = StackViewsAuto;
static int default_xmin = StackViewsAuto, default_ymax = StackViewsAuto;
static int initial_xscroll = StackViewsAuto, initial_yscroll = StackViewsAuto;

static WindowShowObjectBlock wsob =
{
  {
    0,
    0,
    0,
    INT_MIN /* forces immediate reset to default coordinates */
  },
  0,
  0,
  WimpWindow_Top,
  0, /* unused window flags */
  0, /* unused parent window */
  0  /* unused alignment flags */
};

/* ----------------------------------------------------------------------- */
/*                         Public functions                                */

void StackViews_configure(int xmin, int ymax, int width, int height, int xscroll, int yscroll)
{
  default_xmin = xmin;
  default_ymax = ymax;
  initial_width = width;
  initial_height = height;
  initial_xscroll = xscroll;
  initial_yscroll = yscroll;
}

/* ----------------------------------------------------------------------- */

CONST _kernel_oserror *StackViews_open(ObjectId id, ObjectId parent, ComponentId parent_component)
{
  int width = initial_width, height = initial_height;
  WimpGetWindowStateBlock state;

  DEBUGF("StackViews: Request to open window &%X with parent &%X\n", id, parent);

  ON_ERR_RTN_E(window_get_wimp_handle(0, id, &state.window_handle));
  ON_ERR_RTN_E(wimp_get_window_state(&state));

  wsob.xscroll = initial_xscroll;
  wsob.yscroll = initial_yscroll;

  if (width == StackViewsAuto ||
      height == StackViewsAuto ||
      wsob.xscroll == StackViewsAuto ||
      wsob.yscroll == StackViewsAuto)
  {
    /* Use window's existing dimensions and/or scroll offsets */

    if (width == StackViewsAuto)
    {
      width = state.visible_area.xmax - state.visible_area.xmin;
      DEBUGF("StackViews: Using existing width %d\n", width);
    }

    if (height == StackViewsAuto)
    {
      height = state.visible_area.ymax - state.visible_area.ymin;
      DEBUGF("StackViews: Using existing height %d\n", height);
    }

    if (wsob.xscroll == StackViewsAuto)
    {
      wsob.xscroll = state.xscroll;
      DEBUGF("StackViews: Using existing x scroll %d\n", wsob.xscroll);
    }
    if (wsob.yscroll == StackViewsAuto)
    {
      wsob.yscroll = state.yscroll;
      DEBUGF("StackViews: Using existing y scroll %d\n", wsob.yscroll);
    }
  }

  {
    int bottom_border, gap, scr_width, scr_height;
    WimpSysInfo wimp_vsn;

    ON_ERR_RTN_E(get_screen_size(&scr_width, &scr_height));

    ON_ERR_RTN_E(wimp_read_sys_info(7, &wimp_vsn));
    DEBUGF("StackViews: Wimp version: %.2f\n", (float)wimp_vsn.r0/100.0);
    if (wimp_vsn.r0 >= 400)
    {
      /* Read height of bottom border (see functional spec of Ursula Wimp) */
      int info_block[100/sizeof(int)]; /* block must be 100 bytes */
      _kernel_swi_regs regs;
      regs.r[0] = 11;
      regs.r[1] = (int)info_block;
      info_block[0] = state.window_handle; /* window handle */
      ON_ERR_RTN_E(_kernel_swi(Wimp_Extend, &regs, &regs));
      bottom_border = info_block[2];
    }
    else
    {
      if (TEST_BITS(state.flags, WimpWindow_HScroll))
      {
        bottom_border = 40;
      }
      else
      {
        int y_eigen;
        bool valid;

        ON_ERR_RTN_E(os_read_mode_variable(OS_ReadModeVariable_CurrentMode,
                                           ModeVar_YEigFactor,
                                           &y_eigen,
                                           &valid));
        bottom_border = valid ? 1 << y_eigen : 0;
      }
    }
    DEBUGF("StackViews: Height of bottom border: %d\n", bottom_border);

#ifdef PEEK_WIMP_CMOS
    /* Should we leave a gap at the bottom of the screen for icon bar? */
    {
      int ret_val = _kernel_osbyte(OSByte_ReadNVRAM, NVRAMMiscFlags, 0);
      if (ret_val == _kernel_ERROR)
        ret_val = 0;

      gap = TEST_BITS(ret_val >> OSByteR2ResultShift, NVRAMMiscFlags_NoObscureIconBar) ?
            IconBarGap : 0;
    }
#else
    gap = IconBarGap;
#endif

    /* Subsequent windows should be opened at an offset of 48 OS units
       moving down the screen (until bottom reached). */
    if (wsob.visible_area.ymax >= gap + bottom_border + height + 48)
    {
      wsob.visible_area.ymax -= 48;
      DEBUGF("StackViews: Modified vertical coordinate is %d\n",
            wsob.visible_area.ymax);
    }
    else
    {
      /* Force return to default window position */
      DEBUGF("StackViews: Resetting window coordinates to default\n");
            wsob.visible_area.xmin = default_xmin;
            wsob.visible_area.ymax = default_ymax;

      if (wsob.visible_area.xmin == StackViewsAuto ||
          wsob.visible_area.ymax == StackViewsAuto)
      {
        /* Open window centred vertically or horizontally on screen */
        if (wsob.visible_area.xmin == StackViewsAuto)
        {
          DEBUGF("StackViews: Centring x coordinate on %d\n", scr_width / 2);
          wsob.visible_area.xmin = scr_width / 2 - width / 2;
        }

        if (wsob.visible_area.ymax == StackViewsAuto)
        {
          DEBUGF("StackViews: Centring y coordinate on %d\n", scr_height / 2);
          wsob.visible_area.ymax = scr_height / 2 + height / 2;
        }
      }
    }
  }

  wsob.visible_area.ymin = wsob.visible_area.ymax - height;
  wsob.visible_area.xmax = wsob.visible_area.xmin + width;

  DEBUGF("StackViews: Visible area coordinates: %d,%d,%d,%d\n",
        wsob.visible_area.xmin, wsob.visible_area.ymin,
        wsob.visible_area.xmax, wsob.visible_area.ymax);

  return toolbox_show_object(0,
                             id,
                             Toolbox_ShowObject_FullSpec,
                             &wsob,
                             parent,
                             parent_component);
}

/* ----------------------------------------------------------------------- */

CONST _kernel_oserror *StackViews_open_get_bbox(ObjectId id, ObjectId parent, ComponentId parent_component, BBox *bbox)
{
  ON_ERR_RTN_E(StackViews_open(id, parent, parent_component));

  if (bbox != NULL)
    *bbox = wsob.visible_area;

  return NULL; /* success */
}
