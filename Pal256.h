/*
 * CBLibrary: 256 colour selection dialogue box
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

/* Pal256.h declares two functions that provide an interface to a colour
   selection dialogue box that allows user selection from the default RISC OS
   256 colour palette.

Dependencies: ANSI C library, Acorn library kernel, Acorn's WIMP, toolbox &
              event libraries.
Message tokens: NoMem.
History:
  CJB: 04-Nov-04: Added clib-style documentation and dependency information.
  CJB: 05-Mar-05: Added documentation on new function Pal256_colour_brightness.
  CJB: 05-Jul-05: Changed type of Pal256_colour_brightness argument to unsigned
                  long.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
                  A pointer to a 256 colour palette must now be passed to
                  Pal256_initialise() instead of resolving this external
                  dependency at link time.
  CJB: 12-Oct-09: Updated to use new PaletteEntry type, and 'unsigned int'
                  instead of a mixture of 'char' and 'int' for colour numbers.
                  Added extra arguments to the prototype of Pal256_initialise.
                  Marked the Pal256_colour_brightness function as deprecated.
  CJB: 15-Oct-09: Added "NoMem" to list of required message tokens.
  CJB: 05-May-12: Made the extra arguments to Pal256_initialise conditional
                  upon CBLIB_OBSOLETE.
  CJB: 11-Dec-20: Deleted redundant uses of the 'extern' keyword.
  CJB: 28-May-22: Allow initialisation with a 'const' palette array.
*/

#ifndef Pal256_h
#define Pal256_h

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "toolbox.h"

/* CBOSLib headers */
#include "PalEntry.h"

/* Local headers */
#include "Macros.h"

/****************************************************************************
 * Pal256 Toolbox Events                                                    *
 ****************************************************************************/

#define Pal256_ColourSelected     0x100u

typedef struct
{
  ToolboxEventHeader hdr;
  unsigned int       colour_number;
}
Pal256ColourSelectedEvent;
/*
   User colour selections are communicated to the client program via
   this special Toolbox event, which is raised when the user clicks
   on the dialogue box's 'OK' button.
*/

/****************************************************************************
 * Pal256 Functional Interface                                              *
 ****************************************************************************/

CONST _kernel_oserror *Pal256_initialise(
                     ObjectId       /*object*/,
                     PaletteEntry const  /*palette*/[]
#ifndef CBLIB_OBSOLETE
                    ,MessagesFD    */*mfd*/,
                     void         (*/*report_error*/)(CONST _kernel_oserror *)
#endif
);
   /*
    * Sets up some WIMP and toolbox event handlers in order to manage the
    * operation of a Pal256 dialogue box on behalf of a client program.
    * Programs should call this function with the Toolbox object Id of a
    * dialogue box after it has been created (by trapping the
    * Toolbox_ObjectAutoCreated event if necessary). A suitable object
    * template is supplied with this library.
    * Unless 'mfd' is a null pointer, the specified messages file will be given
    * priority over the global messages file when looking up text required by
    * this module. Unless 'report_error' is a null pointer, it should point to
    * a function to be called if an error occurs whilst handling an event.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

CONST _kernel_oserror *Pal256_set_colour(ObjectId /*object*/, unsigned int /*c*/);
   /*
    * Causes colour number 'c' (0-255) to be displayed by the specified Pal256
    * dialogue box 'object'. This is also the colour that will be restored if
    * the dialogue box is reset by the user, yet remains open. Programs should
    * call this function before a Pal256 dialogue box is shown on screen
    * (by trapping the Window_AboutToBeShown event or similar if necessary).
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

char Pal256_colour_brightness(unsigned long colour);
   /*
    * This function is deprecated - you should use 'palette_entry_brightness'
    * instead.
    */

#endif
