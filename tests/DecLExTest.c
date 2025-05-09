/*
 * CBLibrary test: Decode the load and execution addresses of an object
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
#include <stdbool.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>

/* CBLibrary headers */
#include "DateStamp.h"
#include "Macros.h"

/* Local headers */
#include "Tests.h"

static void test1(void)
{
  /* Typed file */
  int file_type;
  OSDateAndTime utc;
  static const char expected[sizeof(utc.bytes)] =
  {
    0xCD, 0xAB, 0x89, 0x67, 0x45
  };
  const int load = (int)0xfff12345;
  const int exec = 0x6789ABCD;

  memset(utc.bytes, CHAR_MAX, sizeof(utc.bytes));
  file_type = decode_load_exec(load, exec, &utc);
  assert(file_type == 0x123);
  assert(memcmp(utc.bytes, expected, sizeof(utc.bytes)) == 0);

  file_type = decode_load_exec(load, exec, NULL);
  assert(file_type == 0x123);
}

static void test2(void)
{
  /* Untyped file */
  int file_type;
  OSDateAndTime utc;
  static const char expected[sizeof(utc.bytes)];
  const int load = 0x12345678;
  const int exec = 0x76543210;

  memset(utc.bytes, CHAR_MAX, sizeof(utc.bytes));
  file_type = decode_load_exec(load, exec, &utc);
  assert(file_type == FileType_None);
  assert(memcmp(utc.bytes, expected, sizeof(utc.bytes)) == 0);

  file_type = decode_load_exec(load, exec, NULL);
  assert(file_type == FileType_None);
}

void DecodeLExe_tests(void)
{
  static const struct
  {
    const char *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Typed file", test1 },
    { "Untyped file", test2 }
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
