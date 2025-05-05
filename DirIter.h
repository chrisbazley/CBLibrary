/*
 * CBLibrary: Directory tree iterator
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

/* DirIter.h declares functions and types for iterator objects to allow
   traversal of a directory tree in depth-first order.

  Example usage (note that nothing is printed for an empty directory):

  _Optional const _kernel_oserror *e;
  _Optional DirIterator *it;
  for (e = diriterator_make(&it, 0, "ADFS::0.$", NULL);
       e == NULL && !diriterator_is_empty(it);
       e = diriterator_advance(it))
  {
     char buffer[256];
     const int n = diriterator_get_object_leaf_name(
                      it, buffer, sizeof(buffer));
     puts(buffer);
     if (n >= sizeof(buffer))
       fprintf(stderr, "Name truncated!\n");
  }
  diriterator_destroy(it);

Dependencies: ANSI C library, Acorn library kernel.
Message tokens: NoMem.
History:
  CJB: 25-Mar-12: Created this header file.
  CJB: 24-Nov-14: Added reset method.
  CJB: 01-Jun-16: Documented that diriterator_destroy(NULL) has no effect.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
 */

#ifndef DirIter_h
#define DirIter_h

/* ISO library headers */
#include <stddef.h> /* (for size_t) */
#include <stdbool.h>

/* Acorn C/C++ library headers */
#include "kernel.h"

/* Local headers */
#include "Macros.h"
#include "DateStamp.h"

#if !defined(USE_OPTIONAL) && !defined(_Optional)
#define _Optional
#endif

typedef struct
{
  OSDateAndTime date_stamp;
  long int      length;
  unsigned int  attributes;
  int           file_type;
}
DirIteratorObjectInfo;
   /*
    * Catalogue information about a filing system object.
    */

typedef struct DirIterator DirIterator;
   /*
    * Incomplete struct type representing a directory tree iterator.
    */

/* Flags for use with the diriterator_make function */
#define DirIterator_RecurseIntoDirectories       (1u << 0)
#define DirIterator_RecurseIntoImages            (1u << 1)

_Optional CONST _kernel_oserror *diriterator_make(_Optional DirIterator ** /*iterator*/,
                                                  unsigned int             /*flags*/,
                                                  const char             * /*path_name*/,
                                                  _Optional const char   * /*pattern*/);
   /*
    * Creates an iterator object to allow traversal of the given directory
    * 'path_name' (e.g. "ADFS::0.$" would enumerate objects in the root
    * directory of a floppy disc). Recursion into sub-directories and
    * image files is controlled by the specified flags. Only objects with
    * names that match the wildcarded string 'pattern' will be included.
    * If 'pattern' is a null pointer or "*" then all names will match.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

_Optional CONST _kernel_oserror *diriterator_reset(DirIterator * /*iterator*/);
   /*
    * Reset a directory iterator to its initial state.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

bool diriterator_is_empty(const DirIterator *iterator);
   /*
    * Finds out whether a specified iterator is empty (i.e. there is no
    * current object to be queried and cannot advance to the next object).
    * Returns: true if the iterator is empty, otherwise false.
    */

int diriterator_get_object_info(const DirIterator     * /*iterator*/,
                                DirIteratorObjectInfo * /*info*/);
   /*
    * Gets catalogue information about the current object from a specified
    * directory tree iterator. Unless 'info' is a null pointer, the object's
    * file type, date stamp, length and attributes will be stored in the
    * object pointed to by 'info'. Untyped files are reported as
    * FileType_None (0x3000) not FileType_Null (-1). Directories and image
    * files with a leaf name prefixed by "!" are reported as
    * FileType_Application (0x2000).
    * Returns: type of the current object (ObjectType_NotFound if the
    *          iterator is empty).
    */

size_t diriterator_get_object_path_name(const DirIterator * /*iterator*/,
                                        char              * /*buffer*/,
                                        size_t              /*buff_size*/);
   /*
    * Gets the full path name of the current object from a specified
    * directory tree iterator. If the iterator is empty then an empty
    * string will be output. Copies as many characters as will fit into
    * the 'buffer' array. If 'buffer_size' is zero, nothing is written and
    * 'buffer' may be a null pointer. Otherwise, characters beyond the
    * 'buffer_size'-1st are discarded and a null character is written
    * at the end of the characters actually written into the array.
    * Returns: the number of characters that would have been written had
    *          'buff_size' been large enough, not counting the terminating
    *          null character (0 if the iterator is empty).
    */

size_t diriterator_get_object_sub_path_name(
                                        const DirIterator * /*iterator*/,
                                        char              * /*buffer*/,
                                        size_t              /*buff_size*/);
   /*
    * Gets the sub-path name of the current object from a specified
    * directory tree iterator, i.e. components of the object's path name
    * beyond the stem specified upon creation of the iterator. (If the stem
    * was "ADFS::0.$.foo" and the current object's full path name is
    * "ADFS::0.$.foo.bar.baz" then "bar.baz" is the sub-path name.) This
    * function behaves like diriterator_get_object_path_name in every other
    * respect.
    */

size_t diriterator_get_object_leaf_name(const DirIterator * /*iterator*/,
                                        char              * /*buffer*/,
                                        size_t              /*buff_size*/);
   /*
    * Gets the leaf name of the current object from a specified directory
    * tree iterator, i.e. the final component of the object's path name.
    * (If the current object's full path name is "ADFS::0.$.foo.bar.baz"
    * then "baz" is the leaf name.) This function behaves like
    * diriterator_get_object_path_name in every other respect.
    */

_Optional CONST _kernel_oserror *diriterator_advance(DirIterator * /*iterator*/);
   /*
    * Advances the given iterator to the next object in the directory tree
    * that matches the wildcarded name pattern, or makes the iterator
    * empty if there are no more matching objects. If the iterator was
    * already empty then this function has no effect and no error is
    * returned. If an error is returned then the current object is
    * unchanged and subsequent attempts to advance may succeed (e.g. if
    * more free memory becomes available).
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

void diriterator_destroy(_Optional DirIterator * /*iterator*/);
   /*
    * Frees memory that was previously allocated for a directory iterator.
    * Does nothing if called with a null pointer.
    */

#endif
