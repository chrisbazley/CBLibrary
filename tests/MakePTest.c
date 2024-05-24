/*
 * CBLibrary test: Create directories in a file path
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
#include <stdio.h>
#include <string.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "swis.h"

/* CBLibrary headers */
#include "FileUtils.h"
#include "Macros.h"

/* Local headers */
#include "Tests.h"

#define PATH_1 "<Wimp$ScrapDir>"
#define PATH_2 PATH_1 ".MakePathTest"
#define PATH_3 PATH_2 ".Foo"
#define PATH_4 PATH_3 ".Bar"
#define PATH_5 PATH_4 ".Baz"

enum
{
  OS_FSControl_Wipe = 27,
  OS_FSControl_Flag_Recurse = 1,
  OS_File_CreateStampedFile = 11,
  OS_File_CreateDirectory = 8,
  OS_File_CreateDirectory_DefaultNoOfEntries = 0,
  OS_File_ReadCatalogueInfo = 17
};

static void wipe(const char *path_name)
{
  _kernel_swi_regs regs;

  assert(path_name != NULL);

  regs.r[0] = OS_FSControl_Wipe;
  regs.r[1] = (int)path_name;
  regs.r[3] = OS_FSControl_Flag_Recurse;
  _kernel_swi(OS_FSControl, &regs, &regs);
}

static int osfile(int op, const char *name, _kernel_osfile_block *inout)
{
  int err;

  assert(name != NULL);
  assert(inout != NULL);

  err = _kernel_osfile(op, name, inout);
  if (err == _kernel_ERROR)
  {
    const _kernel_oserror * const e = _kernel_last_oserror();
    assert(e != NULL);
    printf("Error 0x%x %s\n", e->errnum, e->errmess);
    exit(EXIT_FAILURE);
  }
  return err;
}

static void create_dir(const char *path_name)
{
  _kernel_osfile_block inout;

  assert(path_name != NULL);
  inout.start = OS_File_CreateDirectory_DefaultNoOfEntries;
  osfile(OS_File_CreateDirectory, path_name, &inout);
}

static void create_file(const char *path_name, int type, int size)
{
  _kernel_osfile_block inout;

  assert(path_name != NULL);
  inout.load = type;
  inout.start = 0;
  inout.end = size;
  osfile(OS_File_CreateStampedFile, path_name, &inout);
}

static int read_obj_type(const char *path_name)
{
  _kernel_osfile_block inout;
  int obj_type;

  assert(path_name != NULL);
  obj_type = osfile(OS_File_ReadCatalogueInfo, path_name, &inout);

  return obj_type;
}

/* Mutable array in case string literals are in read-only memory */
static char paths[][100] =
{
  PATH_1, PATH_2, PATH_3, PATH_4, PATH_5
};

static void check_path(void)
{
  /* Last element shouldn't exist. Other elements should be directories. */
  for (size_t i = 0; i < ARRAY_SIZE(paths); ++i)
  {
    const int obj_type = read_obj_type(paths[i]);
    if (i == ARRAY_SIZE(paths)-1) {
      assert((obj_type == ObjectType_NotFound) || (obj_type == ObjectType_File));
    } else {
      assert(obj_type == ObjectType_Directory);
    }
  }
}

static void test1(void)
{
  /* Make whole path */
  CONST _kernel_oserror *e;

  /* Start with a cleanish state (don't delete Scrap directory) */
  wipe(paths[1]);

  e = make_path(paths[4], 0);
  assert(e == NULL);
  check_path();
}

static void test2(void)
{
  /* Make partial path */
  CONST _kernel_oserror *e;

  /* Start with a cleanish state (don't delete Scrap directory) */
  wipe(paths[1]);

  /* Create the first directory */
  create_dir(paths[1]);

  /* Create the remaining directories */
  e = make_path(paths[4], strlen(paths[1])+1);
  assert(e == NULL);
  check_path();
}

static void test3(void)
{
  /* Make degenerate partial path */
  CONST _kernel_oserror *e;

  /* Start with a cleanish state (don't delete Scrap directory) */
  wipe(paths[1]);

  /* Create the first directory */
  create_dir(paths[1]);

  /* Create the remaining directories */
  e = make_path(paths[4], strlen(paths[1])+2);
  assert(e == NULL);
  check_path();
}

static void test4(void)
{
  /* Make existing path */
  size_t i;
  CONST _kernel_oserror *e;

  /* Start with a cleanish state (don't delete Scrap directory) */
  wipe(paths[1]);

  /* Create all path elements except the last as directories */
  for (i = 0; i < ARRAY_SIZE(paths)-1; ++i) {
    create_dir(paths[i]);
  }
  create_file(paths[i], FileType_Text, 1);

  /* Make the whole path (already exists) */
  e = make_path(paths[4], 0);
  assert(e == NULL);
  check_path();
}

void MakePath_tests(void)
{
  static const struct
  {
    const char *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Make whole path", test1 },
    { "Make partial path", test2 },
    { "Make degenerate partial path", test3 },
    { "Make existing path", test4 }
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
