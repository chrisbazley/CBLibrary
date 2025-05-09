/*
 * CBLibrary: Maintain a list of document views and an associated menu object
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

/* ViewsMenu.h declares various functions that allow a list of named Toolbox
   G.U.I. objects to be maintained on behalf of a RISC OS application. Each
   object may have a file path associated with it. The list is presented in the
   form of a submenu, thus allowing the user to view any object by selecting it.

Dependencies: ANSI C library, Acorn library kernel, Acorn's WIMP, toolbox &
              event libraries.
Message tokens: NoMem.
History:
  CJB: 20-Feb-04: Qualified some function parameters with 'const'.
  CJB: 31-Oct-04: Added warning about deprecation of function
                  ViewsMenu_strcmp_nc().
  CJB: 04-Nov-04: Added clib-style documentation and dependency information.
  CJB: 05-Mar-05: Updated documentation on ViewsMenu_add, ViewsMenu_setname
                  and ViewsMenu_remove.
  CJB: 02-Jul-05: Renamed ViewMenu_getfirst and ViewMenu_getnext according
                  to normal convention but added declarations of old names
                  for backward compatibility. Moved ViewsMenu_show_object
                  to list of deprecated functions.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 11-Oct-09: Added arguments to the prototype of ViewsMenu_create.
  CJB: 15-Oct-09: Added "NoMem" to list of required message tokens.
  CJB: 26-Feb-12: Made the arguments to ViewsMenu_create conditional upon
                  CBLIB_OBSOLETE.
  CJB: 11-Dec-20: Deleted redundant uses of the 'extern' keyword.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
                  Allow null 'type' argument for ViewsMenu_show_object().
*/

#ifndef ViewsMenu_h
#define ViewsMenu_h

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

_Optional CONST _kernel_oserror *ViewsMenu_create(
#ifdef CBLIB_OBSOLETE
                       void
#else
                       _Optional MessagesFD  * /*mfd*/,
                       void                 (* /*report_error*/ )(CONST _kernel_oserror *)
#endif
);
   /*
    * Creates the shared menu object (for which a template named 'ViewsMenu'
    * must exist in the application's Resource file) and registers an event
    * handler to respond to user menu selections. Unless 'mfd' is a null
    * pointer, the specified messages file will be given priority over the
    * global messages file when looking up text required by this module. Unless
    * 'report_error' is a null pointer, it should point to a function to be
    * called if an error occurs whilst handling an event.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

_Optional CONST _kernel_oserror *ViewsMenu_parentcreated(ObjectId /*parent_menu*/, ComponentId /*parent_entry*/);
   /*
    * Records 'parent_menu' as being the Toolbox object Id of our parent menu,
    * and registers event handlers to enable/disable our menu item (specified
    * by 'parent_entry') before our parent menu is shown, and action any pending
    * removals after it has closed. The parent menu must have been configured
    * (using ResEd) to deliver Menu_AboutToBeShown and Menu_HasBeenHidden
    * events.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

_Optional CONST _kernel_oserror *ViewsMenu_add(ObjectId /*showobject*/, const char * /*view_name*/, const char * /*file_path*/);
   /*
    * Adds an entry named 'view_name' to the bottom of our menu and associates
    * this with the specified object Id 'showobject' and contents of string
    * 'file_path' (which is copied to a private buffer for safekeeping).
    * As far as the program is concerned the latter is just an arbitrary string
    * by which the list of objects may be searched using ViewsMenu_findview. An
    * attempt to add two menu entries associated with the same object Id may
    * result in abnormal program termination.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

_Optional CONST _kernel_oserror *ViewsMenu_setname(ObjectId /*showobject*/, const char * /*view_name*/, _Optional const char * /*file_path*/);
   /*
    * Changes the text of the menu entry and/or file path associated with the
    * Toolbox object 'showobject'. Either or both of the 'view_name' and
    * 'file_path' pointers may be NULL, in which case the corresponding value
    * will not be updated. May cause abnormal program termination if an
    * unknown object id is supplied.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

_Optional CONST _kernel_oserror *ViewsMenu_remove(ObjectId /*showobject*/);
   /*
    * Removes the menu entry and file path associated with the Toolbox object
    * 'showobject'. The menu entry will not be removed immediately if it is on
    * screen when this function is called - user selection of such 'dead'
    * menu entries will result in a warning beep. May cause abnormal program
    * termination if an unknown object id is supplied.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

_Optional CONST _kernel_oserror *ViewsMenu_showall(void);
   /*
    * Shows all known Toolbox objects in the reverse order to that in which the
    * entries were added to the menu (i.e. latest first). Internally this
    * function uses DeIconise_show_object.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

ObjectId ViewsMenu_findview(const char * /*file_path_to_match*/);
   /*
    * Searches the internal list of Toolbox objects for one where the associated
    * string matches 'file_path_to_match'. The comparisons are case insensitive
    * because this facility is intended for use with RISC OS file paths.
    * Returns: the Toolbox Id of the object associated with the specified
    *          string, or NULL_ObjectId if no match.
    */

ObjectId ViewsMenu_getfirst(void);
   /*
    * Finds the first Toolbox object in our internal list. Generally this
    * corresponds to the bottom-most menu entry, unless it is awaiting removal
    * following a call to ViewsMenu_delete. You can use this function in
    * conjunction with ViewsMenu_getnext to enumerate all Toolbox objects
    * that appear on our menu.
    * Returns: the Toolbox Id of the first object, or NULL_ObjectId if none
    *          (because our menu is empty).
    */

ObjectId ViewsMenu_getnext(ObjectId /*current*/);
   /*
    * Finds the Toolbox object that comes after 'current' in our internal list.
    * Returns: the Toolbox Id of the next object, or NULL_ObjectId if no more.
    */


/* The following functions are deprecated and should not be used in
   new or updated programs. */
bool ViewsMenu_strcmp_nc(const char * /*string1*/, const char * /*string2*/);

_Optional CONST _kernel_oserror *ViewsMenu_show_object(unsigned int /*flags*/,
  ObjectId /*id*/, int /*show_type*/, _Optional void * /*type*/,
  ObjectId /*parent*/, ComponentId /*parent_component*/);

ObjectId ViewMenu_getfirst(void);
ObjectId ViewMenu_getnext(ObjectId /*current*/);

#endif
