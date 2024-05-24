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

/* History:
  CJB: 25-Mar-12: Created this source file.
  CJB: 23-Nov-14: Adapted to use type OS_GBPB_CatalogueInfo.
  CJB: 24-Nov-14: Added reset method.
  CJB: 26-Nov-14: Extra debugging info.
  CJB: 17-Dec-14: Updated to use the generic linked list implementation.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 21-Apr-16: Modified format strings to avoid GNU C compiler warnings.
  CJB: 07-Aug-18: Modified to use stringbuffer_append_separated and
                  PATH_SEPARATOR from "Platform.h" instead of two calls to
                  stringbuffer_append.
  CJB: 05-Feb-19: Use stringbuffer_append_all where appropriate.
  CJB: 28-Apr-19: Less verbose debugging output.
*/

/* ISO library headers */
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/* Acorn C/C++ library headers */
#include "kernel.h"

/* CBUtilLib headers */
#include "StringBuff.h"
#include "StrExtra.h"
#include "LinkedList.h"

/* CBOSLib headers */
#include "MessTrans.h"
#include "OSGBPB.h"

/* Local headers */
#include "Internal/CBMisc.h"
#include "Platform.h"
#include "DirIter.h"
#include "DateStamp.h"


/* Refilling the buffer early makes it more complex to find the total
   size of remaining entries and leaves less room for fresh ones. */
#undef KEEP_MORE_THAN_ONE_ENTRY

/* Constant numeric values */
enum
{
  GrowthFactor = 2, /* Multiplier for buffer size when overflow occurs */
  ErrorNum_BufferOverflow = 484, /* Error number used by _kernel_osgbpb */
  NameSize   = 11, /* Not a limit, but object names longer than 10
                      characters are still unusual on RISC OS. */
  MaxEntries = 16  /* Increase this to reduce the frequency of calling
                      _kernel_osgbpb. */
};

#define DEFAULT_BUFFER_SIZE ((offsetof(OS_GBPB_CatalogueInfo, name) + \
                            WORD_ALIGN(NameSize)) * MaxEntries)

/* Record for one level of the directory tree. Such records are members of
   a double-linked list ('parent' points to the next record and 'child'
   points to the previous one). */
typedef struct DirIteratorLevel
{
  LinkedListItem list_item; /* Linked to directory above and below this one. */
  size_t path_name_len; /* Length of this directory's path name, to allow
                           elements to be appended/removed efficiently. */
  int gbpb_next; /* Offset of the next next catalogue entry to be read
                    (0 at start of directory, -1 at end). */
  unsigned int nentries; /* Number of catalogue entries in the buffer that the
                   iterator hasn't yet advanced beyond. */
  const OS_GBPB_CatalogueInfo *entry; /* Pointer to catalogue entry for current object, or
                            NULL if none. */
  size_t entry_name_len; /* Length of the name of the current object. */
  size_t buffer_size; /* Size of the buffer for catalogue entries, in
                         bytes. */
  char buffer[]; /* Variable-length buffer for catalogue entries read by
                    _kernel_osgbpb. */
}
DirIteratorLevel;

/* State of a directory tree iterator. 'dir_list' is a linked list
   of data structures: one for each element appended to the 'path_name',
   plus one for the top-level directory. */
struct DirIterator
{
  unsigned int flags; /* Miscellaneous flags (e.g. whether to recurse into
                         directories and/or image files). */
  char *pattern; /* Pointer to wildcarded name to match, or NULL to match
                    all (equivalent to "*"). */
  StringBuffer path_name; /* Path name of current deepest directory. */
  size_t path_name_len; /* Length of the root path, for convenience of
                           diriterator_get_object_sub_path_name. */
  LinkedList dir_list;
};

static CONST _kernel_oserror *no_mem(void)
{
  return messagetrans_error_lookup(NULL, DUMMY_ERRNO, "NoMem", 0);
}

static bool free_level_callback(LinkedList *list, LinkedListItem *item, void *arg)
{
  const LinkedListItem * const stop = arg;

  /* If stop is NULL then no item can match it and all will be freed. */
  assert(item != NULL);
  if (item != stop)
  {
    assert(item != NULL);
    DEBUG_VERBOSEF("DirIterator: freeing level %p\n", (void *)item);
    linkedlist_remove(list, item);
    free(item);
  }

  return item == stop;
}

static void free_levels(LinkedList *dir_list, LinkedListItem *stop)
{
  linkedlist_for_each(dir_list, free_level_callback, stop);
}

static CONST _kernel_oserror *extend_buffer(DirIterator       *iterator,
                                            DirIteratorLevel **levelp)
{
  size_t new_size, entry_offset;
  DirIteratorLevel *level, *new_level;
  LinkedListItem *prev;
  CONST _kernel_oserror *e = NULL;

  assert(levelp != NULL);
  level = *levelp;
  assert(level != NULL);

  /* Calculate a byte offset from the base of the buffer to the current
     entry. This will be used to relocate the current entry pointer if the
     buffer moves. */
  if (level->entry != NULL)
    entry_offset = (const char *)level->entry - level->buffer;
  else
    entry_offset = SIZE_MAX;

  /* Try to allocate a larger buffer */
  new_size = level->buffer_size * GrowthFactor;
  DEBUG_VERBOSEF("DirIterator: trying to expand buffer from %zu to %zu bytes\n",
    level->buffer_size, new_size);

  /* If reallocation succeeds then the list item will move in memory, so
     we must unlink it from the list first. */
  prev = linkedlist_get_prev(&level->list_item);
  linkedlist_remove(&iterator->dir_list, &level->list_item);

  new_level = realloc(level, offsetof(DirIteratorLevel, buffer) + new_size);
  if (new_level == NULL)
  {
    DEBUGF("DirIterator: realloc failed!\n");
    e = no_mem();
  }
  else
  {
    /* *levelp is no longer a valid pointer, but I assume it is safe to
       print and use in comparisons */
    DEBUG_VERBOSEF("DirIterator: realloc successful, level %p is now at %p\n",
      (void *)level, (void *)new_level);
    level = new_level;
    level->buffer_size = new_size;

    /* Relocate the pointer to the current entry (if any) */
    if (entry_offset != SIZE_MAX)
      level->entry = (const OS_GBPB_CatalogueInfo *)(level->buffer + entry_offset);

    *levelp = level;
  }

  /* Reinsert the list item at the correct place whether it was
     successfully reallocated or not. */
  linkedlist_insert(&iterator->dir_list, prev, &level->list_item);

  return e;
}

static CONST _kernel_oserror *refill_buffer(DirIterator       *iterator,
                                            DirIteratorLevel **levelp)
{
  CONST _kernel_oserror *e = NULL;
  size_t keep_size = 0;
  DirIteratorLevel *level;
  const char *path_name;
  bool retry;

  assert(iterator != NULL);
  path_name = stringbuffer_get_pointer(&iterator->path_name);
  assert(path_name != NULL);

  assert(levelp != NULL);
  level = *levelp;
  assert(level != NULL);

  /* I don't trust _kernel_osgbpb to leave the buffer untouched on error, so
     move any remaining catalogue entries (including the current one) to the
     start of the buffer for safekeeping. */
  if (level->nentries > 0)
  {
    size_t entry_name_size = level->entry_name_len + 1;

#ifdef KEEP_MORE_THAN_ONE_ENTRY
    const OS_GBPB_CatalogueInfo *entry = level->entry;
    int nentries = level->nentries;
    for (--nentries; nentries != 0; --nentries)
    {
      /* Calculate the length of the next entry's name */
      assert(entry != NULL);
      entry = (const OS_GBPB_CatalogueInfo *)(entry->name + WORD_ALIGN(entry_name_size));
      entry_name_size = strlen(entry->name) + 1;
    }
    keep_size += (const char *)entry - (const char *)level->entry;
#else
    assert(level->nentries == 1);
#endif /* KEEP_MORE_THAN_ONE_ENTRY */

    keep_size += offsetof(OS_GBPB_CatalogueInfo, name) + entry_name_size;

    DEBUG_VERBOSEF("DirIterator: Moving %d entries (%zu bytes) within buffer %p\n",
      level->nentries, keep_size, level->buffer);

    memmove(level->buffer, level->entry, keep_size);
    keep_size = WORD_ALIGN(keep_size); /* align before fresh entries */
  }
  level->entry = (const OS_GBPB_CatalogueInfo *)level->buffer;

  do
  {
    unsigned int n;
    retry = false;

    /* The offset to the next item to read is updated by each call to
       _kernel_osgbpb. */
    assert(level->gbpb_next != OS_GBPB_ReadCat_PositionEnd);

    /* Read catalogue entries until an error occurs,
       at least one entry has been read,
       or the end of the directory is reached. */
    do
    {
      n = MaxEntries;
      e = os_gbpb_read_cat_no_path(path_name,
                                   level->buffer + keep_size,
                                   level->buffer_size - keep_size,
                                   &n,
                                   &level->gbpb_next,
                                   iterator->pattern);
    }
    while (e == NULL && n == 0 && level->gbpb_next != OS_GBPB_ReadCat_PositionEnd);

    if (e == NULL)
    {
      /* If there was previously no 'current' entry then find the length of
         the new 'current' entry */
      if (level->nentries == 0)
      {
        assert(level->entry != NULL);
        level->entry_name_len = strlen(level->entry->name);
      }
      level->nentries += n;
    }
    else
    {
      if (e->errnum == ErrorNum_BufferOverflow)
      {
        e = extend_buffer(iterator, levelp);
        level = *levelp;

        /* If the buffer was successfully extended then try again
           with the same offset to read from as last time. */
        if (e == NULL)
          retry = true;
      }
    }
  }
  while (retry);

  return e;
}

static CONST _kernel_oserror *enter_dir(DirIterator *iterator)
{
  CONST _kernel_oserror *e = NULL;
  DirIteratorLevel *level;

  assert(iterator != NULL);

  DEBUGF("DirIterator: Entering '%s'\n",
         stringbuffer_get_pointer(&iterator->path_name));

  level = malloc(offsetof(DirIteratorLevel, buffer) + DEFAULT_BUFFER_SIZE);
  if (level != NULL)
  {
    /* Record the length of the path name leading up to this directory. */
    level->path_name_len = stringbuffer_get_length(&iterator->path_name);
    level->entry = NULL;
    level->nentries = 0;
    level->gbpb_next = 0; /* start of directory */
    level->buffer_size = DEFAULT_BUFFER_SIZE;
    linkedlist_insert(&iterator->dir_list, NULL, &level->list_item);

    /* Try to fill the buffer with catalogue entries for this level. */
    {
      DirIteratorLevel *tmp = level; /* Avoid taking the address of 'level'*/
      e = refill_buffer(iterator, &tmp);
      assert(tmp != NULL);
      level = tmp;
    }

    /* Don't bother entering empty directories. */
    if (e == NULL && level->nentries > 0)
    {
      DEBUG_VERBOSEF("DirIterator: Deepest directory is now %p (path length %zu)\n",
        (void *)level, level->path_name_len);
    }
    else
    {
      linkedlist_remove(&iterator->dir_list, &level->list_item);
      free(level);
      DEBUGF("DirIterator: Ignoring empty directory '%s'\n",
             stringbuffer_get_pointer(&iterator->path_name));
    }
  }
  else
  {
    e = no_mem();
  }
  return e;
}

static bool can_enter_dir(const DirIterator *iterator, const OS_GBPB_CatalogueInfo *entry)
{
  unsigned int mask;

  assert(iterator != NULL);
  assert(entry != NULL);

  /* If the current object is a directory or image file and the
     appropriate recursion flag is set then enter it. */
  switch (entry->info.object_type)
  {
    case ObjectType_Image:
      mask = DirIterator_RecurseIntoImages;
      break;
    case ObjectType_Directory:
      mask = DirIterator_RecurseIntoDirectories;
      break;
    default:
      mask = 0;
      break;
  }

  return mask & iterator->flags;
}

static void advance(DirIteratorLevel *level)
{
  const OS_GBPB_CatalogueInfo *entry;

  DEBUG_VERBOSEF("DirIterator: Advancing to next object on level %p\n",
    (void *)level);
  assert(level != NULL);
  assert(level->nentries > 0);

  if (--level->nentries > 0)
  {
    /* Calculate the address of the next catalogue entry in the buffer */
    const size_t name_size = WORD_ALIGN(level->entry_name_len + 1);

    entry = level->entry;
    assert(entry != NULL);
    entry = (const OS_GBPB_CatalogueInfo *)(entry->name + name_size);

    /* Cache the length of the object name, for future use */
    level->entry_name_len = strlen(entry->name);
  }
  else
  {
    /* No more entries on the current level */
    entry = NULL;
  }

  DEBUGF("DirIterator: Next entry is %p ('%s'), %d entries remain\n",
         (void *)entry, entry == NULL ? "" : entry->name, level->nentries);

  level->entry = entry;
}

static CONST _kernel_oserror *leave_dir(DirIterator *iterator)
{
  DirIteratorLevel *ancestor, *deepest_dir;
  CONST _kernel_oserror *e = NULL;

  assert(iterator != NULL);
  deepest_dir = (DirIteratorLevel *)linkedlist_get_head(&iterator->dir_list);

  DEBUGF("DirIterator: Leaving level %p ('%s')\n", (void *)deepest_dir,
         stringbuffer_get_pointer(&iterator->path_name));

  assert(deepest_dir != NULL);
  ancestor = (DirIteratorLevel *)linkedlist_get_next(&deepest_dir->list_item);

  while (e == NULL && ancestor != NULL && ancestor->nentries == 0)
  {
    if (ancestor->gbpb_next == OS_GBPB_ReadCat_PositionEnd)
    {
      /* End of ancestor directory: go up another level */
      ancestor = (DirIteratorLevel *)linkedlist_get_next(&ancestor->list_item);
    }
    else
    {
      /* Remove the leaf name of the current directory from the path */
      stringbuffer_truncate(&iterator->path_name, ancestor->path_name_len);

      /* Try to refill the buffer with catalogue entries for the ancestor
         directory */
      {
        DirIteratorLevel *tmp = ancestor; /* Avoid taking the address of
                                             'ancestor' */
        e = refill_buffer(iterator, &tmp);
        assert(tmp != NULL);
        ancestor = tmp;
      }

      /* Reinstate the leaf name of the current directory */
      stringbuffer_undo(&iterator->path_name);
    }
  }

  if (e == NULL)
  {
    /* Free the directory level structs lower than the lowest ancestor on
       which we managed to find catalogue entries.
       This works even if ancestor == NULL (in which case the
       iterator is empty and all directory level structs are freed). */
    free_levels(&iterator->dir_list, &ancestor->list_item);

    /* Remove the leaf names of the lower directories from the path */
    if (ancestor != NULL)
      stringbuffer_truncate(&iterator->path_name, ancestor->path_name_len);

    DEBUG_VERBOSEF("DirIterator: Deepest directory is now %p\n",
      (void *)ancestor);
  }

  return e;
}

static size_t get_name(const DirIterator *iterator,
                       char              *buffer,
                       size_t             buff_size,
                       size_t             skip_size)
{
  DirIteratorLevel *level;
  size_t nchars;

  DEBUG_VERBOSEF("DirIterator: skip %zu characters of path\n",
    skip_size);

  assert(iterator != NULL);
  level = (DirIteratorLevel *)linkedlist_get_head(&iterator->dir_list);
  if (level == NULL)
  {
    /* Output an empty string because the iterator is empty */
    DEBUGF("DirIterator: Iterator %p is empty\n", (void *)iterator);
    if (buff_size > 0)
    {
      assert(buffer != NULL);
      *buffer = '\0';
    }
    nchars = 0;
  }
  else
  {
    DEBUG_VERBOSEF("DirIterator: Deepest directory is %p (path name length %zu)\n",
      (void *)level, level->path_name_len);
    assert(level->nentries > 0);
    assert(level->entry != NULL);
    assert(strlen(level->entry->name) == level->entry_name_len);
    assert(level->path_name_len ==
           stringbuffer_get_length(&iterator->path_name));

    if (buff_size > 0)
    {
      /* -1 is to leave room for the null terminator */
      size_t nbytes, bytes_free = buff_size - 1;
      char *write = buffer;

      assert(buffer != NULL);

      /* Allow for the fact that we may skip the whole path, in which
         case we are only getting the leaf name. */
      if (skip_size < level->path_name_len)
      {
        /* Concatenate as much of the directory path name as will fit in the
           remaining space. */
        nbytes = LOWEST(level->path_name_len - skip_size, bytes_free);
        memcpy(write,
               stringbuffer_get_pointer(&iterator->path_name) + skip_size,
               nbytes);
        write += nbytes;
        bytes_free -= nbytes;

        /* Concatenate a path separator if it fits in the remaining space. */
        if (bytes_free > 0)
        {
          *(write++) = PATH_SEPARATOR;
          --bytes_free;
        }
      }

      /* Concatenate as much of the leaf name as will fit in the remaining
         space */
      nbytes = LOWEST(level->entry_name_len, bytes_free);
      memcpy(write, level->entry->name, nbytes);
      write += nbytes;
      bytes_free -= nbytes;

      /* Append a nul terminator */
      assert(write < buffer + buff_size);
      *write = '\0';

      DEBUG_VERBOSEF("DirIterator: (truncated) name is '%s'\n", buffer);
    }

    nchars = level->entry_name_len;
    DEBUG_VERBOSEF("DirIterator: leaf name length is %zu\n", nchars);

    /* Allow for the fact that we may skip the whole path, in which
       case we are only getting the leaf name. */
    if (skip_size < level->path_name_len)
      nchars += level->path_name_len - skip_size + 1;

  }

  DEBUG_VERBOSEF("DirIterator: total string length is %zu\n", nchars);
  return nchars;
}

CONST _kernel_oserror *diriterator_make(DirIterator  **iterator,
                                        unsigned int   flags,
                                        const char    *path_name,
                                        const char    *pattern)
{
  CONST _kernel_oserror *e = NULL;
  DirIterator *it = NULL;

  assert(iterator != NULL);
  assert(path_name != NULL);
  DEBUGF("DirIterator: Making iterator for path '%s' with flags 0x%x and pattern '%s'\n",
         path_name, flags, pattern ? pattern : "");

  it = malloc(sizeof(*it));
  if (it != NULL)
  {
    if (pattern == NULL || strcmp(pattern, "*") == 0)
    {
      /* Match any object name. */
      it->pattern = NULL;
    }
    else
    {
      /* Duplicate the wildcarded object name to be matched. */
      it->pattern = strdup(pattern);
      if (it->pattern == NULL)
        e = no_mem();
    }

    if (e == NULL)
    {
      stringbuffer_init(&it->path_name);
      if (stringbuffer_append_all(&it->path_name, path_name))
      {
        it->flags = flags;
        linkedlist_init(&it->dir_list);
        it->path_name_len = stringbuffer_get_length(&it->path_name);

        e = enter_dir(it);
        if (e != NULL)
          stringbuffer_destroy(&it->path_name);
      }
      else
      {
        e = no_mem();
      }

      if (e != NULL)
        free(it->pattern); /* may be null */
    }

    if (e != NULL)
      free(it);
  }
  else
  {
    e = no_mem();
  }

  /* Store a pointer to the new iterator's state, or a null pointer if
     we are about to return an error. */
  *iterator = (e == NULL ? it : NULL);

  return e;
}

CONST _kernel_oserror *diriterator_reset(DirIterator *iterator)
{
  CONST _kernel_oserror *e = NULL;
  LinkedList old_dir_list;

  DEBUGF("DirIterator: Resetting iterator %p\n", (void *)iterator);
  assert(iterator != NULL);

  /* Try to recreate the top level data structure again. */
  old_dir_list = iterator->dir_list;
  linkedlist_init(&iterator->dir_list);
  stringbuffer_truncate(&iterator->path_name, iterator->path_name_len);

  e = enter_dir(iterator);
  if (e == NULL)
  {
    /* Destroy the old data structures on success. */
    free_levels(&old_dir_list, NULL);
  }
  else
  {
    /* Restore the previous state on error */
    iterator->dir_list = old_dir_list;
    stringbuffer_undo(&iterator->path_name);
  }

  return e;
}

bool diriterator_is_empty(const DirIterator *iterator)
{
  bool is_empty;

  assert(iterator != NULL);
  is_empty = (linkedlist_get_head(&iterator->dir_list) == NULL);
  DEBUGF("DirIterator: Iterator %p is%s empty\n",
         (void *)iterator, is_empty ? "" : " not");

  return is_empty;
}

int diriterator_get_object_info(const DirIterator     *iterator,
                                DirIteratorObjectInfo *info)
{
  DirIteratorLevel *level;
  int object_type;

  DEBUGF("DirIterator: Getting object info from iterator %p to buffer %p\n",
         (void *)iterator, (void *)info);

  assert(iterator != NULL);
  level = (DirIteratorLevel *)linkedlist_get_head(&iterator->dir_list);
  if (level == NULL)
  {
    /* No current object because the iterator is empty */
    object_type = ObjectType_NotFound;
  }
  else
  {
    const OS_GBPB_CatalogueInfo *entry;

    assert(level->nentries > 0);
    entry = level->entry;
    assert(entry != NULL);

    if (info != NULL)
    {
      info->file_type = decode_load_exec(entry->info.load,
                                         entry->info.exec,
                                         &info->date_stamp);

      if (entry->info.object_type == ObjectType_Directory ||
          entry->info.object_type == ObjectType_Image)
      {
        info->file_type = (entry->name[0] == '!' ?
                          FileType_Application : FileType_Directory);
      }

      info->length = entry->info.length;
      info->attributes = entry->info.attributes;
    }

    object_type = entry->info.object_type;
  }

  DEBUG_VERBOSEF("DirIterator: object type is %d\n", object_type);
  return object_type;
}

size_t diriterator_get_object_path_name(const DirIterator *iterator,
                                        char              *buffer,
                                        size_t             buff_size)
{
  DEBUGF("DirIterator: Getting path name from iterator %p into "
         "buffer %p of size %zu\n", (void *)iterator, buffer,
         buff_size);

  /* Include all characters of the path name of the current level. */
  return get_name(iterator, buffer, buff_size, 0);
}

size_t diriterator_get_object_sub_path_name(const DirIterator *iterator,
                                            char              *buffer,
                                            size_t             buff_size)
{
  DEBUGF("DirIterator: Getting sub-path name from iterator %p into "
         "buffer %p of size %zu\n", (void *)iterator, buffer,
         buff_size);

  /* Skip those characters of the path name of the current level which also
     belong to the path of the top-level directory. */
  assert(iterator != NULL);
  return get_name(iterator,
                  buffer,
                  buff_size,
                  iterator->path_name_len + 1);
}

size_t diriterator_get_object_leaf_name(const DirIterator *iterator,
                                        char              *buffer,
                                        size_t             buff_size)
{
  DEBUGF("DirIterator: Getting leaf name from iterator %p into "
         "buffer %p of size %zu\n", (void *)iterator, buffer,
         buff_size);

  /* Skip all characters of the path name of the current level. */
  return get_name(iterator, buffer, buff_size, SIZE_MAX);
}

CONST _kernel_oserror *diriterator_advance(DirIterator *iterator)
{
  CONST _kernel_oserror *e = NULL;
  DirIteratorLevel *level;
  DEBUG_VERBOSEF("DirIterator: Advancing iterator %p\n", (void *)iterator);

  assert(iterator != NULL);
  level = (DirIteratorLevel *)linkedlist_get_head(&iterator->dir_list);
  if (level == NULL)
  {
    DEBUGF("DirIterator: Empty\n");
  }
  else
  {
    bool entered = false;

    assert(level->nentries > 0);
    if (can_enter_dir(iterator, level->entry))
    {
      if (stringbuffer_append_separated(&iterator->path_name,
                                        PATH_SEPARATOR, level->entry->name))
      {
        e = enter_dir(iterator);
        if (e == NULL &&
            level != (DirIteratorLevel *)linkedlist_get_head(&iterator->dir_list))
        {
          /* Don't want to return to the same entry on this level. */
          advance(level);
          entered = true;
        }
        else
        {
          stringbuffer_undo(&iterator->path_name);
        }
      }
      else
      {
        e = no_mem();
      }
    }

    /* If we entered a sub-directory then there is nothing more to do. */
    if (!entered && e == NULL)
    {
      /* Have we reached the last catalogue entry on the current level?
         Threshold can be increased if we can KEEP_MORE_THAN_ONE_ENTRY. */
      if (level->nentries < 2)
      {
        /* Are we at the end of the directory? */
        if (level->gbpb_next != OS_GBPB_ReadCat_PositionEnd)
        {
          /* Try to refill the buffer on the current level. */
          DirIteratorLevel *tmp = level; /* Avoid taking the address of
                                            'level' */
          e = refill_buffer(iterator, &tmp);
          assert(tmp != NULL);
          level = tmp;
        }

        if (e == NULL)
        {
          /* Did we manage to read any more catalogue entries? */
          assert(level != NULL);
          if (level->nentries < 2)
          {
            /* Go up a level until reaching the top or finding a directory in
               which we haven't already advanced past all of the entries. */
            e = leave_dir(iterator);
          }
          else
          {
            /* Advance to the next catalogue entry on the current level. */
            advance(level);
          }
        }
      }
      else
      {
        /* Advance to the next catalogue entry on the current level. */
        advance(level);
      }
    }
  }

  return e;
}

void diriterator_destroy(DirIterator *iterator)
{
  DEBUGF("DirIterator: Destroying iterator %p\n", (void *)iterator);
  if (iterator != NULL)
  {
    /* Free each member of the linked list in turn, starting at the head. */
    free_levels(&iterator->dir_list, NULL);

    stringbuffer_destroy(&iterator->path_name);
    free(iterator->pattern); /* may be null */
    free(iterator);
  }
}
