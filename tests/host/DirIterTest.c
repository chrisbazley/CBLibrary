/* CBLibrary host tests: synthetic directory entries and reset reads.
 * Copyright (C) 2026 Christopher Bazley
 */
#undef NDEBUG
#include <limits.h>
#include "../../DirIter.c"

_kernel_oserror *messagetrans_error_lookup(MessagesFD *mfd, int errnum,
  const char *token, size_t nparam, ...)
{
  static _kernel_oserror error;
  (void)mfd; (void)token; (void)nparam;
  error.errnum = errnum;
  return &error;
}

/* Synthetic catalogue reads exercise path truncation without RISC OS SWIs. */
static bool fail_read;

_kernel_oserror *os_gbpb_read_cat_no_path(const char *path, void *buffer,
  size_t size, size_t *n, int *position, const char *pattern)
{
  static _kernel_oserror error = {.errnum = 1};
  (void)pattern;
  assert(!strcmp(path, "ADFS::0.$.root"));
  if (fail_read)
  {
    return &error;
  }
  assert(size >= sizeof(OS_GBPB_CatalogueInfo) + 4);
  OS_GBPB_CatalogueInfo *const entry = buffer;
  memset(entry, 0, sizeof(*entry) + 4);
  entry->info.object_type = ObjectType_File;
  strcpy(entry->name, "new");
  *n = 1;
  *position = OS_GBPB_ReadCat_PositionEnd;
  return NULL;
}

int main(void)
{
#ifdef FORTIFY
  /* Fail iterator, pattern and path allocations in turn. */
  for (unsigned long limit = 0; limit < 3; ++limit)
  {
    _Optional DirIterator *iterator = NULL;
    Fortify_SetNumAllocationsLimit(limit);
    _Optional CONST _kernel_oserror *const error =
      diriterator_make(&iterator, 0, "ADFS::0.$.root", "file*");
    Fortify_SetNumAllocationsLimit(ULONG_MAX);
    assert(error);
    assert(!iterator);
  }
#endif
  static const char path[] = "ADFS::0.$.root.child", leaf[] = "file";
  OS_GBPB_CatalogueInfo *entry = calloc(1, sizeof(*entry) + sizeof leaf);
  DirIteratorLevel *level = calloc(1, sizeof(*level));
  assert(entry && level);
  memcpy(entry->name, leaf, sizeof leaf);
  level->entry = entry;
  level->entry_name_len = strlen(leaf);
  level->path_name_len = strlen(path);
  level->nentries = 1;
  DirIterator it = {.path_name_len = strlen("ADFS::0.$.root"),
    .dir_list = {.head = &level->list_item, .tail = &level->list_item}};
  strb_t *const name = &it.path_name;
  assert(!strb_cpy(name, path));
  static const char expected[] = "ADFS::0.$.root.child.file";
  for (size_t n = 0; n < sizeof expected + 3; ++n)
  {
    char out[sizeof expected + 5];
    memset(out, 'X', sizeof out);
    size_t const length = diriterator_get_object_path_name(&it, out + 1, n);
    assert(length == strlen(expected));
    assert(out[0] == 'X');
    if (n)
    {
      size_t const count = LOWEST(n - 1, strlen(expected));
      assert(!memcmp(out + 1, expected, count));
      assert(out[count + 1] == 0);
      assert(out[n + 1] == 'X');
    }
    else
    {
      assert(out[1] == 'X');
    }
  }
  STRB_DECL_FIXED(sb, 63);
  assert(!strb_puts(sb, "prefix:tail"));
  strb_seek(sb, 7);
  assert(!diriterator_get_object_path_name_sb(&it, sb));
  assert(!strcmp(strb_cptr(sb), "prefix:ADFS::0.$.root.child.filetail"));
  strb_empty(sb);
  assert(!diriterator_get_object_sub_path_name_sb(&it, sb));
  assert(!strcmp(strb_cptr(sb), "child.file"));
  strb_setmode(sb, strb_overwrite);
  strb_seek(sb, 0);
  assert(!diriterator_get_object_leaf_name_sb(&it, sb));
  assert(!strcmp(strb_cptr(sb), "filed.file"));
  STRB_DECL_FIXED(small, 3);
  assert(!strb_puts(small, "old"));
  strb_seek(small, 1);
  assert(diriterator_get_object_path_name_sb(&it, small));
  assert(!strcmp(strb_cptr(small), "old"));
  assert(strb_tell(small) == 1);
  assert(strb_error(small));
  /* Failed reset must restore both the old path and the old directory list. */
  size_t const old_pos = strb_tell(name);
  fail_read = true;
  assert(diriterator_reset(&it));
  assert(it.dir_list.head == &level->list_item);
  assert(strb_tell(name) == old_pos);
  assert(!strcmp(strb_cptr(name), path));

  /* Successful reset discards the old level and starts at the root. */
  fail_read = false;
  assert(!diriterator_reset(&it));
  assert(strb_tell(name) == it.path_name_len);
  assert(strb_len(name) == it.path_name_len);
  assert(!strcmp(strb_cptr(name), "ADFS::0.$.root"));
  char reset_name[64];
  diriterator_get_object_path_name(&it, reset_name, sizeof reset_name);
  assert(!strcmp(reset_name, "ADFS::0.$.root.new"));
  free_levels(&it.dir_list, NULL);
  free(entry);
  it.dir_list = (LinkedList){0};
  assert(!diriterator_get_object_path_name_sb(&it, sb));
  assert(diriterator_get_object_path_name(&it, NULL, 0) == 0);
  /* Existing getters must not inherit the new library's capacity limit. */
  size_t const long_len = (size_t)STRB_MAX_SIZE + 17;
  OS_GBPB_CatalogueInfo *long_entry = calloc(1, sizeof(*entry) + long_len + 1);
  char *long_out = malloc(long_len + 2);
  assert(long_entry && long_out);
  memset(long_entry->name, 'L', long_len);
  level = calloc(1, sizeof(*level));
  assert(level);
  level->nentries = 1;
  level->path_name_len = strb_tell(name);
  level->entry = long_entry;
  level->entry_name_len = long_len;
  it.dir_list = (LinkedList){.head = &level->list_item, .tail = &level->list_item};
  long_out[long_len + 1] = 'X';
  assert(diriterator_get_object_leaf_name(&it, long_out, long_len + 1) == long_len);
  assert(!memcmp(long_out, long_entry->name, long_len + 1));
  assert(long_out[long_len + 1] == 'X');
  free(long_entry);
  free(long_out);
  free(level);
  puts("DirIter output tests passed");
  strb_destroy(name);
}
