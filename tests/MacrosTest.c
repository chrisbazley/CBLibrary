/*
 * CBLibrary test: Useful macro definitions
 * Copyright (C) 2012 Christopher Bazley
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
#include <string.h>
#include <stdio.h>

/* CBLibrary headers */
#include "Macros.h"

/* Local headers */
#include "Tests.h"

static const char *nullptr = NULL;

static void test1(void)
{
  /* STRING_OR_NULL with strings */
  static const char * const strings[] =
  {
    "foo", "twig", "p", "", "\n", "\t", "\r"
  };
  for (size_t i = 0; i < sizeof(strings) / sizeof(strings[0]); ++i)
  {
    const char * const s = STRING_OR_NULL(strings[i]);
    assert(s != NULL);
    assert(s == strings[i]);
  }
}

static void test2(void)
{
  /* STRING_OR_NULL with null pointer */
  const char * const s = STRING_OR_NULL(nullptr);
  assert(s != NULL);
  assert(*s == '\0');
}

void Macros_tests(void)
{
  static const struct
  {
    const char *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "STRING_OR_NULL with strings", test1 },
    { "STRING_OR_NULL with null pointer", test2 }
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
