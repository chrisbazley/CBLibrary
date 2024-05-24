/*
 * CBLibrary test: User data list
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

/* ISO library headers */
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <ctype.h>

/* CBLibrary headers */
#include "UserData.h"
#include "Macros.h"

/* Local headers */
#include "Tests.h"

enum
{
  NumberOfItems = 8,
  FortifyAllocationLimit = 2048,
};

static struct
{
  UserData *data;
  void *arg;
}
callbacks[NumberOfItems];

static unsigned int callback_count;

static bool record_callbacks(UserData *data, void *arg)
{
  assert(data != NULL);
  callbacks[callback_count].data = data;
  callbacks[callback_count++].arg = arg;
  printf("Callback %u\n", callback_count);
  return false;
}

static void record_destroys(UserData *data)
{
  assert(data != NULL);
  callbacks[callback_count++].data = data;
}

static bool record_is_safe(UserData *data)
{
  assert(data != NULL);
  callbacks[callback_count++].data = data;
  return (callback_count % 2) == 0;
}

static bool stop_iteration(UserData *data, void *arg)
{
  unsigned int *num_to_visit = arg;
  assert(data != NULL);
  assert(num_to_visit != NULL);
  return ++callback_count >= *num_to_visit;
}

static bool remove_in_callback(UserData *data, void *arg)
{
  assert(data != NULL);
  NOT_USED(arg);
  if (callback_count++ % 2)
    userdata_remove_from_list(data);
  return false;
}

static bool never_call_me(UserData *data, void *arg)
{
  NOT_USED(data);
  NOT_USED(arg);
  assert("List isn't empty" == NULL);
  return true;
}

static void test1(void)
{
  /* Initialize */
  userdata_init();
  userdata_for_each(never_call_me, NULL);
}

static void test2(void)
{
  /* Add user data */
  UserData data[NumberOfItems];
  unsigned int i;
  int dummy;

  memset(data, CHAR_MAX, sizeof(data));

  userdata_init();

  for (i = 0; i < ARRAY_SIZE(data); ++i)
  {
    const bool success = userdata_add_to_list(&data[i], NULL, NULL, "");
    assert(success);
  }

  /* List contains data[i-1], data[i-2], data[i-3] */
  callback_count = 0;
  userdata_for_each(record_callbacks, &dummy);

  assert(callback_count == ARRAY_SIZE(data));

  for (i = 0; i < callback_count; ++i)
  {
    assert(callbacks[i].data == &data[ARRAY_SIZE(data) - 1 - i]);
    assert(callbacks[i].arg == &dummy);
  }

  for (i = 0; i < ARRAY_SIZE(data); ++i)
    userdata_remove_from_list(&data[i]);
}

static void test3(void)
{
  /* Remove user data */
  UserData data[NumberOfItems];
  unsigned int i;

  memset(data, CHAR_MAX, sizeof(data));

  userdata_init();

  for (i = 0; i < ARRAY_SIZE(data); ++i)
  {
    const bool success = userdata_add_to_list(&data[i], NULL, NULL, "");
    assert(success);
  }

  /* List contains data[i-1], data[i-2], data[i-3] */

  for (; i > 0; --i)
  {
    unsigned int j;
    int dummy;

    userdata_remove_from_list(&data[i-1]);

    callback_count = 0;
    userdata_for_each(record_callbacks, &dummy);

    assert(callback_count == i-1);

    for (j = 0; j < callback_count; ++j)
    {
      assert(i >= 2);
      assert(i - 2 >= j);
      assert(callbacks[j].data == &data[i - 2 - j]);
      assert(callbacks[j].arg == &dummy);
    }
  }
}

static void test4(void)
{
  /* Destroy all with defaults */
  UserData data[NumberOfItems];
  unsigned int i;
  int dummy;

  memset(data, CHAR_MAX, sizeof(data));

  userdata_init();

  /* If 'destroy' is NULL then the data is removed from the list but
     no callback occurs. */
  for (i = 0; i < ARRAY_SIZE(data); ++i)
  {
    const bool success = userdata_add_to_list(&data[i], NULL, i % 2 ? record_destroys : NULL, "");
    assert(success);
  }

  callback_count = 0;
  userdata_destroy_all();

  /* The destructor should have been called for only
     half of the user data */
  assert(callback_count == ARRAY_SIZE(data)/2);

  for (i = 0; i < callback_count; ++i)
    assert(callbacks[i].data == &data[ARRAY_SIZE(data) - 1 - i*2]);

  callback_count = 0;
  userdata_for_each(record_callbacks, &dummy);

  /* Only half of the user data should have been removed
     (items without destructors) */
  assert(callback_count == ARRAY_SIZE(data)/2);

  for (i = 0; i < callback_count; ++i)
  {
    assert(callbacks[i].data == &data[ARRAY_SIZE(data) - 1 - i*2]);
    assert(callbacks[i].arg == &dummy);
  }
}

static void test5(void)
{
  /* Stop iteration */
  UserData data[NumberOfItems];
  unsigned int i, num_to_visit = ARRAY_SIZE(data) / 2;

  memset(data, CHAR_MAX, sizeof(data));

  userdata_init();

  for (i = 0; i < ARRAY_SIZE(data); ++i)
  {
    const bool success = userdata_add_to_list(&data[i], NULL, NULL, "");
    assert(success);
  }

  callback_count = 0;
  userdata_for_each(stop_iteration, &num_to_visit);
  assert(callback_count == num_to_visit);

  for (i = 0; i < ARRAY_SIZE(data); ++i)
    userdata_remove_from_list(&data[i]);
}

static void test6(void)
{
  /* Remove in callback */
  UserData data[NumberOfItems];
  unsigned int i;
  int dummy;

  memset(data, CHAR_MAX, sizeof(data));

  userdata_init();

  for (i = 0; i < ARRAY_SIZE(data); ++i)
  {
    const bool success = userdata_add_to_list(&data[i], NULL, NULL, "");
    assert(success);
  }

  callback_count = 0;
  userdata_for_each(remove_in_callback, NULL);
  assert(callback_count == ARRAY_SIZE(data));

  callback_count = 0;
  userdata_for_each(record_callbacks, &dummy);
  assert(callback_count == ARRAY_SIZE(data)/2);

  for (i = 0; i < callback_count; ++i)
  {
    assert(callbacks[i].data == &data[ARRAY_SIZE(data) - 1 - i*2]);
    assert(callbacks[i].arg == &dummy);
  }

  for (i = 1; i < ARRAY_SIZE(data); i+=2)
    userdata_remove_from_list(&data[i]);
}

static void test7(void)
{
  /* Count unsafe with defaults */
  UserData data[NumberOfItems];
  unsigned int i, unsafe_count;

  memset(data, CHAR_MAX, sizeof(data));

  userdata_init();

  /* If 'is_safe' is NULL then the data is assumed to be safe to destroy
     and no callback occurs. */
  for (i = 0; i < ARRAY_SIZE(data); ++i)
  {
    const bool success = userdata_add_to_list(&data[i], i % 2 ? record_is_safe : NULL, NULL, "");
    assert(success);
  }

  callback_count = 0;
  unsafe_count = userdata_count_unsafe();
  assert(unsafe_count == ARRAY_SIZE(data)/4);
  assert(callback_count == ARRAY_SIZE(data)/2);

  for (i = 0; i < callback_count; ++i)
    assert(callbacks[i].data == &data[ARRAY_SIZE(data) - 1 - i*2]);

  for (i = 0; i < ARRAY_SIZE(data); ++i)
    userdata_remove_from_list(&data[i]);
}

static void test8(void)
{
  /* Count unsafe */
  UserData data[NumberOfItems];
  unsigned int i, unsafe_count;

  memset(data, CHAR_MAX, sizeof(data));

  userdata_init();

  for (i = 0; i < ARRAY_SIZE(data); ++i)
  {
    const bool success = userdata_add_to_list(&data[i], record_is_safe, NULL, "");
    assert(success);
  }

  callback_count = 0;
  unsafe_count = userdata_count_unsafe();
  assert(unsafe_count == ARRAY_SIZE(data)/2);
  assert(callback_count == ARRAY_SIZE(data));

  for (i = 0; i < callback_count; ++i)
    assert(callbacks[i].data == &data[ARRAY_SIZE(data) - 1 - i]);

  for (i = 0; i < ARRAY_SIZE(data); ++i)
    userdata_remove_from_list(&data[i]);
}

static void test9(void)
{
  /* Find by name */
  unsigned int i;
  static const char * const names[] =
  {
    "Clive's",
    "Disc",
    "BOX"
  };
  char upper_case[64];
  UserData data[ARRAY_SIZE(names)];

  memset(data, CHAR_MAX, sizeof(data));

  userdata_init();

  for (i = 0; i < ARRAY_SIZE(data); ++i)
  {
    const bool success = userdata_add_to_list(&data[i], NULL, NULL, names[i]);
    assert(success);
  }

  for (i = 0; i < ARRAY_SIZE(data); ++i)
    assert(userdata_find_by_file_name(names[i]) == &data[i]);

  /* Check that string comparison is case-insensitive */
  for (i = 0; i <= strlen(names[0]); ++i)
    upper_case[i] = toupper(names[0][i]);

  assert(userdata_find_by_file_name(upper_case) == &data[0]);

  /* Check the string not found case */
  assert(userdata_find_by_file_name("Neon") == NULL);

  for (i = 0; i < ARRAY_SIZE(data); ++i)
    userdata_remove_from_list(&data[i]);
}

static void test10(void)
{
  /* Set name */
  static const char * const names[] =
  {
    "RAM::RamDisc0.$.temp",
    "IDEFS::4.$.baz.bar.foo",
    "ADFS::0.$.foo"
  };
  UserData data;
  unsigned int i;
  bool success;
  const char *got_name;
  size_t got_len;

  memset(&data, CHAR_MAX, sizeof(data));

  userdata_init();

  success = userdata_add_to_list(&data, NULL, NULL, "");
  assert(success);

  got_name = userdata_get_file_name(&data);
  assert(*got_name == '\0');

  got_len = userdata_get_file_name_length(&data);
  assert(got_len == 0);

  for (i = 0; i < ARRAY_SIZE(names); ++i)
  {
    success = userdata_set_file_name(&data, names[i]);
    assert(success);

    got_name = userdata_get_file_name(&data);
    assert(!strcmp(got_name, names[i]));

    got_len = userdata_get_file_name_length(&data);
    assert(got_len == strlen(names[i]));
  }

  userdata_remove_from_list(&data);
}

static void test11(void)
{
  /* Add user data fail recovery */
  unsigned long limit;
  UserData data[NumberOfItems];
  unsigned int i;
  bool success;

  memset(data, CHAR_MAX, sizeof(data));

  userdata_init();

  for (i = 0; i < ARRAY_SIZE(data)-1; ++i)
  {
    success = userdata_add_to_list(&data[i], NULL, NULL, "");
    assert(success);
  }

  for (limit = 0; limit < FortifyAllocationLimit; ++limit)
  {
    unsigned int j;

    Fortify_SetNumAllocationsLimit(limit);
    success = userdata_add_to_list(&data[i], NULL, NULL, "notempty");
    Fortify_SetNumAllocationsLimit(ULONG_MAX);

    if (success)
    {
      ++i;
      limit = FortifyAllocationLimit; /* success - test complete */
    }

    /* Check that the state of the list is unchanged on error,
       or finally the item was added if successful */
    callback_count = 0;
    userdata_for_each(record_callbacks, NULL);
    printf("%u == %u\n", callback_count, i);
    assert(callback_count == i);

    for (j = 0; j < callback_count; ++j)
      assert(callbacks[j].data == &data[i - 1 - j]);
  }
  assert(limit != FortifyAllocationLimit);

  for (i = 0; i < ARRAY_SIZE(data); ++i)
    userdata_remove_from_list(&data[i]);
}

static void test12(void)
{
  /* Set name fail recovery */
  unsigned long limit;
  UserData data;
  bool success;
  const char *got_name, *expected;
  size_t got_len;
  static const char * const names[2] =
  {
    "foo",
    "longerthanfoo",
  };

  memset(&data, CHAR_MAX, sizeof(data));

  userdata_init();

  expected = names[0];
  success = userdata_add_to_list(&data, NULL, NULL, expected);
  assert(success);

  for (limit = 0; limit < FortifyAllocationLimit; ++limit)
  {
    Fortify_SetNumAllocationsLimit(limit);
    success = userdata_set_file_name(&data, names[1]);
    Fortify_SetNumAllocationsLimit(ULONG_MAX);

    if (success)
    {
      expected = names[1];
      limit = FortifyAllocationLimit; /* success - test complete */
    }

    /* Check that the file name is unchanged on error,
       or finally changed if successful */
    got_name = userdata_get_file_name(&data);
    assert(!strcmp(got_name, expected));

    got_len = userdata_get_file_name_length(&data);
    assert(got_len == strlen(expected));
  }
  assert(limit != FortifyAllocationLimit);

  userdata_remove_from_list(&data);
}

static void test13(void)
{
  /* Destroy user data with destructor */
  UserData data[NumberOfItems];
  unsigned int i;

  memset(data, CHAR_MAX, sizeof(data));

  userdata_init();

  for (i = 0; i < ARRAY_SIZE(data); ++i)
  {
    const bool success = userdata_add_to_list(&data[i], NULL, record_destroys, "");
    assert(success);
  }

  /* List contains data[i-1], data[i-2], data[i-3] */

  for (; i > 0; --i)
  {
    unsigned int j;
    int dummy;

    callback_count = 0;
    userdata_destroy(&data[i-1]);

    assert(callback_count == 1);
    assert(callbacks[0].data == &data[i-1]);

    callback_count = 0;
    userdata_for_each(record_callbacks, &dummy);

    assert(callback_count == ARRAY_SIZE(data));

    for (j = 0; j < callback_count; ++j)
    {
      assert(callbacks[j].data == &data[ARRAY_SIZE(data) - 1 - j]);
      assert(callbacks[j].arg == &dummy);
    }
  }
}

static void test14(void)
{
  /* Destroy user data without destructor */
  UserData data[NumberOfItems];
  unsigned int i;

  memset(data, CHAR_MAX, sizeof(data));

  userdata_init();

  for (i = 0; i < ARRAY_SIZE(data); ++i)
  {
    const bool success = userdata_add_to_list(&data[i], NULL, NULL, "");
    assert(success);
  }

  /* List contains data[i-1], data[i-2], data[i-3] */

  for (; i > 0; --i)
  {
    unsigned int j;
    int dummy;

    userdata_destroy(&data[i-1]);

    callback_count = 0;
    userdata_for_each(record_callbacks, &dummy);

    assert(callback_count == i-1);

    for (j = 0; j < callback_count; ++j)
    {
      assert(i >= 2);
      assert(i - 2 >= j);
      assert(callbacks[j].data == &data[i - 2 - j]);
      assert(callbacks[j].arg == &dummy);
    }
  }
}

void UserData_tests(void)
{
  static const struct
  {
    const char *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Initialize", test1 },
    { "Add user data", test2 },
    { "Remove user data", test3 },
    { "Destroy all with defaults", test4 },
    { "Stop iteration", test5 },
    { "Remove in callback", test6 },
    { "Count unsafe with defaults", test7 },
    { "Count unsafe", test8 },
    { "Find by name", test9 },
    { "Set name", test10 },
    { "Add user data fail recovery", test11 },
    { "Set name fail recovery", test12 },
    { "Destroy user data with destructor", test13 },
    { "Destroy user data without destructor", test14 }
  };

  for (size_t count = 0; count < ARRAY_SIZE(unit_tests); count ++)
  {
    printf("Test %zu/%zu : %s\n",
           1 + count,
           ARRAY_SIZE(unit_tests),
           unit_tests[count].test_name);

    Fortify_EnterScope();

    unit_tests[count].test_func();

    Fortify_LeaveScope();
  }
}
