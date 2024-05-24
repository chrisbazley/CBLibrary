/*
 * CBLibrary test: Directory tree iterator
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
#include <string.h>
#include <limits.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "swis.h"

/* CBLibrary headers */
#include "DirIter.h"
#include "Macros.h"

/* Local headers */
#include "Tests.h"

#define EMPTY_PATH "<Wimp$ScrapDir>.DirIterTest.empty"

enum
{
  ErrorNum_BufferOverflow = 705,
  Territory_Current = -1,
  OS_FSControl_Wipe = 27,
  OS_FSControl_Flag_Recurse = 1,
  OS_File_CreateStampedFile = 11,
  OS_File_CreateDirectory = 8,
  OS_File_CreateDirectory_DefaultNoOfEntries = 0,
  OS_File_Attribute_ReadForYou = 1,
  OS_File_Attribute_WriteForYou = 2,
  ErrorNum_DirectoryDoesNotExist = 214,
  FortifyAllocationLimit = 2048,
  StringBufferSize = 256,
  NumberOfIterators = 5,
  NumberOfAdvances = 10
};

static struct
{
  const char *name;
  int type;
  int size;
}
test_objects[] =
{
  {
    "<Wimp$ScrapDir>.DirIterTest",
    FileType_Directory,
    0
  },
  {
    "<Wimp$ScrapDir>.DirIterTest.!foo",
    FileType_Application,
    2048
  },
  {
    "<Wimp$ScrapDir>.DirIterTest.!foo.bar",
    FileType_Squash,
    0
  },
  {
    "<Wimp$ScrapDir>.DirIterTest.!foo.noob",
    FileType_Data,
    13
  },
  {
    "<Wimp$ScrapDir>.DirIterTest.fee",
    FileType_Text,
    27
  },
  {
    "<Wimp$ScrapDir>.DirIterTest.fi",
    FileType_Obey,
    31
  },
  {
    "<Wimp$ScrapDir>.DirIterTest.foo",
    FileType_Directory,
    2048
  },
  {
    "<Wimp$ScrapDir>.DirIterTest.foo.fum",
    FileType_Directory,
    2048
  },
  {
    "<Wimp$ScrapDir>.DirIterTest.IfMyFella'sInAHurryHe'llNeverHaveToWorryHeKnowsThatI'maNaturalGirlINeverWorryIfI'mShowingWithAllThatI'veGotGoingIJustWashMyFaceForgetAboutCurls!",
    FileType_Sprite,
    2048
  }
};

static bool pattern_match(const char *string, const char *pattern)
{
  bool match = true;

  assert(string != NULL);
  if (pattern == NULL)
    pattern = "*";

  printf("Trying to match '%s' with pattern '%s'\n", string, pattern);

  for (; *pattern != '\0' && match; ++pattern)
  {
    switch (*pattern)
    {
      case '#': /* Match any single character */
        if (*string != '\0')
        {
          ++string;
        }
        else
        {
          puts("String too short");
          match = false;
        }
        break;

      case '*': /* Match any 0 or more characters */
        while (*string != '\0' && *string != pattern[1])
          string++;

        break;

      default: /* Match one character from the pattern */
        if (*string == *pattern)
        {
          ++string;
        }
        else
        {
          printf("Mismatch of %c with %c\n", *string, *pattern);
          match = false;
        }
        break;
    }
  }

  if (*string != '\0')
  {
    puts("String too long");
    match = false;
  }

  if (match)
    puts("Successful match");

  return match;
}

static void wipe(const char *path_name)
{
  _kernel_swi_regs regs;

  assert(path_name != NULL);

  regs.r[0] = OS_FSControl_Wipe;
  regs.r[1] = (int)path_name;
  regs.r[3] = OS_FSControl_Flag_Recurse;
  _kernel_swi(OS_FSControl, &regs, &regs);
}

static void osfile(int op, const char *name, _kernel_osfile_block *inout)
{
  const int err = _kernel_osfile(op, name, inout);
  if (err == _kernel_ERROR)
  {
    const _kernel_oserror * const e = _kernel_last_oserror();
    assert(e != NULL);
    printf("Error 0x%x %s\n", e->errnum, e->errmess);
    exit(EXIT_FAILURE);
  }
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

static void init(void)
{
  unsigned int i;

  wipe(test_objects[0].name);

  for (i = 0; i < ARRAY_SIZE(test_objects); ++i)
  {
    switch (test_objects[i].type)
    {
      case FileType_Directory:
      case FileType_Application:
        create_dir(test_objects[i].name);
        break;

      default:
        create_file(test_objects[i].name,
                    test_objects[i].type,
                    test_objects[i].size);
        break;
    }
  }
}

static void final(void)
{
  wipe(test_objects[0].name);
}

static int date_and_time_to_string(OSDateAndTime *utc,
                                   char *buffer,
                                   size_t buff_size)
{
  _kernel_swi_regs regs;
  _kernel_oserror *e;
  /* This SWI doesn't tell you the required buffer size on
     buffer overflow, but luckily it is entirely predictable. */
  int nchars = sizeof("00:00:00 01 Jan 1900")-1;

  regs.r[0] = Territory_Current;
  regs.r[1] = (int)utc->bytes;
  regs.r[2] = (int)buffer;
  regs.r[3] = (int)buff_size;
  regs.r[4] = (int)"%24:%MI:%SE %DY %M3 %CE%YR";
  e = _kernel_swi(Territory_ConvertDateAndTime, &regs, &regs);
  if (e != NULL && e->errnum != ErrorNum_BufferOverflow)
  {
    nchars = -1;
  }

  return nchars;
}

static void validate_object_info(DirIteratorObjectInfo *info, int object_type, size_t i)
{
  unsigned int expected_attributes;
  char buffer[StringBufferSize];

  assert(i < ARRAY_SIZE(test_objects));

  int n = date_and_time_to_string(
                   &info->date_stamp, buffer, sizeof(buffer));
  assert(n >= 0);
  assert(n < (int)sizeof(buffer));
  printf("object type: %d\n", object_type);

  switch (test_objects[i].type)
  {
    case FileType_Directory:
    case FileType_Application:
      assert(object_type == ObjectType_Directory);
      expected_attributes = 0;
      break;

    default:
      assert(object_type == ObjectType_File);
      expected_attributes = (OS_File_Attribute_ReadForYou |
                             OS_File_Attribute_WriteForYou);
      break;
  }

  if (info != NULL)
  {
    n = date_and_time_to_string(&info->date_stamp, buffer, sizeof(buffer));
    assert(n >= 0);
    assert(n < (int)sizeof(buffer));
    printf("date stamp:%s\nlength: %ld\nattributes: 0x%x\nfile type: 0x%x\n",
           buffer, info->length, info->attributes, info->file_type);

    assert(info->length == test_objects[i].size);
    assert(info->file_type == test_objects[i].type);
    assert(info->attributes == expected_attributes);
  }
}

static void validate_object(const DirIterator *it, size_t i)
{
  DirIteratorObjectInfo info;
  char buffer[StringBufferSize];

  assert(it != NULL);
  assert(i < ARRAY_SIZE(test_objects));

  assert(!diriterator_is_empty(it));

  /* Find last path separator */
  const char *expected = strrchr(test_objects[i].name, '.');
  assert(expected != NULL);
  ++expected;

  size_t n = diriterator_get_object_leaf_name(it, buffer, sizeof(buffer));
  assert(n == strlen(expected));
  puts(buffer);
  assert(strcmp(buffer, expected) == 0);

  assert(strlen(test_objects[i].name) > strlen(test_objects[0].name) + 1);
  expected = test_objects[i].name + strlen(test_objects[0].name) + 1;

  n = diriterator_get_object_sub_path_name(it, buffer, sizeof(buffer));
  assert(n == strlen(expected));
  puts(buffer);
  assert(strcmp(buffer, expected) == 0);

  expected = test_objects[i].name;

  n = diriterator_get_object_path_name(it, buffer, sizeof(buffer));
  assert(n == strlen(expected));
  puts(buffer);
  assert(strcmp(buffer, expected) == 0);

  const int object_type = diriterator_get_object_info(it, &info);
  validate_object_info(&info, object_type, i);
}

static size_t find_next(size_t i, unsigned int flags, const char *pattern)
{
  for (; i < ARRAY_SIZE(test_objects); ++i)
  {
    /* Does the last separator in the path coincide with the end of
       the root directory path? */
    const char * const last_sep = strrchr(test_objects[i].name, '.');
    assert(last_sep != NULL);
    if ( ( (flags & DirIterator_RecurseIntoDirectories) ||
            last_sep == test_objects[i].name +
                        strlen(test_objects[0].name) ) &&
         pattern_match(last_sep+1, pattern) )
    {
      break; /* yes: expect this catalogue entry to be included */
    }

    printf("Skipping entry '%s'\n", test_objects[i].name);
  }

  return i;
}

static void check_buffer(char *buffer, size_t buff_size, const char *expected, size_t n)
{
  assert(buffer != NULL);
  assert(expected != NULL);

  const size_t expected_size = LOWEST(strlen(expected) + 1, buff_size);
  if (expected_size > 0)
  {
    /* Check that the truncated string was returned correctly */
    puts(buffer);
    assert(strncmp(buffer, expected, expected_size - 1) == 0);
    assert(buffer[expected_size - 1] == '\0');
  }

  /* Check that any free bytes following the nul terminator are untouched */
  for (size_t i = expected_size; i < StringBufferSize; ++i)
    assert(buffer[i] == CHAR_MAX);

  assert(n == strlen(expected));
}

static void check_empty(DirIterator *it)
{
  assert(it);

  for (int i = 0; i < NumberOfAdvances; ++i)
  {
    DirIteratorObjectInfo info;
    char buffer[StringBufferSize];

    assert(diriterator_is_empty(it));

    assert(diriterator_get_object_info(it, NULL) == ObjectType_NotFound);
    assert(diriterator_get_object_info(it, &info) == ObjectType_NotFound);

    assert(diriterator_get_object_path_name(it, NULL, 0) == 0);

    memset(buffer, CHAR_MAX, sizeof(buffer));
    size_t n = diriterator_get_object_path_name(it, buffer, sizeof(buffer));
    check_buffer(buffer, sizeof(buffer), "", n);

    assert(diriterator_get_object_leaf_name(it, NULL, 0) == 0);

    memset(buffer, CHAR_MAX, sizeof(buffer));
    n = diriterator_get_object_leaf_name(it, buffer, sizeof(buffer));
    check_buffer(buffer, sizeof(buffer), "", n);

    assert(diriterator_get_object_sub_path_name(it, NULL, 0) == 0);

    memset(buffer, CHAR_MAX, sizeof(buffer));
    n = diriterator_get_object_sub_path_name(it, buffer, sizeof(buffer));
    check_buffer(buffer, sizeof(buffer), "", n);

    /* Advancing an empty iterator should have no effect */
    diriterator_advance(it);
  }
}

static void simple_test(unsigned int flags, const char *pattern)
{
  const _kernel_oserror *e;
  DirIterator *it;
  size_t i;

  for (e = diriterator_make(&it, flags, test_objects[0].name, pattern), i = 1;
       !diriterator_is_empty(it);
       e = diriterator_advance(it), ++i)
  {
    assert(e == NULL);

    /* Skip entries excluded because of no recursion or pattern mismatch */
    i = find_next(i, flags, pattern);

    /* Validate the current object against the array from which the
       directory tree was generated */
    validate_object(it, i);
  }
  assert(e == NULL);

  /* Check that the iterator didn't become empty too early */
  i = find_next(i, flags, pattern);
  assert(i >= ARRAY_SIZE(test_objects));

  /* Check that the iterator stays in the correct empty state */
  check_empty(it);

  diriterator_destroy(it);
}

static void test1(void)
{
  /* Make/destroy */
  DirIterator *it[NumberOfIterators];

  for (size_t i = 0; i < ARRAY_SIZE(it); i++)
  {
    const _kernel_oserror * const e = diriterator_make(
                                          &it[i],
                                          0,
                                          test_objects[0].name,
                                          NULL);
    assert(e == NULL);
  }

  for (size_t i = 0; i < ARRAY_SIZE(it); i++)
  {
    assert(it[i] != NULL);
    assert(!diriterator_is_empty(it[i]));
  }

  for (size_t i = 0; i < ARRAY_SIZE(it); i++)
    diriterator_destroy(it[i]);
}

static void test2(void)
{
  /* No recursion */
  simple_test(0, NULL);
}

static void test3(void)
{
  /* Directory recursion */
  simple_test(DirIterator_RecurseIntoDirectories, NULL);
}

static void test4(void)
{
  /* No recursion with pattern */
  simple_test(0, "*fo#");
}

static void test5(void)
{
  /* Directory recursion with pattern */
  simple_test(DirIterator_RecurseIntoDirectories, "*oo*");
}

static void test6(void)
{
  /* Make from empty directory without recursion */
  DirIterator *it;

  create_dir(EMPTY_PATH);
  const _kernel_oserror * const e = diriterator_make(&it, 0, EMPTY_PATH, NULL);
  wipe(EMPTY_PATH);

  assert(e == NULL);
  check_empty(it);

  diriterator_destroy(it);
}

static void test7(void)
{
  /* Make from empty directory with recursion */
  DirIterator *it;

  create_dir(EMPTY_PATH);
  const _kernel_oserror * const e = diriterator_make(
    &it, DirIterator_RecurseIntoDirectories, EMPTY_PATH, NULL);
  wipe(EMPTY_PATH);

  assert(e == NULL);
  check_empty(it);

  diriterator_destroy(it);
}

static void test8(void)
{
  /* Make from missing directory without recursion */
  DirIterator *it;
  const _kernel_oserror * const e = diriterator_make(
    &it, 0, "<Wimp$ScrapDir>.DirIterTest.missing", NULL);
  assert(e != NULL);
  assert(e->errnum == ErrorNum_DirectoryDoesNotExist);
  assert(it == NULL);
}

static void test9(void)
{
  /* Make from missing directory with recursion */
  DirIterator *it;
  const _kernel_oserror * const e = diriterator_make(
    &it, DirIterator_RecurseIntoDirectories, "<Wimp$ScrapDir>.DirIterTest.missing", NULL);
  assert(e != NULL);
  assert(e->errnum == ErrorNum_DirectoryDoesNotExist);
  assert(it == NULL);
}

static void test10(void)
{
  /* Get leaf name */
  DirIterator *it;
  const _kernel_oserror *e = diriterator_make(
    &it, DirIterator_RecurseIntoDirectories, test_objects[0].name, NULL);
  assert(e == NULL);
  assert(it != NULL);

  /* Advance past the first object because we want a sub-path with at least
     two elements */
  e = diriterator_advance(it);
  assert(e == NULL);

  /* Find last path separator */
  const char * const last_sep = strrchr(test_objects[2].name, '.');
  assert(last_sep != NULL);

  char buffer[StringBufferSize];
  for (size_t buff_size = 0; buff_size <= sizeof(buffer); ++buff_size)
  {
    /* Initialize buffer to known contents */
    memset(buffer, CHAR_MAX, sizeof(buffer));

    const size_t n = diriterator_get_object_leaf_name(it, buffer, buff_size);
    check_buffer(buffer, buff_size, last_sep + 1, n);
  }

  diriterator_destroy(it);
}

static void test11(void)
{
  /* Get sub-path name */
  DirIterator *it;
  const _kernel_oserror *e = diriterator_make(
    &it, DirIterator_RecurseIntoDirectories, test_objects[0].name, NULL);
  assert(e == NULL);
  assert(it != NULL);

  /* Advance past the first object because we want a sub-path with at least
     two elements */
  e = diriterator_advance(it);
  assert(e == NULL);

  char buffer[StringBufferSize];
  for (size_t buff_size = 0; buff_size <= sizeof(buffer); ++buff_size)
  {
    /* Initialize buffer to known contents */
    memset(buffer, CHAR_MAX, sizeof(buffer));

    const size_t n = diriterator_get_object_sub_path_name(it, buffer, buff_size);

    assert(strlen(test_objects[2].name) > strlen(test_objects[0].name) + 1);

    check_buffer(buffer,
                 buff_size,
                 test_objects[2].name + strlen(test_objects[0].name) + 1,
                 n);
  }

  diriterator_destroy(it);
}

static void test12(void)
{
  /* Get full path name */
  DirIterator *it;
  const _kernel_oserror *e = diriterator_make(
    &it, DirIterator_RecurseIntoDirectories, test_objects[0].name, NULL);
  assert(e == NULL);
  assert(it != NULL);

  /* Advance past the first object because we want a sub-path with at least
     two elements */
  e = diriterator_advance(it);
  assert(e == NULL);

  char buffer[StringBufferSize];
  for (size_t buff_size = 0; buff_size <= sizeof(buffer); ++buff_size)
  {
    /* Initialize buffer to known contents */
    memset(buffer, CHAR_MAX, sizeof(buffer));

    const size_t n = diriterator_get_object_path_name(it, buffer, buff_size);
    check_buffer(buffer, buff_size, test_objects[2].name, n);
  }

  diriterator_destroy(it);
}

static void test13(void)
{
  /* Get object info */
  DirIterator *it;
  DirIteratorObjectInfo info[2];

  const _kernel_oserror *e = diriterator_make(
    &it, DirIterator_RecurseIntoDirectories, test_objects[0].name, NULL);
  assert(e == NULL);
  assert(it != NULL);

  /* Initialize buffer to known contents */
  memset(&info, CHAR_MAX, sizeof(info));

  const int object_type = diriterator_get_object_info(it, &info[0]);
  validate_object_info(&info[0], object_type, 1);

  /* Check that free bytes following the info are untouched */
  const char * const bytes = (const char *)&info[1];
  for (size_t n = 0; n < sizeof(info[1]); ++n)
    assert(bytes[n] == CHAR_MAX);

  diriterator_destroy(it);
}

static void test14(void)
{
  /* Get leaf name with null buffer */
  DirIterator *it;
  const _kernel_oserror *e = diriterator_make(
    &it, DirIterator_RecurseIntoDirectories, test_objects[0].name, NULL);
  assert(e == NULL);
  assert(it != NULL);

  /* Advance past the first object because we want a sub-path with at least
     two elements */
  e = diriterator_advance(it);
  assert(e == NULL);

  /* Find last path separator */
  const char * const last_sep = strrchr(test_objects[2].name, '.');
  assert(last_sep != NULL);

  const size_t n = diriterator_get_object_leaf_name(it, NULL, 0);
  assert(n == strlen(last_sep + 1));

  diriterator_destroy(it);
}

static void test15(void)
{
  /* Get sub-path name with null buffer */
  DirIterator *it;
  const _kernel_oserror *e = diriterator_make(
    &it, DirIterator_RecurseIntoDirectories, test_objects[0].name, NULL);
  assert(e == NULL);
  assert(it != NULL);

  /* Advance past the first object because we want a sub-path with at least
     two elements */
  e = diriterator_advance(it);
  assert(e == NULL);

  const size_t n = diriterator_get_object_sub_path_name(it, NULL, 0);
  assert(strlen(test_objects[2].name) > strlen(test_objects[0].name) + 1);
  assert(n == strlen(test_objects[2].name) - strlen(test_objects[0].name) - 1);

  diriterator_destroy(it);
}

static void test16(void)
{
  /* Get full path name with null buffer */
  DirIterator *it;
  const _kernel_oserror *e = diriterator_make(
    &it, DirIterator_RecurseIntoDirectories, test_objects[0].name, NULL);
  assert(e == NULL);
  assert(it != NULL);

  /* Advance past the first object because we want a sub-path with at least
     two elements */
  e = diriterator_advance(it);
  assert(e == NULL);

  const size_t n = diriterator_get_object_path_name(it, NULL, 0);
  assert(n == strlen(test_objects[2].name));

  diriterator_destroy(it);
}

static void test17(void)
{
  /* Get object info with null buffer */
  DirIterator *it;
  const _kernel_oserror * const e = diriterator_make(
    &it, 0, test_objects[0].name, NULL);
  assert(e == NULL);
  assert(it != NULL);

  const int object_type = diriterator_get_object_info(it, NULL);
  validate_object_info(NULL, object_type, 1);

  diriterator_destroy(it);
}

static void test18(void)
{
  /* Make fail recovery */
  DirIterator *it;
  unsigned long limit;

  for (limit = 0; limit < FortifyAllocationLimit; ++limit)
  {
    Fortify_SetNumAllocationsLimit(limit);
    const _kernel_oserror *e = diriterator_make(
      &it, 0, test_objects[0].name, "#*");
    Fortify_SetNumAllocationsLimit(ULONG_MAX);

    if (e == NULL)
      break;

    assert(it == NULL);
  }
  assert(limit != FortifyAllocationLimit);

  assert(it != NULL);
  assert(!diriterator_is_empty(it));

  char buffer[StringBufferSize];
  const size_t n = diriterator_get_object_path_name(it, buffer, sizeof(buffer));
  assert(n == strlen(test_objects[1].name));
  assert(strcmp(buffer, test_objects[1].name) == 0);

  diriterator_destroy(it);
}

static void test19(void)
{
  /* Advance fail recovery */
  DirIterator *it;
  size_t i;
  const _kernel_oserror *e = diriterator_make(
    &it, DirIterator_RecurseIntoDirectories, test_objects[0].name, NULL);
  assert(e == NULL);
  assert(it != NULL);

  for (i = 1; !diriterator_is_empty(it); ++i)
  {
    unsigned long limit;

    /* Validate the current object against the array from which the
       directory tree was generated */
    validate_object(it, i);

    for (limit = 0; limit < FortifyAllocationLimit; ++limit)
    {
      Fortify_SetNumAllocationsLimit(limit);
      e = diriterator_advance(it);
      Fortify_SetNumAllocationsLimit(ULONG_MAX);

      if (e == NULL)
        break; /* success - validate the next object */
    }
    assert(limit != FortifyAllocationLimit);
  }

  /* Check that the iterator didn't become empty too early */
  assert(i >= ARRAY_SIZE(test_objects));

  /* Check that the iterator stays in the correct empty state */
  check_empty(it);

  diriterator_destroy(it);
}

static void test20(void)
{
  /* Reset */
  DirIterator *it;
  const _kernel_oserror *e = diriterator_make(
    &it, DirIterator_RecurseIntoDirectories, test_objects[0].name, NULL);
  assert(e == NULL);
  assert(it != NULL);

  /* Gradually increase the number of advances between resets */
  for (size_t j = 1; !diriterator_is_empty(it); ++j)
  {
    size_t i;

    for (e = diriterator_reset(it), i = 1;
         !diriterator_is_empty(it) && i <= j;
         e = diriterator_advance(it), ++i)
    {
      assert(e == NULL);

      /* Validate the current object against the array from which the
         directory tree was generated */
      validate_object(it, i);
    }
    assert(e == NULL);

    /* Check that the iterator didn't become empty too early */
    if (diriterator_is_empty(it))
    {
      assert(i >= ARRAY_SIZE(test_objects));
    }
  }

  diriterator_destroy(it);
}

static void test21(void)
{
  /* Reset fail recovery */
  unsigned long limit;

  for (limit = 0; limit < FortifyAllocationLimit; ++limit)
  {
    size_t i;
    DirIterator *it;
    const _kernel_oserror *e = diriterator_make(
      &it, DirIterator_RecurseIntoDirectories, test_objects[0].name, NULL);
    assert(e == NULL);
    assert(it != NULL);

    /* Advance to an interesting state distinct from post-reset */
    for (i = 1; i <= ARRAY_SIZE(test_objects)/2; ++i)
    {
      e = diriterator_advance(it);
      assert(e == NULL);
    }

    Fortify_SetNumAllocationsLimit(limit);
    e = diriterator_reset(it);
    Fortify_SetNumAllocationsLimit(ULONG_MAX);

    if (e == NULL)
    {
      limit = FortifyAllocationLimit; /* success - test complete */
      i = 1;
    }

    /* Check that the state of the iterator is unchanged on error,
       or that the reset was successful if no error was returned. */
    for (; !diriterator_is_empty(it); ++i)
    {
      /* Validate the current object against the array from which the
         directory tree was generated */
      validate_object(it, i);

      e = diriterator_advance(it);
      assert(e == NULL);
    }

    /* Check that the iterator didn't become empty too early */
    assert(i >= ARRAY_SIZE(test_objects));

    diriterator_destroy(it);
  }
  assert(limit != FortifyAllocationLimit);
}

static void test22(void)
{
  /* Destroy null */
  diriterator_destroy(NULL);
}

void DirIter_tests(void)
{
  static const struct
  {
    const char *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Make/destroy", test1 },
    { "No recursion", test2 },
    { "Directory recursion", test3 },
    { "No recursion with pattern", test4 },
    { "Directory recursion with pattern", test5 },
    { "Make from empty directory without recursion", test6 },
    { "Make from empty directory with recursion", test7 },
    { "Make from missing directory without recursion", test8 },
    { "Make from missing directory with recursion", test9 },
    { "Get leaf name", test10 },
    { "Get sub-path name", test11 },
    { "Get full path name", test12 },
    { "Get object info", test13 },
    { "Get leaf name with null buffer", test14 },
    { "Get sub-path name with null buffer", test15 },
    { "Get full path name with null buffer", test16 },
    { "Get object info with null buffer", test17 },
    { "Make fail recovery", test18 },
    { "Advance fail recovery", test19 },
    { "Reset", test20 },
    { "Reset fail recovery", test21 },
    { "Destroy null", test22 }
  };

  init();

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

  final();
}
