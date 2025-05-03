/*
 * CBLibrary: Interruptible implementation of Fednet game file (de)compression
 * Copyright (C) 2003 Christopher Bazley
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
  CJB: 07-Mar-04: Updated to use the new macro names defined in h.Macros.
  CJB: 13-Jun-04: Because all macro definitions are now expression statements,
                  have changed those invocations which omitted a trailing ';'.
  CJB: 06-Nov-04: Eliminated usage of newly deprecated msgs_lookup_sub() macro.
  CJB: 13-Jan-05: Changed to use new msgs_error... functions, hence no
                  longer requires external error block 'shared_err_block'.
  CJB: 15-Jan-05: Eliminated floating point arithmetic from get_comp_perc() and
                  get_decomp_perc() - not worthwhile for very unlikely case of
                  overflow. Moved calc to new internal function get_perc().
                  Changed to use new DEBUG macro rather than ugly in-line code.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 17-Oct-06: Renamed function 'save_compressedM' as 'save_compressedM2',
                  removed code to set file type, and extended it to allow any
                  sub-section of a flex block to be saved. Created a veneer
                  with the old function name for backward compatibility.
  CJB: 22-Jun-09: Use variable name rather than type with 'sizeof' operator,
                  removed unnecessary casts from 'void *' and tweaked spacing.
                  Corrected some erroneous uses of field specifier '%d' instead
                  of '%u' for unsigned values.
  CJB: 06-Sep-09: Updated to use new function set_file_type().
  CJB: 08-Sep-09: Get rid of superfluous structure tags '_decomp_state' and
                  '_comp_state'.
  CJB: 13-Oct-09: Titivated formatting and removed dependency on MsgTrans by
                  creating an initialisation function which stores a pointer to
                  a messages file descriptor.
  CJB: 05-Jan-11: Rewrote load_compressedM and save_compressedM2 to use
                  GKeyDecomp and GKeyComp instead of code derived from O'Shea.
  CJB: 07-Jan-11: Removed code relating to superfluous variable 'out_pending'.
  CJB: 08-Jan-11: No longer need to handle InputTruncated in save_compressedM2.
  CJB: 30-Nov-14: Now uses fread_int32le and fwrite_int32le from FileRWInt.h.
  CJB: 01-Jan-15: Removed definition of unused enum value FednetHeaderSize.
                  Apply Fortify to standard library I/O function calls.
                  Now detects failure of the fseek function.
  CJB: 02-Jan-15: Got rid of goto statements.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 21-Apr-16: Substituted format specifier %zu for %lu to avoid the need
                  to cast the matching parameters.
  CJB: 01-Nov-18: Replaced DEBUG macro usage with DEBUGF.
  CJB: 03-Nov-19: Rewritten to use my streams library.
  CJB: 06-Nov-19: Fixed failure to check the return value of fclose_dec().
  CJB: 29-Sep-20: Fixed missing/misplaced casts in assertions.
                  Made debugging output less verbose by default.
  CJB: 03-May-25: Fix #include filename case.
*/

/* ISO library headers */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "flex.h"
#include "toolbox.h"

/* CBUtilLib headers */
#include "FileRWInt.h"

/* StreamLib headers */
#include "WriterGKey.h"
#include "ReaderGKey.h"
#include "WriterFlex.h"
#include "ReaderFlex.h"

/* CBOSLib headers */
#include "MessTrans.h"

/* Local headers */
#include "Internal/CBMisc.h"
#include "FopenCount.h"
#include "FedCompMT.h"
#include "FileUtils.h"
#ifdef CBLIB_OBSOLETE
#include "msgtrans.h"
#endif /* CBLIB_OBSOLETE */
#include "NoBudge.h"
#include "Internal/FOpPrivate.h"

enum
{
  FednetHistoryLog2 = 9, /* Base 2 logarithm of the history size used by
                         the compression algorithm */
  BufferSize        = 256, /* I/O buffer size for compressed bitstream */
};

typedef struct
{
  fileop_common   common;
}
decomp_state;

typedef struct
{
  fileop_common  common;
  unsigned int   start_offset;
  unsigned int   end_offset;
}
comp_state;

static MessagesFD *desc;

/* ----------------------------------------------------------------------- */
/*                         Private functions                               */

static CONST _kernel_oserror *lookup_error(const char *const token,
  const char *const param)
{
#ifdef CBLIB_OBSOLETE
  /* This is an ugly hack to make sure that application-specific messages are
     still picked up even if the client program didn't register a message file
     descriptor with fednet_comp_mt_initialise */
  if (desc == NULL)
    desc = msgs_get_descriptor();
#endif /* CBLIB_OBSOLETE */

  /* Look up error message from the token, outputting to an internal buffer */
  return messagetrans_error_lookup(desc, DUMMY_ERRNO, token, 1, param);
}

/* ----------------------------------------------------------------------- */

static int get_perc(long int const progress, long int const total)
{
  DEBUGF("get_perc progress: %ld total: %ld\n", progress, total);

  if (total == 0)
    return 100; /* guard against divide-by-zero */

  if (progress >= LONG_MAX / 100) /* and overflow on multiply */
    return progress < total ? 0 : 100;

  return (int)((progress * 100) / total);
}

/* ----------------------------------------------------------------------- */

typedef enum
{
  DESTROY_OK,
  DESTROY_WRITE_FAIL,
  DESTROY_FCLOSE_FAIL
} DestroyResult;

static DestroyResult destroy_common(fileop_common *const common)
{
  DestroyResult result = DESTROY_OK;
  assert(common != NULL);
  reader_destroy(&common->reader);
  if (writer_destroy(&common->writer) < 0)
  {
    result = DESTROY_WRITE_FAIL;
  }
  if (fclose_dec(common->f) && result == DESTROY_OK)
  {
    result = DESTROY_FCLOSE_FAIL;
  }
  free(common);
  return result;
}

/* ----------------------------------------------------------------------- */

static const char *destroy_decomp(decomp_state *const decomp)
{
  assert(decomp != NULL);
  return destroy_common(&decomp->common) == DESTROY_WRITE_FAIL ? "NoMem" : NULL;
}

/* ----------------------------------------------------------------------- */

static const char *destroy_comp(comp_state *const comp)
{
  assert(comp != NULL);
  return destroy_common(&comp->common) != DESTROY_OK ? "WriteFail" : NULL;
}

/* ----------------------------------------------------------------------- */

static void destroy_cb(void *const fop)
{
  (void)destroy_common(fop);
}

/* ----------------------------------------------------------------------- */

static decomp_state *make_decomp(const char *const file_path,
  flex_ptr buffer_anchor, const char **const e_token)
{
  const char *token = NULL;
  decomp_state *state = malloc(sizeof(*state));
  if (state == NULL)
  {
    token = "NoMem";
  }
  else
  {
    state->common.destructor = destroy_cb;
    state->common.f = fopen_inc(file_path, "rb"); /* open for reading */

    if (state->common.f == NULL)
    {
      DEBUGF("fopen_inc failed\n");
      token = "OpenInFail";
    }
    else
    {
      /* Get size of decompressed data */
      long int len;
      if (!fread_int32le(&len, state->common.f) ||
          fseek(state->common.f, 0, SEEK_SET))
      {
        DEBUGF("fread_int32le or fseek failed\n");
        token = "ReadFail";
      }
      else if (len < 0 || len > INT_MAX ||
               !flex_alloc(buffer_anchor, (int)len))
      {
        DEBUGF("flex_alloc %ld failed\n", len);
        token = "NoMem";
      }
      else
      {
        state->common.len = len;
        if (!reader_gkey_init(&state->common.reader, FednetHistoryLog2,
          state->common.f))
        {
          DEBUGF("reader_gkey_init failed\n");
          token = "NoMem";
          flex_free(buffer_anchor);
        }
        else
        {
          writer_flex_init(&state->common.writer, buffer_anchor);
        }
      }
      if (token)
      {
        fclose_dec(state->common.f);
      }
    }
    if (token)
    {
      free(state);
      state = NULL;
    }
  }
  *e_token = token;
  return state;
}

/* ----------------------------------------------------------------------- */

static comp_state *make_comp(const char *const file_path,
  flex_ptr buffer_anchor, unsigned int const start_offset,
  unsigned int const end_offset, const char **const e_token)
{
  const char *token = NULL;
  comp_state *state = malloc(sizeof(*state));
  if (state == NULL)
  {
    token = "NoMem";
  }
  else
  {
    unsigned int const len = end_offset - start_offset;
    assert(end_offset >= start_offset);
    state->common.len = len;
    state->start_offset = start_offset;
    state->end_offset = end_offset;

    state->common.destructor = destroy_cb;
    state->common.f = fopen_inc(file_path, "wb"); /* open for writing */

    if (state->common.f == NULL)
    {
      DEBUGF("fopen_inc failed\n");
      token = "OpenOutFail";
    }
    else
    {
      if (!writer_gkey_init(&state->common.writer,
        FednetHistoryLog2, len, state->common.f))
      {
        DEBUGF("writer_gkey_init failed\n");
        token = "NoMem";
      }
      else
      {
        reader_flex_init(&state->common.reader, buffer_anchor);
      }
      if (token)
      {
        fclose_dec(state->common.f);
      }
    }
    if (token)
    {
      free(state);
      state = NULL;
    }
  }
  *e_token = token;
  return state;
}

/* ----------------------------------------------------------------------- */
/*                         Public functions                                */

CONST _kernel_oserror *compress_initialise(MessagesFD *const mfd)
{
  /* Store pointer to messages file descriptor */
  desc = mfd;
  return NULL; /* success */
}

/* ----------------------------------------------------------------------- */

unsigned int get_decomp_perc(FILE ***const handle)
{
  assert(handle != NULL);
  const decomp_state *const state = (decomp_state *)*handle;
  assert(state != NULL);

  long int const wpos = writer_ftell(&state->common.writer);
  assert(wpos >= 0);
  DEBUGF("get_decomp_perc wpos: %ld\n", wpos);
  return get_perc(wpos, state->common.len);
}

/* ----------------------------------------------------------------------- */

unsigned int get_comp_perc(FILE ***const handle)
{
  assert(handle != NULL);
  const comp_state *const state = (comp_state *)*handle;
  assert(state != NULL);

  long int const rpos = reader_ftell(&state->common.reader);
  assert(rpos >= 0);
  assert(state->start_offset <= (unsigned long)rpos);
  assert(state->end_offset >= (unsigned long)rpos);
  DEBUGF("get_comp_perc start_offset: %u rpos: %ld\n",
        state->start_offset, rpos);

  return get_perc(rpos - state->start_offset, state->common.len);
}

/* ----------------------------------------------------------------------- */

CONST _kernel_oserror *load_compressedM(const char *const file_path,
  flex_ptr buffer_anchor, const volatile bool *const time_up,
  FILE ***const handle)
{
  assert(file_path != NULL);
  assert(buffer_anchor != NULL);
  assert(handle != NULL);
  const char *e_token = NULL;
  decomp_state *state = (decomp_state *)*handle;

  if (state == NULL)
  {
    DEBUGF("starting decompression process\n");
    state = make_decomp(file_path, buffer_anchor, &e_token);
  }

  if (state)
  {
    DEBUGF("Time up %d at start\n", *time_up);
    char buffer[BufferSize];
    do
    {
      size_t const nread = reader_fread(buffer, 1, sizeof(buffer),
        &state->common.reader);

      DEBUG_VERBOSEF("Read %zu of %zu bytes\n", nread, sizeof(buffer));
      assert(nread <= sizeof(buffer));
      if (reader_ferror(&state->common.reader))
      {
        DEBUGF("reader_fread failed\n");
        e_token = "ReadFail";
        break;
      }

      size_t const nwrote = writer_fwrite(buffer, 1, nread,
        &state->common.writer);

      assert(nwrote <= nread);
      if (nwrote != nread)
      {
        DEBUGF("Wrote %zu of %zu bytes\n", nwrote, nread);
        e_token = "NoMem";
        break;
      }
    }
    while (!reader_feof(&state->common.reader) && !*time_up);

    if (e_token != NULL || reader_feof(&state->common.reader))
    {
      DEBUGF("Decompression complete or error\n");
      if (state)
      {
        const char *const e = destroy_decomp(state);
        if (e_token == NULL)
        {
          e_token = e;
        }
      }
      state = NULL;
    }
    else
    {
      DEBUGF("Pausing decompression\n");
    }
  }

  *handle = (FILE **)state; /* write back pointer to state */
  return e_token != NULL ? lookup_error(e_token, file_path) : NULL;
}

/* ----------------------------------------------------------------------- */

CONST _kernel_oserror *save_compressedM2(const char *const file_path,
  flex_ptr buffer_anchor, const volatile bool *const time_up,
  unsigned int const start_offset, unsigned int const end_offset,
  FILE ***const handle)
{
  assert(file_path != NULL);
  assert(buffer_anchor != NULL);
  assert(end_offset >= start_offset);
  assert(end_offset <= (unsigned)flex_size(buffer_anchor));
  assert(handle != NULL);
  comp_state *state = (comp_state *)*handle;
  const char *e_token = NULL;

  if (state == NULL)
  {
    DEBUGF("starting compression process\n");
    state = make_comp(file_path, buffer_anchor, start_offset, end_offset,
      &e_token);

    if (state)
    {
      int const err = reader_fseek(&state->common.reader,
        start_offset, SEEK_SET);
      assert(!err);
      NOT_USED(err);
    }
  }

  if (state)
  {
    DEBUGF("Time up %d at start\n", *time_up);
    char buffer[BufferSize];
    bool truncated = false;
    do
    {
      long int const rpos = reader_ftell(&state->common.reader);
      assert(rpos >= 0);
      assert(state->start_offset <= (unsigned long)rpos);
      assert(state->end_offset >= (unsigned long)rpos);
      unsigned long const rem = state->end_offset - (unsigned long)rpos;
      DEBUGF("Still need to read %lu bytes\n", rem);

      size_t nmemb = sizeof(buffer);
      if (nmemb > rem)
      {
        DEBUGF("Truncating input\n");
        nmemb = (size_t)rem;
        truncated = true;
      }

      size_t const nread = reader_fread(buffer, 1, nmemb,
        &state->common.reader);

      DEBUG_VERBOSEF("Read %zu of %zu bytes\n", nread, nmemb);
      assert(nread <= nmemb);
      assert(!reader_ferror(&state->common.reader));

      size_t const nwrote = writer_fwrite(buffer, 1, nread,
        &state->common.writer);

      assert(nwrote <= nread);
      if (nwrote != nread)
      {
        DEBUGF("Wrote %zu of %zu bytes\n", nwrote, nread);
        e_token = "WriteFail";
        break;
      }
    }
    while (!truncated && !reader_feof(&state->common.reader) && !*time_up);

    if (e_token != NULL || truncated || reader_feof(&state->common.reader))
    {
      DEBUGF("Compression complete or error\n");
      const char *const e = destroy_comp(state);
      if (e_token == NULL)
      {
         e_token = e;
      }
      state = NULL;
    }
    else
    {
      DEBUGF("Pausing compression\n");
    }
  }

  *handle = (FILE **)state; /* write back pointer to state */
  return e_token != NULL ? lookup_error(e_token, file_path) : NULL;
}

/* ----------------------------------------------------------------------- */

#ifdef CBLIB_OBSOLETE
/* The following function is deprecated; use save_compressedM2(). */
CONST _kernel_oserror *save_compressedM(const char *file_path, int file_type, flex_ptr buffer_anchor, const volatile bool *time_up, FILE ***handle)
{
  ON_ERR_RTN_E(save_compressedM2(file_path, buffer_anchor, time_up, 0,
                                 flex_size(buffer_anchor), handle));

  /* Set file type */
  if (*handle == NULL)
  {
    ON_ERR_RTN_E(set_file_type(file_path, file_type));
  }
  return NULL; /* no error */
}
#endif
