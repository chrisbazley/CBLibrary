/*
 * CBLibrary test: Find a given no. of elements at the tail of a file path
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
#include <stdio.h>
#include <limits.h>

/* CBLibrary headers */
#include "PathTail.h"
#include "Macros.h"

/* Local headers */
#include "Tests.h"

#define PATH_1 "ADFS::Rissa"
#define PATH_2 PATH_1 ".$"
#define PATH_3 PATH_2 ".Programming"
#define PATH_4 PATH_3 ".AcornC/C++"

static void test1(void)
{
  /* 1 components */
  const char *path = PATH_4;
  const char *tail = pathtail(path, 1);
  assert(tail == path + sizeof(PATH_3));
  assert(strcmp(path, PATH_4)==0);
}

static void test2(void)
{
  /* 2 components */
  const char *path = PATH_4;
  const char *tail = pathtail(path, 2);
  assert(tail == path + sizeof(PATH_2));
  assert(strcmp(path, PATH_4)==0);
}

static void test3(void)
{
  /* 3 components */
  const char *path = PATH_4;
  const char *tail = pathtail(path, 3);
  assert(tail == path + sizeof(PATH_1));
  assert(strcmp(path, PATH_4)==0);
}

static void test4(void)
{
  /* All components */
  const char *path = PATH_4;
  const char *tail = pathtail(path, 4);
  assert(tail == path);
  assert(strcmp(path, PATH_4)==0);
}

static void test5(void)
{
  /* Too many components */
  const char *path = PATH_4;
  const char *tail = pathtail(path, 5);
  assert(tail == path);
  assert(strcmp(path, PATH_4)==0);
}

static void test6(void)
{
  /* No component */
  const char *path = PATH_4;
  const char *tail = pathtail(path, 0);
  assert(tail == path + sizeof(PATH_4));
  assert(strcmp(path, PATH_4)==0);
}

static void test7(void)
{
  /* No path separator */
  const char *path = PATH_1;
  const char *tail = pathtail(path, 1);
  assert(tail == path);
  assert(strcmp(path, PATH_1)==0);
}

static void test8(void)
{
  /* Leading path separator */
  const char *path = ".foo";
  const char *tail = pathtail(path, 1);
  assert(tail == path + 1);
  assert(strcmp(path, ".foo")==0);
}

static void test9(void)
{
  /* Trailing path separator */
  const char *path = "foo.";
  const char *tail = pathtail(path, 1);
  assert(tail == path + sizeof("foo"));
  assert(strcmp(path, "foo.")==0);
}

void PathTail_tests(void)
{
  static const struct
  {
    const char *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "1 component", test1 },
    { "2 components", test2 },
    { "3 components", test3 },
    { "All components", test4 },
    { "Too many components", test5 },
    { "No components", test6 },
    { "No path separator", test7 },
    { "Leading path separator", test8 },
    { "Trailing path separator", test9 }
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
