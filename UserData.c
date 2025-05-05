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

/* History:
  CJB: 11-Dec-14: Created this source file.
  CJB: 27-Dec-14: Added userdata_destroy function to the public interface.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 09-Apr-16: Fixed a bad format string in userdata_destroy
                  (a StringBuffer pointer was misused as a char pointer).
  CJB: 10-Apr-16: Cast pointer parameters to void * to match %p.
  CJB: 05-Feb-19: Use stringbuffer_append_all where appropriate.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
*/

/* ISO library headers */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

/* CBUtilLib headers */
#include "StrExtra.h"
#include "StringBuff.h"
#include "LinkedList.h"

/* Local headers */
#include "UserData.h"
#include "Internal/CBMisc.h"

/* Linked list of user data structures */
static LinkedList user_data_list = { NULL, NULL };

typedef struct
{
  UserDataCallbackFn *callback;
  void *arg;
}
UserDataVisitorCtx;

/* ----------------------------------------------------------------------- */
/*                       Function prototypes                               */

static UserDataCallbackFn destroy_user_data, count_unsafe_user_data, user_data_name_matches;
static LinkedListCallbackFn user_data_visitor;

/* ----------------------------------------------------------------------- */
/*                         Public functions                                */
void userdata_init(void)
{
  linkedlist_init(&user_data_list);
}

/* ----------------------------------------------------------------------- */

void userdata_remove_from_list(UserData *data)
{
  assert(data != NULL);
  DEBUGF("UserData: Removing user data %p from list\n", (void *)data);
  stringbuffer_destroy(&data->file_name);
  linkedlist_remove(&user_data_list, &data->list_item);
}

/* ----------------------------------------------------------------------- */

bool userdata_add_to_list(UserData *data,
                          _Optional UserDataIsSafeFn *is_safe,
                          _Optional UserDataDestroyFn *destroy,
                          const char *file_name)
{
  bool success = true;

  assert(data != NULL);
  assert(file_name != NULL);
  DEBUGF("UserData: Adding user data %p with file name '%s'\n",
         (void *)data, file_name);

  data->is_safe = is_safe;
  data->destroy = destroy;
  stringbuffer_init(&data->file_name);
  if (!stringbuffer_append_all(&data->file_name, file_name))
  {
    DEBUGF("UserData: Failed to duplicate file name string\n");
    success = false;
  }
  else
  {
    linkedlist_insert(&user_data_list, NULL, &data->list_item);
  }

  return success;
}

/* ----------------------------------------------------------------------- */

unsigned int userdata_count_unsafe(void)
{
  unsigned int count = 0;

  DEBUGF("UserData: Counting unsafe user data items\n");
  userdata_for_each(count_unsafe_user_data, &count);
  DEBUGF("UserData: %u unsafe user data items\n", count);
  return count;
}

/* ----------------------------------------------------------------------- */

bool userdata_set_file_name(UserData *data, const char *file_name)
{
  bool success;

  assert(data != NULL);
  assert(file_name != NULL);
  DEBUGF("UserData: setting file name of user data %p to '%s'\n",
         (void *)data, file_name);

  stringbuffer_truncate(&data->file_name, 0);
  success = stringbuffer_append_all(&data->file_name, file_name);
  if (!success)
    stringbuffer_undo(&data->file_name);
  return success;
}

/* ----------------------------------------------------------------------- */

char *userdata_get_file_name(const UserData *data)
{
  assert(data != NULL);
  return stringbuffer_get_pointer(&data->file_name);
}

/* ----------------------------------------------------------------------- */

size_t userdata_get_file_name_length(const UserData *data)
{
  assert(data != NULL);
  return stringbuffer_get_length(&data->file_name);
}

/* ----------------------------------------------------------------------- */

_Optional UserData *userdata_find_by_file_name(const char *file_name)
{
  _Optional UserData *user_data;

  assert(file_name != NULL);
  DEBUGF("UserData: Searching for user data with file name '%s'\n",
         file_name);
  user_data = userdata_for_each(user_data_name_matches, (void *)file_name);
  if (user_data == NULL)
  {
    DEBUGF("UserData: No matching user data\n");
  }
  else
  {
    DEBUGF("UserData: Found matching user data %p\n", (void *)user_data);
  }
  return user_data;
}

/* ----------------------------------------------------------------------- */

void userdata_destroy(UserData *data)
{
  assert(data != NULL);
  DEBUGF("UserData: Destroying user data item %p with file name '%s'\n",
         (void *)data, stringbuffer_get_pointer(&data->file_name));

  if (!data->destroy)
    userdata_remove_from_list(data);
  else
    data->destroy(data);
}

/* ----------------------------------------------------------------------- */

void userdata_destroy_all(void)
{
  DEBUGF("UserData: Destroying all user data items\n");
  userdata_for_each(destroy_user_data, (void *)NULL);
}

/* ----------------------------------------------------------------------- */

_Optional UserData *userdata_for_each(UserDataCallbackFn *callback, void *arg)
{
  UserDataVisitorCtx visitor_context;

  visitor_context.callback = callback;
  visitor_context.arg = arg;

  return (UserData *)linkedlist_for_each(&user_data_list,
                                         user_data_visitor,
                                         &visitor_context);
}

/* ----------------------------------------------------------------------- */
/*                         Private functions                               */

static bool user_data_visitor(LinkedList *list, LinkedListItem *item, void *arg)
{
  UserData * const data = (UserData *)item;
  assert(arg);
  const UserDataVisitorCtx * const visitor_context = arg;

  assert(data != NULL);
  NOT_USED(list);
  assert(list == &user_data_list);
  assert(visitor_context->callback);

  return visitor_context->callback(data, visitor_context->arg);
}

/* ----------------------------------------------------------------------- */

static bool destroy_user_data(UserData *data, void *arg)
{
  NOT_USED(arg);
  userdata_destroy(data);
  return false; /* next item */
}

/* ----------------------------------------------------------------------- */

static bool count_unsafe_user_data(UserData *data, void *arg)
{
  unsigned int * const count = arg;
  bool is_safe = true;

  assert(data != NULL);
  assert(count != NULL);

  if (data->is_safe)
  {
    DEBUGF("UserData: Getting safe state of user data item %p\n",
           (void *)data);
    is_safe = data->is_safe(data);
  }

  if (!is_safe)
    ++(*count);

  DEBUGF("UserData: User data %p is %ssafe (count %u)\n", (void *)data,
         is_safe ? "" : "un", *count);

  return false; /* next item */
}

/* ----------------------------------------------------------------------- */

static bool user_data_name_matches(UserData *data, void *arg)
{
  const char * const file_name = arg;
  assert(arg != NULL);
  return !stricmp(userdata_get_file_name(data), file_name);
}
