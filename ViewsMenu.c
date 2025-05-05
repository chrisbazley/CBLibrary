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

/* History:
  CJB: 20-Feb-04: Now makes less use of strncpy() where unnecessary.
                  Qualified some function parameters as 'const'.
                  Now forceably terminates view name in case it was truncated
                  by strncpy().
  CJB: 07-Mar-04: Added #include "msgtrans.h" (no longer included from
                  Macros.h).
                  Updated to use the new macro names defined in h.Macros.
  CJB: 07-Mar-04: Now uses new CLONE_STR and STRCPY_SAFE macros in place of
                  custom code (should behave the same).
  CJB: 13-Jun-04: Because all macro definitions are now expression statements,
                  have changed those invocations which omitted a trailing ';'.
  CJB: 31-Oct-04: Changed to use stricmp() instead of own function.
  CJB: 13-Jan-05: Changed to use new msgs_error() function.
  CJB: 05-Mar-05: ViewsMenu_add, ViewsMenu_setname and ViewsMenu_remove now use
                  assert() to ensure object id not duplicate/unknown instead of
                  returning pointer to 'shared_err_block' (no longer required).
  CJB: 17-Mar-05: Moved code to do pending removals out of
                  _ViewsMenu_parentclosehandler into a separate function.
  CJB: 02-Jul-05: Changed to use DeIconise_show_object instead of own function.
                  _ViewsMenu_clickhandler now produces audible warning by
                  standard method instead of '_kernel_oswrch(7)'.
                  Rectified anomaly of some function names beginning
                  'ViewMenu' (incorrect) instead of 'ViewsMenu' (correct).
  CJB: 08-Aug-05: Miscellaneous reorganisation of source (no external effect).
  CJB: 06-Feb-06: Updated to use strdup() function instead of CLONE_STR macro.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 25-Oct-06: Made inclusion of deprecated functions conditional upon
                  pre-processor symbol CBLIB_OBSOLETE.
  CJB: 22-Jun-09: Use variable name rather than type with 'sizeof' operator and
                  tweaked spacing.
  CJB: 09-Sep-09: Stop using many reserved identifiers which start with an
                  underscore followed by a capital letter ('_ViewInfo' and
                  '_ViewsMenu_...).
  CJB: 30-Sep-09: Renamed some event handler functions.
  CJB: 13-Oct-09: Deleted superfluous header inclusions. Removed dependencies
                  on MsgTrans and Err modules by storing pointers to a messages
                  file descriptor and an error-reporting callback upon
                  initialisation. Use 'for' loops in preference to 'while'
                  loops. Fixed a bug where ViewsMenu_showall would have looped
                  forever upon encountering a list item awaiting removal.
  CJB: 26-Feb-12: Made the arguments to ViewsMenu_create conditional upon
                  CBLIB_OBSOLETE.
  CJB: 17-Dec-14: Updated to use the generic linked list implementation.
                  Fixed a bug where ViewsMenu_getnext returned NULL_ObjectId
                  if the view straight after the 'current' view was pending
                  removal even if a view further beyond it was not.
  CJB: 18-Dec-14: Do deferred removals upon program exit, to avoid apparent
                  memory leaks.
  CJB: 23-Dec-14: Apply Fortify to Toolbox & Event library function calls.
  CJB: 02-Jan-15: Got rid of goto statements.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 18-Apr-16: Cast pointer parameters to void * to match %p.
  CJB: 29-Aug-20: Deleted a redundant static function pre-declaration.
  CJB: 03-May-25: Fix #include filename case.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
 */

/* ISO library headers */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "toolbox.h"
#include "event.h"
#include "menu.h"

/* CBUtilLib headers */
#include "StrExtra.h"
#include "LinkedList.h"

/* CBOSLib headers */
#include "MessTrans.h"

/* Local headers */
#include "ViewsMenu.h"
#include "DeIconise.h"
#ifdef CBLIB_OBSOLETE
#include "msgtrans.h"
#include "Err.h"
#endif /* CBLIB_OBSOLETE */
#include "Internal/CBMisc.h"

typedef struct ViewInfo
{
  LinkedListItem   list_item;
  ObjectId         object;
  char             name[256]; /* to show in menu */
  char            *file_path; /* to avoid loading duplicate files */
  bool             remove_me;
}
ViewInfo;

static ComponentId VM_parent_entry;
static LinkedList view_list;
static ObjectId VM, VM_parent;
static bool menu_showing = false, removals_pending = false;
static _Optional MessagesFD *desc;
#ifndef CBLIB_OBSOLETE
static void (*report)(CONST _kernel_oserror *);
#endif

/* ----------------------------------------------------------------------- */
/*                       Function prototypes                               */

static ToolboxEventHandler parent_about_to_be_shown, menu_selection, parent_has_been_hidden;
static void do_deferred_removals(void);
static CONST _kernel_oserror *lookup_error(const char *token);
static _Optional CONST _kernel_oserror *destroy_view(ViewInfo *view_info);
static LinkedListCallbackFn destroy_view_if_pending, view_has_matching_path, view_has_matching_object, view_show_object;

/* ----------------------------------------------------------------------- */
/*                         Public functions                                */

_Optional CONST _kernel_oserror *ViewsMenu_create(
#ifdef CBLIB_OBSOLETE
                         void
#else
                         _Optional MessagesFD  *mfd,
                         void                 (*report_error)(const _kernel_oserror *)
#endif
)
{
  /* Call to create views menu (before any other function!) */

  /* Store pointers to messages file descriptor and error-reporting function */
#ifdef CBLIB_OBSOLETE
  desc = msgs_get_descriptor();
#else
  desc = mfd;
  report = report_error;
#endif

  linkedlist_init(&view_list);
  atexit(do_deferred_removals);

  /* Create menu */
  ON_ERR_RTN_E(toolbox_create_object(0, "ViewsMenu", &VM));

  /* Listen for clicks */
  return event_register_toolbox_handler(VM,
                                        Menu_Selection,
                                        menu_selection,
                                        (void *)NULL);
}

/* ----------------------------------------------------------------------- */

_Optional CONST _kernel_oserror *ViewsMenu_parentcreated(ObjectId parent_menu, ComponentId parent_entry)
{
  /* To be called when parent menu is created */
  VM_parent_entry = parent_entry;
  VM_parent = parent_menu;

  /* Register event handlers for when parent is opened and closed */
  ON_ERR_RTN_E(event_register_toolbox_handler(parent_menu,
                                              Menu_AboutToBeShown,
                                              parent_about_to_be_shown,
                                              (void *)NULL));

  return event_register_toolbox_handler(parent_menu,
                                        Menu_HasBeenHidden,
                                        parent_has_been_hidden,
                                        (void *)NULL);
}

/* ----------------------------------------------------------------------- */

_Optional CONST _kernel_oserror *ViewsMenu_setname(ObjectId showobject, const char *view_name, _Optional const char *file_path)
{
  _Optional ViewInfo *view_info;
  _Optional char *new_ptr;

  view_info = (ViewInfo *)linkedlist_for_each(
              &view_list, view_has_matching_object, &showobject);

  assert(view_info != NULL);

  if (view_info != NULL)
  {
    /* Found specified menu entry */
    if (file_path != NULL)
    {
      /* Change filepath associated with this menu entry */
      new_ptr = strdup(file_path);
      if (new_ptr == NULL)
        return lookup_error("NoMem");

      free(view_info->file_path);
      view_info->file_path = &*new_ptr;
    }
    if (view_name != NULL)
    {
      /* Change text for this menu entry
         (the cast of view_name to plain char * shouldn't be necessary,
         but the menu_set_entry_text() definition is lax.) */
      ON_ERR_RTN_E(menu_set_entry_text(0,
                                       VM,
                                       (ComponentId)(uintptr_t)view_info,
                                       (char *)view_name));
    }
  }

  return NULL;
}

/* ----------------------------------------------------------------------- */

ObjectId ViewsMenu_getfirst(void)
{
  _Optional ViewInfo *view_info;

  view_info = (ViewInfo *)linkedlist_for_each(
              &view_list, view_has_matching_object, (void *)NULL);

  return view_info == NULL ? NULL_ObjectId : view_info->object;
}

/* ----------------------------------------------------------------------- */

ObjectId ViewsMenu_getnext(ObjectId current)
{
  _Optional ViewInfo *view_info;

  view_info = (ViewInfo *)linkedlist_for_each(
              &view_list, view_has_matching_object, &current);
  if (view_info != NULL)
  {
    for (view_info = (ViewInfo *)linkedlist_get_next(&view_info->list_item);
         view_info != NULL;
         view_info = (ViewInfo *)linkedlist_get_next(&view_info->list_item))
    {
      if (!view_info->remove_me)
        return view_info->object; /* found */
    }
  }

  return NULL_ObjectId;  /* 'current' view not found, or end of list */
}

/* ----------------------------------------------------------------------- */

_Optional CONST _kernel_oserror *ViewsMenu_add(ObjectId showobject, const char *view_name, const char *file_path)
{
  _Optional ViewInfo *new_view, *view_info;

  DEBUGF("ViewsMenu: Add viewsmenu entry for object 0x%x with name %s and path %s\n",
         showobject, view_name, file_path);

  /* Check not already on list */
  view_info = (ViewInfo *)linkedlist_for_each(
              &view_list, view_has_matching_object, &showobject);

  assert(view_info == NULL);
  if (view_info != NULL)
    return NULL; /* duplicate object id */

  /* Create new menu entry */
  new_view = malloc(sizeof(*new_view));
  if (new_view == NULL)
    return lookup_error("NoMem");

  /* Set entry text, and associated toolbox object & filepath */
  new_view->remove_me = false;
  _Optional char *p = strdup(file_path);
  if (p == NULL)
  {
    free(new_view);
    return lookup_error("NoMem");
  }
  new_view->file_path = &*p;

  new_view->object = showobject;
  STRCPY_SAFE(&*new_view->name, view_name);

  /* Add entry to menu */
  {
    _Optional _kernel_oserror *errptr;
    MenuTemplateEntry Entry =
    {
      0,
      (ComponentId)(uintptr_t)new_view,
      &*new_view->name,
      sizeof(new_view->name),
      (void *)NULL,
      (void *)NULL,
      0,
      0,
      (void *)NULL,
      0
    };

    errptr = menu_add_entry(0,
                            VM,
                            Menu_AddEntryAtEnd,
                            (char *)&Entry,
                            0);
    if (errptr != NULL)
    {
      free(new_view->file_path);
      free(new_view);
      return errptr; /* failure */
    }
  }

  /* Link new menu entry into list */
  linkedlist_insert(&view_list, NULL, &new_view->list_item);

  return NULL; /* success */
}

/* ----------------------------------------------------------------------- */

_Optional CONST _kernel_oserror *ViewsMenu_showall(void)
{
  /* Bring all open windows to the front */
  _Optional CONST _kernel_oserror *e = NULL;
  linkedlist_for_each(&view_list, view_show_object, &e);
  return e;
}

/* ----------------------------------------------------------------------- */

_Optional CONST _kernel_oserror *ViewsMenu_remove(ObjectId showobject)
{
  /* Remove a window from the list */
  _Optional ViewInfo *view_info;

  DEBUGF("ViewsMenu: Remove viewsmenu entry for object 0x%x\n", showobject);

  view_info = (ViewInfo *)linkedlist_for_each(
              &view_list, view_has_matching_object, &showobject);

  assert(view_info != NULL);

  if (view_info != NULL)
  {
    /* Is views menu showing? */
    if (menu_showing)
    {
      /* Removing entry seems to cause trouble if menu is open */
      view_info->remove_me = true;
      removals_pending = true;
      DEBUGF("ViewsMenu: Deferred removal of viewsmenu entry %p\n", (void *)view_info);
      /* (Removal deferred until menu closes. Must keep linked list record
      or we will lose the ComponentId of the menu entry.) */
    }
    else
    {
      ON_ERR_RTN_E(destroy_view(&*view_info));
    }
  }

  return NULL; /* success */
}

/* ----------------------------------------------------------------------- */

ObjectId ViewsMenu_findview(const char *file_path_to_match)
{
  /* Find a view matching the specified name */
  _Optional const ViewInfo *view_info;

  assert(file_path_to_match != NULL);

  view_info = (ViewInfo *)linkedlist_for_each(
              &view_list, view_has_matching_path, (char *)file_path_to_match);

  return view_info == NULL ? NULL_ObjectId : view_info->object;
}

#ifdef CBLIB_OBSOLETE
/* ----------------------------------------------------------------------- */
/*                       Deprecated functions                              */

bool ViewsMenu_strcmp_nc(const char *string1, const char *string2)
{
  return stricmp(string1, string2) == 0;
}

/* ----------------------------------------------------------------------- */

_Optional CONST _kernel_oserror *ViewsMenu_show_object(unsigned int flags, ObjectId id, int show_type, void *type, ObjectId parent, ComponentId parent_component)
{
  return DeIconise_show_object(flags,
                               id,
                               show_type,
                               type,
                               parent,
                               parent_component);
}

/* ----------------------------------------------------------------------- */

ObjectId ViewMenu_getfirst(void)
{
  return ViewsMenu_getfirst();
}

/* ----------------------------------------------------------------------- */

ObjectId ViewMenu_getnext(ObjectId current)
{
  return ViewsMenu_getnext(current);
}
#endif /* CBLIB_OBSOLETE */

/* ----------------------------------------------------------------------- */
/*                         Private functions                               */

static void check_error(_Optional CONST _kernel_oserror *e)
{
#ifdef CBLIB_OBSOLETE
  (void)err_check(e);
#else
  if (e != NULL && report)
    report(&*e);
#endif
}

/* ----------------------------------------------------------------------- */

static CONST _kernel_oserror *lookup_error(const char *token)
{
  /* Look up error message from the token, outputting to an internal buffer */
  return messagetrans_error_lookup(desc, DUMMY_ERRNO, token, 0);
}

/* ----------------------------------------------------------------------- */

static int parent_about_to_be_shown(int           event_code,
                                    ToolboxEvent *event,
                                    IdBlock      *id_block,
                                    void         *handle)
{
  /* Parent of views menu is about to open */
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(handle);

  /* If linked list is empty then grey views submenu entry */
  check_error(menu_set_fade(0,
                            id_block->self_id,
                            VM_parent_entry,
                            linkedlist_get_head(&view_list) == NULL));

  menu_showing = true; /* we need to defer deletions until menu closes */

  return 0; /* pass event on (not our menu!) */
}

/* ----------------------------------------------------------------------- */

static int parent_has_been_hidden(int           event_code,
                                  ToolboxEvent *event,
                                  IdBlock      *id_block,
                                  void         *handle)
{
  /* Parent of views menu has closed */
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(id_block);
  NOT_USED(handle);

  menu_showing = false; /* we no longer need to defer deletions */

  if (removals_pending)
    do_deferred_removals();

  return 1; /* claim event */
}

/* ----------------------------------------------------------------------- */

static int menu_selection(int           event_code,
                          ToolboxEvent *event,
                          IdBlock      *id_block,
                          void         *handle)
{
  ObjectId parent;
  ComponentId parent_component;
  ViewInfo *view_info;
  _Optional CONST _kernel_oserror *e = NULL;

  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(handle);

  view_info = (ViewInfo *)(uintptr_t)id_block->self_component;
  if (view_info->remove_me)
  {
    putchar('\a'); /* beep */
  }
  else
  {
    /* Get parent of object associated with this menu entry */
    e = toolbox_get_parent(0, view_info->object, &parent, &parent_component);
    if (e == NULL)
    {
      /* Trap in case parent no longer exists */
      {
        unsigned int state;
        if (toolbox_get_object_state(0, parent, &state) != NULL)
        {
          parent = NULL_ObjectId;
          parent_component = NULL_ComponentId;
        }
      }

      /* Re-open, preserving current parent */
      e = DeIconise_show_object(0,
                                view_info->object,
                                Toolbox_ShowObject_Default,
                                NULL,
                                parent,
                                parent_component);
    }
  }

  check_error(e);
  return 1; /* claim event */
}

/* ----------------------------------------------------------------------- */

static void do_deferred_removals(void)
{
  /* Remove any entries on views menu that have been marked as dead
     (either because menu showing or client is walking list) */
  linkedlist_for_each(&view_list, destroy_view_if_pending, (void *)NULL);
  removals_pending = false;
}

/* ----------------------------------------------------------------------- */

static bool destroy_view_if_pending(LinkedList *list, LinkedListItem *item, void *arg)
{
  ViewInfo * const view_info = (ViewInfo *)item;

  assert(view_info != NULL);
  NOT_USED(arg);
  NOT_USED(list);

  if (view_info->remove_me)
    check_error(destroy_view(view_info));

  return false; /* next item */
}

/* ----------------------------------------------------------------------- */

static bool view_has_matching_path(LinkedList *list, LinkedListItem *item, void *arg)
{
  const ViewInfo * const view_info = (ViewInfo *)item;
  const char * const file_path_to_match = arg;

  assert(view_info != NULL);
  assert(file_path_to_match != NULL);
  NOT_USED(list);

  return !view_info->remove_me &&
         stricmp(view_info->file_path, file_path_to_match) == 0;
}

/* ----------------------------------------------------------------------- */

static bool view_has_matching_object(LinkedList *list, LinkedListItem *item, void *arg)
{
  const ViewInfo * const view_info = (ViewInfo *)item;
  const ObjectId * const object_to_match = arg;

  assert(view_info != NULL);
  NOT_USED(list);

  /* NULL matches any toolbox object */
  return !view_info->remove_me &&
         (object_to_match == NULL || view_info->object == *object_to_match);
}

/* ----------------------------------------------------------------------- */

static bool view_show_object(LinkedList *list, LinkedListItem *item, void *arg)
{
  const ViewInfo * const view_info = (ViewInfo *)item;
  _Optional CONST _kernel_oserror ** const eout = arg;
  _Optional CONST _kernel_oserror *e = NULL;

  assert(view_info != NULL);
  assert(eout != NULL);
  NOT_USED(list);

  if (!view_info->remove_me)
  {
    ObjectId parent;
    ComponentId parent_component;
    unsigned int state;

    /* Show object using existing parent */
    e = toolbox_get_parent(0, view_info->object, &parent, &parent_component);
    if (e == NULL)
    {
      /* Trap incase parent object no longer exists */
      if (toolbox_get_object_state(0, parent, &state) != NULL)
      {
        parent = NULL_ObjectId;
        parent_component = NULL_ComponentId;
      }

      e = DeIconise_show_object(0,
                                view_info->object,
                                Toolbox_ShowObject_Default,
                                NULL,
                                parent,
                                parent_component);
    }
  }

  *eout = e;
  return (e != NULL); /* stop on error */
}

/* ----------------------------------------------------------------------- */

static _Optional CONST _kernel_oserror *destroy_view(ViewInfo *view_info)
{
  assert(view_info != NULL);
  DEBUGF("ViewsMenu: Removing view record %p\n", (void *)view_info);

  /* Link over record */
  linkedlist_remove(&view_list, &view_info->list_item);

  free(view_info->file_path);
  free(view_info);

  return menu_remove_entry(0, VM, (ComponentId)(uintptr_t)view_info);
}
