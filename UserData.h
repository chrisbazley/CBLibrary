/*
 * CBLibrary: User data list
 * Copyright (C) 2014 Christopher Bazley
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

/* UserData.h declares functions and types for a list of user data
   that may need to be saved before they are destroyed.

Dependencies: ANSI C library.
Message tokens: None.
History:
  CJB: 11-Dec-14: Created this header file.
  CJB: 27-Dec-14: Added userdata_destroy function to the public interface.
  CJB: 11-Dec-20: Deleted redundant uses of the 'extern' keyword.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
*/

#ifndef userdata_h
#define userdata_h

/* ISO library headers */
#include <stdbool.h>

/* CBUtilLib headers */
#include "StringBuff.h"
#include "LinkedList.h"

#if !defined(USE_OPTIONAL) && !defined(_Optional)
#define _Optional
#endif

struct UserData;

typedef bool UserDataIsSafeFn(struct UserData *item);
   /*
    * Type of function called to determine whether or not user data should
    * be saved before a task exits. The callee is expected to access any
    * application-specific data relative to the address of 'item'.
    * Returns: true if the user data item can be safely destroyed.
    */

typedef void UserDataDestroyFn(struct UserData *item);
   /*
    * Type of function called to destroy a user data item and remove it from
    * the list. The callee is expected to access any application-specific
    * data relative to the address of 'item'.
    */

typedef struct UserData
{
  LinkedListItem list_item;
  StringBuffer file_name;
  _Optional UserDataIsSafeFn *is_safe;
  _Optional UserDataDestroyFn *destroy;
}
UserData;
   /*
    * A generic user data item type. This is usually a member of a bigger
    * struct containing application-specific data.
    */

void userdata_init(void);
   /*
    * Initializes a global per-task list of user data. The initialized list
    * is empty.
    */

bool userdata_add_to_list(UserData *data,
                          _Optional UserDataIsSafeFn *is_safe,
                          _Optional UserDataDestroyFn *destroy,
                          const char *file_name);
   /*
    * Adds a user data item to a global per-task list. Storage allocation
    * for the item is the caller's responsibility. If 'is_safe' is NULL then
    * the item is assumed to be always safe to destroy. If 'destroy' is NULL
    * then userdata_destroy_all merely removes the item from the list (e.g.
    * because storage for it was statically allocated). The 'file_name'
    * string is copied.
    * Returns: true if successful, or false if memory allocation failed.
    */

void userdata_remove_from_list(UserData *data);
   /*
    * Removes a user data item from a global list. Does not call its
    * destructor function (if any).
    */

unsigned int userdata_count_unsafe(void);
   /*
    * Counts the number of user data items that are not safe to destroy.
    * Returns: the number of unsafe user data items (e.g. unsaved documents).
    */

bool userdata_set_file_name(UserData *data, const char *file_name);
   /*
    * Sets the file path string associated with a given user data item,
    * e.g. because it has been saved in a new location. The 'file_name'
    * string is copied.
    * Returns: true if successful, or false if memory allocation failed.
    */

char *userdata_get_file_name(const UserData *data);
   /*
    * Returns a direct pointer to the file path string associated with a
    * given user data item. May instead return a pointer to a string
    * literal if the length is 0. Should not be used to modify the string
    * unless immediately restored (e.g. as in make_path).
    */

size_t userdata_get_file_name_length(const UserData *data);
   /*
    * Gets the length in characters of the file path string associated
    * with a given user data item, not including nul terminator.
    */

_Optional UserData *userdata_find_by_file_name(const char *file_name);
   /*
    * Finds a user data item matching the given file path. The comparison
    * is case-insensitive like RISC OS file names.
    * Returns: address of the user data item with the matching file path,
    *          or NULL if none was found.
    */

void userdata_destroy(UserData *data);
   /*
    * Destroys one user data item.
    */

void userdata_destroy_all(void);
   /*
    * Destroys all user data items.
    */

typedef bool UserDataCallbackFn(UserData *data, void *arg);
   /*
    * Type of function called back for each user data item. The value of
    * 'arg' is that passed to the userdata_for_each function and is expected
    * to point to any additional parameters. It is safe to remove the current
    * item in this function.
    * Returns: true to stop iterating over the list, otherwise false.
    */

_Optional UserData *userdata_for_each(UserDataCallbackFn *callback, void *arg);
   /*
    * Calls a given function for each user data item in the global list.
    * The value of 'arg' will be passed to the 'callback' function with the
    * address of each item and is expected to point to any additional
    * parameters required. Can be used to find user data, if a suitable
    * callback function is provided.
    * Returns: address of the user data item on which iteration stopped,
    *          or NULL if the callback function never returned true.
    */

#endif
