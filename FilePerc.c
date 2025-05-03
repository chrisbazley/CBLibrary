/*
 * CBLibrary: Display progress and check for ESCAPE during file operations
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
  CJB: 13-Jan-05: Changed to use new msgs_error() function, hence no longer
                  requires external error block 'shared_err_block'.
  CJB: 06-Feb-06: Miscellaneous changes in perc_operation() aimed at
                  simplifying program control flow.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 24-Oct-06: Renamed ensure_timer_removed() as _fp_deregister_timer().
                  Moved code to control the hourglass and escape key into a
                  separate functions _fp_enable_esc() and _fp_disable_esc().
                  Split the old function perc_operation() into two new
                  functions file_perc_load() and file_perc_save(), thus
                  eliminating unused arguments for load operations.
                  file_perc_save() allows any sub-section of a flex block to be
                  saved instead of making special provision for sprite areas
                  (hence the top bit of the file type value should not be set).
                  It sets the file type itself instead of delegating this to
                  save_compressedM() or save_fileM(). Re-implemented
                  perc_operation() as a complex veneer to file_perc_load() and
                  file_perc_save().
  CJB: 22-Jun-09: Whitespace changes only.
  CJB: 06-Sep-09: Updated to use new function set_file_type().
  CJB: 14-Oct-09: Titivated formatting, replaced 'magic' values with named
                  constants and removed dependency on MsgTrans by creating an
                  initialisation function which stores a pointer to a messages
                  file descriptor. Now uses MERGE_ERR macro where appropriate.
  CJB: 05-May-12: Added support for stress-testing failure of _kernel_osbyte.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 18-Apr-16: Cast pointer parameters to void * to match %p.
  CJB: 01-Nov-18: Replaced DEBUG macro usage with DEBUGF.
  CJB: 03-May-25: Fix #include filename case.
 */

/* ISO library headers */
#include <stdlib.h>
#include <stdio.h>
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
#include "Timer.h"
#include "FedCompMT.h"
#ifndef COMP_OPS_ONLY
#include "LoadSaveMT.h"
#endif
#include "AbortFOp.h"
#include "FilePerc.h"
#include "FileUtils.h"
#ifdef CBLIB_OBSOLETE
#include "msgtrans.h"
#endif /* CBLIB_OBSOLETE */

/* Constant numeric values */
enum
{
  HourglassUpdateFrequency    = 10,  /* in centiseconds */
  OSByte_RWEscapeKeyStatus    = 229, /* _kernel_osbyte reason code */
  OSByte_ClearEscapeCondition = 124  /* _kernel_osbyte reason code */

};

static volatile bool time_up = true;
static bool at_exit = false;
static MessagesFD *desc;

/* ----------------------------------------------------------------------- */
/*                       Function prototypes                               */

static void _fp_deregister_timer(void);
static CONST _kernel_oserror *_fp_enable_esc(void);
static CONST _kernel_oserror *_fp_disable_esc(void);
static CONST _kernel_oserror *lookup_error(const char *token);

/* ----------------------------------------------------------------------- */
/*                         Public functions                                */

CONST _kernel_oserror *file_perc_initialise(MessagesFD *mfd)
{
  CONST _kernel_oserror *e;

  /* Store pointer to messages file descriptor */
  desc = mfd;

  /* Ensure that subsidiary modules have also been initialised */
  e = compress_initialise(mfd);
#ifndef COMP_OPS_ONLY
  if (e == NULL)
    e = loadsave_initialise(mfd);
#endif
  return e;
}

/* ----------------------------------------------------------------------- */

CONST _kernel_oserror *file_perc_load(FilePercOp type, const char *file_path, flex_ptr buffer_anchor)
{
  FILE **handle = NULL;
  CONST _kernel_oserror *err;

  DEBUGF("FilePerc: Starting a load operation of type %d with path '%s' and "
        "flex pointer %p\n", type, file_path, (void *)buffer_anchor);
#ifdef COMP_OPS_ONLY
  assert(type == FilePercOp_Decomp);
#else
  assert(type == FilePercOp_Decomp || type == FilePercOp_Load);
#endif
  if (type != FilePercOp_Decomp && type != FilePercOp_Load)
    return NULL;

  ON_ERR_RTN_E(_fp_enable_esc());

  do
  {
    unsigned int perc;

    /* Set up a ticker event to set the 'time_up' flag in the background */
    time_up = false;
    err = timer_register(&time_up, HourglassUpdateFrequency);
    if (err != NULL) {
      time_up = true; /* could not set up ticker event */
      break;
    }

    if (type == FilePercOp_Decomp)
      err = load_compressedM(file_path, buffer_anchor, &time_up, &handle);
#ifndef COMP_OPS_ONLY
    else
      err = load_fileM2(file_path, buffer_anchor, &time_up, &handle);
#endif

    _fp_deregister_timer(); /* in case ticker event still pending */
    if (err != NULL || handle == NULL)
      break; /* an error has occurred or else operation is complete */

#ifdef COMP_OPS_ONLY
    perc = get_decomp_perc(&handle);
#else
    perc = (type == FilePercOp_Decomp ? get_decomp_perc(&handle) :
           get_loadsave_perc(&handle));
#endif
    hourglass_percentage(perc);

    /* Check for escape condition (i.e. has user pressed Esc?) */
    if (_kernel_escape_seen())
      err = lookup_error("Escape");

  }
  while (err == NULL); /* loop until user presses escape */

  MERGE_ERR(err, _fp_disable_esc());

  /* If an error occurred during loading then free the input buffer */
  if (err != NULL && *buffer_anchor != NULL)
  {
    DEBUGF("FilePerc: Deallocating input buffer (currently at %p)\n",
          *buffer_anchor);
    flex_free(buffer_anchor);
  }

  /* If stopped in mid-operation then close it down */
  if (handle != NULL)
    abort_file_op(&handle);

  return err; /* success */
}

/* ----------------------------------------------------------------------- */

CONST _kernel_oserror *file_perc_save(FilePercOp type, const char *file_path, unsigned int file_type, flex_ptr buffer_anchor, unsigned int start_offset, unsigned int end_offset)
{
  FILE **handle = NULL;
  CONST _kernel_oserror *err;

  DEBUGF("FilePerc: Starting a save operation of type %d with path '%s', "
        "flex pointer %p, data %u - %u\n", type, file_path, (void *)buffer_anchor,
        start_offset, end_offset);
#ifdef COMP_OPS_ONLY
  assert(type == FilePercOp_Comp);
#else
  assert(type == FilePercOp_Comp || type == FilePercOp_Save);
#endif
  if (type != FilePercOp_Comp && type != FilePercOp_Save)
    return NULL;

  ON_ERR_RTN_E(_fp_enable_esc());

  do
  {
    unsigned int perc;

    /* Set up a ticker event to set the 'time_up' flag in the background */
    time_up = false;
    err = timer_register(&time_up, HourglassUpdateFrequency);
    if (err != NULL)
    {
      time_up = true; /* could not set up ticker event */
      break;
    }

#ifdef COMP_OPS_ONLY
    err = save_compressedM2(file_path,
                            buffer_anchor,
                            &time_up,
                            start_offset,
                            end_offset,
                            &handle);
#else
    if (type == FilePercOp_Comp)
    {
      err = save_compressedM2(file_path,
                              buffer_anchor,
                              &time_up,
                              start_offset,
                              end_offset,
                              &handle);
    }
    else
    {
      err = save_fileM2(file_path,
                        buffer_anchor,
                        &time_up,
                        start_offset,
                        end_offset,
                        &handle);
    }
#endif

    _fp_deregister_timer(); /* in case ticker event still pending */
    if (err != NULL || handle == NULL)
      break; /* an error has occurred or else operation is complete */

#ifdef COMP_OPS_ONLY
    perc = get_comp_perc(&handle);
#else
    perc = (type == FilePercOp_Comp ? get_comp_perc(&handle) :
           get_loadsave_perc(&handle));
#endif
    hourglass_percentage(perc);

    /* Check for escape condition (i.e. has user pressed Esc?) */
    if (_kernel_escape_seen())
      err = lookup_error("Escape");

  }
  while (err == NULL); /* loop until user presses escape */

  MERGE_ERR(err, _fp_disable_esc());

  /* If save completed successfully then set requested file type */
  if (handle == NULL && err == NULL)
    err = set_file_type(file_path, file_type);

  /* If stopped in mid-operation then close it down */
  if (handle != NULL)
    abort_file_op(&handle);

  return err; /* success */
}

/* ----------------------------------------------------------------------- */

#ifdef CBLIB_OBSOLETE
/* The following function is deprecated; use file_perc_save or file_perc_load */
CONST _kernel_oserror *perc_operation(FilePercOp type, const char *file_path, unsigned int file_type, flex_ptr buffer_anchor)
{
  if (type == FilePercOp_Save || type == FilePercOp_Comp)
  {
    unsigned int start_offset;
    SpriteAreaHeader *fake = (SpriteAreaHeader *)0;

    if (TEST_BITS(file_type, FILEPERC_SPRITEAREA) && type == FilePercOp_Save)
      start_offset = sizeof(fake->size);
    else
      start_offset = 0;

    ON_ERR_RTN_E(file_perc_save(type,
                                file_path,
                                file_type & ~FILEPERC_SPRITEAREA,
                                buffer_anchor,
                                start_offset,
                                flex_size(buffer_anchor)));
  }
  else if (type == FilePercOp_Load || type == FilePercOp_Decomp)
  {
    ON_ERR_RTN_E(file_perc_load(type, file_path, buffer_anchor));

    if (TEST_BITS(file_type, FILEPERC_SPRITEAREA) &&
        type == FilePercOp_Load &&
        *buffer_anchor != NULL)
    {
      DEBUGF("FilePerc: Pre-pending sprite area size\n");
      SpriteAreaHeader **area = (SpriteAreaHeader **)buffer_anchor;
      if (!flex_midextend(buffer_anchor, 0, sizeof((*area)->size)))
      {
        /* Failed to extend input buffer at start */
        return lookup_error("NoMem");
      }
      /* Write sprite area size as first word */
      (*area)->size = flex_size(buffer_anchor);
    }
  }

  return NULL; /* success */
}
#endif

/* ----------------------------------------------------------------------- */
/*                         Private functions                               */

static CONST _kernel_oserror *lookup_error(const char *token)
{
#ifdef CBLIB_OBSOLETE
  /* This is an ugly hack to make sure that application-specific messages are
     still picked up even if the client program didn't register a message file
     descriptor with file_perc_initialise */
  if (desc == NULL)
    desc = msgs_get_descriptor();
#endif /* CBLIB_OBSOLETE */

  /* Look up error message from the token, outputting to an internal buffer */
  return messagetrans_error_lookup(desc, DUMMY_ERRNO, token, 0);
}

/* ----------------------------------------------------------------------- */

static void _fp_deregister_timer(void)
{
  if (!time_up)
  {
    timer_deregister(&time_up);
    /* (suppress errors, as event may occur between check and removal) */
    time_up = true;
  }
}

/* ----------------------------------------------------------------------- */

static CONST _kernel_oserror *_fp_enable_esc(void)
{
  /* Enable escape key & reset escape detection */
  if (_kernel_osbyte(OSByte_RWEscapeKeyStatus, 0, 0) == _kernel_ERROR)
    return _kernel_last_oserror();

  _kernel_escape_seen();

  if (!at_exit)
  {
    /* Last-ditch effort to remove ticker event routine, in case it is still
       pending when the client program terminates */
    atexit(_fp_deregister_timer);
    at_exit = true;
  }

  hourglass_on();

  return NULL; /* no error */
}

/* ----------------------------------------------------------------------- */

static CONST _kernel_oserror *_fp_disable_esc(void)
{
  hourglass_off();

  /* Disable escape key & clear any escape condition */
  if (_kernel_osbyte(OSByte_RWEscapeKeyStatus, 1, 0) == _kernel_ERROR ||
      _kernel_osbyte(OSByte_ClearEscapeCondition, 0, 0) == _kernel_ERROR)
  {
    return _kernel_last_oserror();
  }

  return NULL; /* no error */
}
