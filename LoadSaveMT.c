/*
 * CBLibrary: Interruptible implementation of file loading and saving
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
  CJB: 15-Jan-05: Changed to use new DEBUG macro rather than ugly in-line code.
  CJB: 09-Sep-06: Minor amendments to debugging output.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 25-Oct-06: Eliminated floating point arithmetic in get_loadsave_perc()
                  (was only used for data transfers of more than about 40 MB).
                  Renamed load_fileM() as load_fileM2(), removed reliance on EOF
                  as termination condition and special provision for sprite
                  files. Renamed function 'save_fileM' as 'save_fileM2', removed
                  code to set file type, and extended it to allow any sub-
                  section of a flex block to be saved. Created veneers with the
                  old function names for backward compatibility.
  CJB: 10-Nov-06: Amendments to debugging output.
  CJB: 22-Jun-09: Use variable name rather than type with 'sizeof' operator,
                  removed unnecessary casts from 'void *' and tweaked spacing.
                  Corrected some erroneous uses of field specifier '%d' instead
                  of '%u' for unsigned values.
  CJB: 06-Sep-09: Updated to use new function set_file_type().
  CJB: 08-Sep-09: Get rid of superfluous structure tag '_fileop_state'.
  CJB: 14-Oct-09: Titivated formatting, replaced 'magic' values with named
                  constants and macro values with 'enum'. Removed dependency on
                  MsgTrans by creating an initialisation function which stores
                  a pointer to a messages file descriptor. Now pre-expands the
                  heap by BUFSIZ instead of 4096 before disabling flex budging
                  across calls to fread/fwrite (memory is used for I/O buffer).
  CJB: 05-May-12: Added support for stress-testing failure of _kernel_osfile.
  CJB: 01-Jan-15: Apply Fortify to standard library I/O function calls.
                  Now detects failure of the fseek function.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 10-Jan-16: Increased the fwrite/fwrite granularity from 64b to 4KB.
  CJB: 18-Apr-16: Cast pointer parameters to void * to match %p.
  CJB: 01-Nov-18: Replaced DEBUG macro usage with DEBUGF.
  CJB: 06-Nov-19: Fixed failure to check the return value of fclose_dec().
  CJB: 10-Nov-19: Modified load_fileM2() to use get_file_size().
  CJB: 03-May-25: Fix #include filename case.
 */

/* ISO library headers */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
#include <string.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "flex.h"
#include "toolbox.h"

/* CBOSLib headers */
#include "MessTrans.h"
#include "Hourglass.h"
#include "SprFormats.h"

/* Local headers */
#include "Internal/CBMisc.h"
#include "NoBudge.h"
#include "LoadSaveMT.h"
#include "FopenCount.h"
#include "FileUtils.h"
#ifdef CBLIB_OBSOLETE
#include "msgtrans.h"
#endif /* CBLIB_OBSOLETE */
#include "Internal/FOpPrivate.h"

/*
  This is a workaround for the fact that IO buffering sometimes runs out when
  accessing files with flex budge (and hence heap expansion) disabled:
*/
#define STATIC_BUFFER

/* Constant numeric values */
enum
{
  Granularity                    = 4096, /* Maximum number of bytes to read/write
                                          before checking for time up */
  HeapPreExpandBig               = BUFSIZ, /* Amount to pre-expand the heap by
                                              before disabling flex budging
                                              for fread/fwrite */
  HeapPreExpandSmall             = 512 /* Amount to pre-expand the heap by
                                          before disabling flex budging for
                                          memcpy */
};

typedef struct
{
  fileop_common  common;
  long int       read_pos;
  unsigned int   start;
  unsigned int   mem_pos;
  unsigned int   limit;
}
fileop_state;

#ifdef STATIC_BUFFER
static char static_buffer[Granularity];
#endif
static MessagesFD *desc;

/* ----------------------------------------------------------------------- */
/*                       Function prototypes                               */

static CONST _kernel_oserror *lookup_error(const char *token, const char *param);

/* ----------------------------------------------------------------------- */
/*                         Public functions                                */

CONST _kernel_oserror *loadsave_initialise(MessagesFD *mfd)
{
  /* Store pointer to messages file descriptor */
  desc = mfd;
  return NULL; /* success */
}

/* ----------------------------------------------------------------------- */

unsigned int get_loadsave_perc(FILE ***handle)
{
  fileop_state *state = (fileop_state *)*handle;
  unsigned int bytes_done, total_size, perc_done;

  DEBUGF("LoadSaveMT: Request for %% done\n");

  assert(state->limit >= state->start);
  total_size = state->limit - state->start;
  if (!total_size) {
    DEBUGF("LoadSaveMT: 0 bytes to transfer!\n");
    return 100u; /* Guard against divide-by-zero */
  }

  /* And overflow on multiply... */
  assert(state->mem_pos >= state->start);
  bytes_done = state->mem_pos - state->start;
  if (bytes_done >= UINT_MAX / 100u) {
    DEBUGF("LoadSaveMT: %% calculation would overflow\n");
    return state->mem_pos < state->limit ? 0u : 100u;
  }
  perc_done = (bytes_done * 100u) / total_size;

  DEBUGF("LoadSaveMT: %u%% complete\n", perc_done);
  return perc_done;
}

/* ----------------------------------------------------------------------- */

CONST _kernel_oserror *load_fileM2(const char *file_path, flex_ptr buffer_anchor, const volatile bool *time_up, FILE ***handle)
{
  fileop_state *state;

  if (*handle == NULL)
  {
    /* Starting afresh */
    DEBUGF("LoadSaveMT: Starting file load operation\n");
    state = malloc(sizeof(*state));
    if (state == NULL)
      return lookup_error("NoMem", NULL);

    /* Get size of file */
    int size;
    CONST _kernel_oserror *const e = get_file_size(file_path, &size);
    if (e != NULL)
    {
      free(state);
      return e;
    }

    /* Allocate buffer for data */
    DEBUGF("LoadSaveMT: Allocating flex block of %d bytes anchored at %p\n",
         size, (void *)buffer_anchor);
    if (size < 0 || !flex_alloc(buffer_anchor, size))
    {
      free(state);
      return lookup_error("NoMem", NULL);
    }

    state->limit = (unsigned)size;
    state->mem_pos = 0;
    state->start = 0;
    state->common.f = NULL;
    state->common.destructor = NULL;
    state->read_pos = 0; /* start reading from beginning of file */
  }
  else
  {
    /* Continue from where we left off */
    state = (fileop_state *)*handle;
    DEBUGF("LoadSaveMT: Resume load at mem_pos %u\n", state->mem_pos);
  }

  _kernel_last_oserror(); /* reset SCL's error recording */

  if (state->common.f == NULL)
  {
    /* (Re)open file and start reading data where we left off */
    state->common.f = fopen_inc(file_path, "rb"); /* open for reading */
    if (state->common.f == NULL ||
        fseek(state->common.f, state->read_pos, SEEK_SET) == -1)
    {
      DEBUGF("LoadSaveMT: fopen_inc or fseek failed\n");
      if (state->common.f != NULL)
        fclose_dec(state->common.f);
      free(state);
      *handle = NULL; /* write back NULL pointer */
      ON_ERR_RTN_E(_kernel_last_oserror()); /* return any OS error */
      return lookup_error("OpenInFail", file_path);
    }
  }

  hourglass_on(); /* floppy discs can be really slow! */
#ifndef STATIC_BUFFER
  {
    nobudge_register(HeapPreExpandBig); /* protect pointer into flexblock */
    void *ptr = *buffer_anchor;
#endif

    if (*time_up)
      DEBUGF("LoadSaveMT: Time up before start (will do one iteration anyway)\n");

    /* Read chunks of data until time up */
    while (state->mem_pos < state->limit)
    {
      size_t chunk_size, num_read;

      if (state->mem_pos + Granularity > state->limit)
        chunk_size = state->limit - state->mem_pos;
      else
        chunk_size = Granularity;

#ifdef STATIC_BUFFER
      /* Read chunk from file into static block */
      num_read = fread(static_buffer, sizeof(char), chunk_size, state->common.f);
      if (num_read > 0)
      {
        /* Copy chunk from static block to flex */
        nobudge_register(HeapPreExpandSmall);
        memcpy((char *)*buffer_anchor + state->mem_pos, static_buffer,
               num_read);
        nobudge_deregister();
      }
#else
      /* Read chunk from file directly into flex block (budge is disabled) */
      num_read = fread((char *)ptr + state->mem_pos, sizeof(char), chunk_size,
                 state->common.f);
#endif
      if (num_read != chunk_size)
        break; /* EOF or read error */

      state->mem_pos += num_read;

      if (*time_up)
        break;
    } /* endwhile */

#ifndef STATIC_BUFFER
    nobudge_deregister();
  }
#endif
  hourglass_off();

  if (ferror(state->common.f))
  {
    /* File error on fread() */
    DEBUGF("LoadSaveMT: Aborting - file error on fread()\n");
    fclose_dec(state->common.f);
    free(state);
    *handle = NULL; /* write back NULL pointer */
    ON_ERR_RTN_E(_kernel_last_oserror()); /* return any OS error */
    return lookup_error("ReadFail", file_path);
  }

  if (state->mem_pos >= state->limit || feof(state->common.f))
  {
    /* Finished (got to end of input) */
    DEBUGF("LoadSaveMT: Loading complete%s\n", feof(state->common.f) ? " (EOF)" : "");
    DEBUGF("LoadSaveMT: Final size of loaded data is %d\n",
          flex_size(buffer_anchor));
    fclose_dec(state->common.f);
    free(state);
    *handle = NULL; /* write back NULL pointer */
  }
  else
  {
    /* Stopped before EOF */
    DEBUGF("LoadSaveMT: Loading suspended at mem_pos %u (time up)\n",
          state->mem_pos);

    if (fopen_num() >= FOPEN_MAX)
    {
      /* if we have no spare file handles then close file */
      state->read_pos = ftell(state->common.f);
      fclose_dec(state->common.f);
      state->common.f = NULL;
    }
    *handle = (FILE **)state; /* write back pointer to state */
  }
  return NULL; /* no error */
}

/* ----------------------------------------------------------------------- */

CONST _kernel_oserror *save_fileM2(const char *file_path, flex_ptr buffer_anchor, const volatile bool *time_up, unsigned int start_offset, unsigned int end_offset, FILE ***handle)
{
  fileop_state *state;
  const char *open_mode;

  if (*handle == NULL)
  {
    /* Starting afresh */
    DEBUGF("LoadSaveMT: Starting save operation\n");

    state = malloc(sizeof(*state));
    if (state == NULL)
      return lookup_error("NoMem", NULL);

    state->limit = end_offset;
    state->start= start_offset;
    state->mem_pos = start_offset;
    state->common.f = NULL;
    state->common.destructor = NULL;
    open_mode = "wb"; /* open for writing */
  }
  else
  {
    /* Continue from where we left off */
    state = (fileop_state *)*handle;
    DEBUGF("LoadSaveMT: Resume save at mem_pos %u\n", state->mem_pos);
    open_mode = "ab"; /* open for appending */
  }

  _kernel_last_oserror(); /* reset SCL's error recording */

  if (state->common.f == NULL)
  {
    /* (Re)open file */
    state->common.f = fopen_inc(file_path, open_mode);
    if (state->common.f == NULL)
    {
      free(state);
      *handle = NULL; /* write back NULL pointer */
      ON_ERR_RTN_E(_kernel_last_oserror()); /* return any OS error */
      return lookup_error("OpenOutFail", file_path);
    }
  }

  hourglass_on(); /* floppy discs can be really slow! */
#ifndef STATIC_BUFFER
  {
    nobudge_register(HeapPreExpandBig); /* protect pointer into flexblock */
    char *ptr = (char *)*buffer_anchor;
#endif

    if (*time_up)
      DEBUGF("LoadSaveMT: Time up before start\n");

    /* Write chunks of data until we run out or time up */
    while (state->mem_pos < state->limit)
    {
      size_t chunk_size, num_written;

      if (state->mem_pos + Granularity > state->limit)
        chunk_size = state->limit - state->mem_pos;
      else
        chunk_size = Granularity;

#ifdef STATIC_BUFFER
      /* Copy chunk from flex to static block */
      nobudge_register(HeapPreExpandSmall);

      memcpy(static_buffer,
             (char *)*buffer_anchor + state->mem_pos,
             chunk_size);

      nobudge_deregister();

      /* Write contents of static block out to file */
      num_written = fwrite(static_buffer, sizeof(char), chunk_size, state->common.f);
#else
      /* Write chunk of flex block out to file (budge is disabled) */
      num_written = fwrite(ptr + state->mem_pos,
                           sizeof(char),
                           chunk_size,
                           state->common.f);
#endif
      if (num_written != chunk_size)
        break; /* write error */

      state->mem_pos += num_written;

      if (*time_up)
        break;
    } /* endwhile */

#ifndef STATIC_BUFFER
    nobudge_deregister();
  }
#endif
  hourglass_off();

  bool write_fail = false;

  if (ferror(state->common.f))
  {
    DEBUGF("LoadSaveMT: File error on fwrite()\n");
    fclose_dec(state->common.f);
    write_fail = true;
    free(state);
    state = NULL;
  }
  else if (state->mem_pos >= state->limit)
  {
    /* Finished (got to end of input) */
    DEBUGF("LoadSaveMT: Saving complete\n");
    if (fclose_dec(state->common.f))
    {
      write_fail = true;
    }
    free(state);
    state = NULL;
  }
  else
  {
    /* Assume stopped cos of time out */
    DEBUGF("LoadSaveMT: Saving suspended at mem_pos %u (time up)\n",
          state->mem_pos);

    if (fopen_num() >= FOPEN_MAX)
    {
      /* if we have no spare file handles then close file */
      if (fclose_dec(state->common.f))
      {
        write_fail = true;
        free(state);
        state = NULL;
      }
      else
      {
        state->common.f = NULL;
      }
    }
  }

  *handle = (FILE **)state; /* write back pointer to state */

  if (write_fail)
  {
    ON_ERR_RTN_E(_kernel_last_oserror()); /* return any OS error */
    return lookup_error("WriteFail", file_path);
  }

  return NULL; /* success */
}

/* ----------------------------------------------------------------------- */

#ifdef CBLIB_OBSOLETE
/* The following function is deprecated; use load_fileM2(). */
CONST _kernel_oserror *load_fileM(const char *file_path, flex_ptr buffer_anchor, const volatile bool *time_up, FILE ***handle, bool sprite)
{
  ON_ERR_RTN_E(load_fileM2(file_path, buffer_anchor, time_up, handle));

  if (*handle == NULL && sprite && *buffer_anchor != NULL)
  {
    SpriteAreaHeader **area = (SpriteAreaHeader **)buffer_anchor;

    DEBUGF("LoadSaveMT: Pre-pending sprite area size\n");
    if (!flex_midextend(buffer_anchor, 0, sizeof((*area)->size)))
    {
      /* Failed to extend input buffer at start */
      return lookup_error("NoMem", NULL);
    }
    /* Write sprite area size as first word */
    (*area)->size = flex_size(buffer_anchor);
  }
  return NULL; /* success */
}

/* ----------------------------------------------------------------------- */

/* The following function is deprecated; use save_fileM2(). */
CONST _kernel_oserror *save_fileM(const char *file_path, int file_type, flex_ptr buffer_anchor, const volatile bool *time_up, FILE ***handle, bool sprite)
{
  SpriteAreaHeader *dummy = (SpriteAreaHeader *)0;
  ON_ERR_RTN_E(save_fileM2(file_path, buffer_anchor, time_up, sprite ?
                           sizeof(dummy->size) : 0, flex_size(buffer_anchor),
                           handle));

  /* Set file type */
  if (*handle == NULL)
    ON_ERR_RTN_E(set_file_type(file_path, file_type));

  return NULL; /* no error */
}
#endif /* CBLIB_OBSOLETE */

/* ----------------------------------------------------------------------- */
/*                         Private functions                               */

static CONST _kernel_oserror *lookup_error(const char *token, const char *param)
{
#ifdef CBLIB_OBSOLETE
  /* This is an ugly hack to make sure that application-specific messages are
     still picked up even if the client program didn't register a message file
     descriptor with file_perc_initialise */
  if (desc == NULL)
    desc = msgs_get_descriptor();
#endif /* CBLIB_OBSOLETE */

  /* Look up error message from the token, outputting to an internal buffer */
  return messagetrans_error_lookup(desc, DUMMY_ERRNO, token, 1, param);
}
