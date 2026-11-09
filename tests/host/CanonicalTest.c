/* CBLibrary host tests: canonical paths with synthetic filing-system output.
 * Copyright (C) 2026 Christopher Bazley
 */
#undef NDEBUG
#include <assert.h>
#include <limits.h>
#include <string.h>
#include "FileUtils.h"
#include "MessTrans.h"
#include "OSFSCntrl.h"
#ifdef FORTIFY
#include "fortify.h"
#endif

static bool fail_measure, fail_write;
static _kernel_oserror error;
static const char canonical[] = "ADFS::0.$.file";

_kernel_oserror *messagetrans_error_lookup(MessagesFD *mfd, int errnum,
  const char *token, size_t nparam, ...)
{
  (void)mfd; (void)token; (void)nparam;
  error.errnum = errnum;
  return &error;
}

_kernel_oserror *os_fscontrol_canonicalise(char *buffer, size_t size,
  const char *pv, const char *ps, const char *f, size_t *nbytes)
{
  assert(!strcmp(f, "file"));
  assert(!strcmp(pv, "Test$Path"));
  assert(!strcmp(ps, "ADFS::0.$."));
  if (buffer ? fail_write : fail_measure)
  {
    return &error;
  }
  if (buffer)
  {
    assert(size == sizeof canonical);
    memcpy(buffer, canonical, sizeof canonical);
  }
  if (nbytes)
  {
    *nbytes = sizeof canonical;
  }
  return NULL;
}

int main(void)
{
  STRB_DECL_FIXED(output, 63);
  assert(!strb_puts(output, "prefix:tail"));
  strb_seek(output, 7);
  assert(!canonicalise_sb(output, "Test$Path", "ADFS::0.$.", "file"));
  assert(!strcmp(strb_cptr(output), "prefix:ADFS::0.$.filetail"));
  assert(strb_tell(output) == 7 + strlen(canonical));
  strb_empty(output);
  assert(!strb_puts(output, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"));
  strb_setmode(output, strb_overwrite);
  strb_seek(output, 7);
  assert(!canonicalise_sb(output, "Test$Path", "ADFS::0.$.", "file"));
  assert(!memcmp(strb_cptr(output) + 7, canonical, strlen(canonical)));
  assert(strb_cptr(output)[7 + strlen(canonical)] == 'X');
  assert(strb_getmode(output) == strb_overwrite);

  STRB_DECL_FIXED(small, 3);
  assert(!strb_puts(small, "old"));
  assert(canonicalise_sb(small, "Test$Path", "ADFS::0.$.", "file"));
  assert(!strcmp(strb_cptr(small), "old"));
  assert(strb_tell(small) == 3);
  assert(strb_error(small));

  fail_measure = true;
  size_t const position = strb_tell(output);
  assert(canonicalise_sb(output, "Test$Path", "ADFS::0.$.", "file"));
  assert(strb_tell(output) == position);
  fail_measure = false;
  fail_write = true;
  assert(canonicalise_sb(output, "Test$Path", "ADFS::0.$.", "file"));
  fail_write = false;
#ifdef FORTIFY
  STRB_DECL_ALLOC(growing, 0);
  Fortify_SetNumAllocationsLimit(0);
  assert(canonicalise_sb(growing, "Test$Path", "ADFS::0.$.", "file"));
  Fortify_SetNumAllocationsLimit(ULONG_MAX);
  assert(!strcmp(strb_cptr(growing), ""));
  assert(strb_tell(growing) == 0);
  strb_free(growing);
#endif
  return 0;
}
