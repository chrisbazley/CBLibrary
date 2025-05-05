/*
 * CBLibrary: Veneers to star commands for Fednet game file (de)compression
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
  CJB: 05-Nov-04: Added 'const' qualifier to char * function arguments.
                  Updated to use the NoBudge component rather than calling
                  flex_set_budge directly.
                  Removed unnecessary inclusion of Timer header.
  CJB: 06-Nov-04: Eliminated usage of newly deprecated msgs_lookup_sub() macro.
  CJB: 13-Jan-05: Changed to use new msgs_error... functions, hence no
                  longer requires external error block 'shared_err_block'.
                  Fixed bug in load_compressed() where flex memory allocated
                  for file would leak if compiled with OLD_SCL_STUBS and
                  insufficent memory for CLI command.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 06-Sep-09: Updated to use new function set_file_type().
  CJB: 09-Sep-09: C89 automatic variable declarations. Added assertions.
  CJB: 14-Oct-09: Replaced 'magic' values with named constants and made
                  compilation of this module dependent on CBLIB_OBSOLETE.
  CJB: 18-Feb-12: Additional assertions to detect string formatting errors
                  and buffer overflow/truncation.
  CJB: 05-May-12: Added support for stress-testing failure of _kernel_oscli.
  CJB: 01-Jan-15: Apply Fortify to standard library I/O function calls.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 06-Apr-16: The return value of sprintf is now explicitly ignored to
                  avoid GNU C compiler warnings.
  CJB: 04-Nov-18: Got rid of OLD_SCL_STUBS usage (assumed to be always true).
  CJB: 03-May-25: Fix #include filename case.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
 */

#ifdef CBLIB_OBSOLETE /* Use c.FedCompMT instead */

/* ISO library headers */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "flex.h"

/* CBOSLib headers */
#include "Hourglass.h"

/* Local headers */
#include "msgtrans.h"
#include "NoBudge.h"
#include "FednetComp.h"
#include "FileUtils.h"
#include "Internal/CBMisc.h"

/* Constant numeric values */
enum
{
  CLoadCmdSize  = sizeof("CLoad  &00000000"),
  CSaveCmdSize  = sizeof("CSave  &00000000 &00000000"),
  PreExpandHeap = 512 /* Number of bytes to pre-allocate before disabling
                         flex budging (and thus heap expansion). */
};

/* ----------------------------------------------------------------------- */
/*                         Public functions                                */

_Optional CONST _kernel_oserror *load_compressed(const char *file_path, flex_ptr buffer_anchor)
{
  /* Allocate buffer and load compressed Fednet datafile */
  assert(file_path != NULL);
  assert(buffer_anchor != NULL);

  {
    int buffer_size, err, nout;
    FILE *read_file;
    const size_t command_size = CLoadCmdSize + strlen(file_path);
    char *command;

    _kernel_last_oserror(); /* reset SCL's error recording */

    /* Get (decompressed) memory requirements */
    read_file = fopen(file_path, "r");
    if (read_file == NULL)
    {
      ON_ERR_RTN_E(_kernel_last_oserror()); /* return any OS error */
      return msgs_errorsubn(DUMMY_ERRNO, "OpenInFail", 1, file_path);
    }

    if (fread(&buffer_size, sizeof(buffer_size), 1, read_file) != 1)
    {
      fclose(read_file);
      ON_ERR_RTN_E(_kernel_last_oserror()); /* return any OS error */
      return msgs_errorsubn(DUMMY_ERRNO, "ReadFail", 1, file_path);
    }
    fclose(read_file);

    /* Allocate buffer for data */
    if (!flex_alloc(buffer_anchor, buffer_size))
      return msgs_error(DUMMY_ERRNO, "NoMem");

    /* Construct CLI command */
    command = malloc(command_size);
    if (command == NULL)
    {
      flex_free(buffer_anchor); /* bugfix 13/01/05 */
      return msgs_error(DUMMY_ERRNO, "NoMem");
    }

    nobudge_register(PreExpandHeap); /* prevent budge */

    nout = sprintf(command,
            "Cload %s &%X", file_path, (int)*buffer_anchor);
    assert(nout >= 0); /* no formatting error */
    assert(nout < command_size); /* no buffer overflow/truncation */
    NOT_USED(nout);

    /* Decompress file */
    hourglass_on();
    err = _kernel_oscli(command);
    hourglass_off();

    free(command);

    nobudge_deregister(); /* allow budge */

    if (err == _kernel_ERROR) {
      flex_free(buffer_anchor);
      return _kernel_last_oserror(); /* fail */
    }
  }
  return NULL; /* success */
}

/* ----------------------------------------------------------------------- */

_Optional CONST _kernel_oserror *save_compressed(const char *file_path, int file_type, flex_ptr buffer_anchor)
{
  /* Save the specified memory as a compressed Fednet file */
  assert(file_path != NULL);
  assert(buffer_anchor != NULL);
  assert(*buffer_anchor != NULL);

  {
    int err, nout;
    const size_t command_size = CSaveCmdSize + strlen(file_path);
    char *command;

    command = malloc(command_size);
    if (command == NULL)
      return msgs_error(DUMMY_ERRNO, "NoMem");

    nobudge_register(PreExpandHeap); /* prevent budge */

    /* Construct CLI command */
    nout = sprintf(command,
            "CSave %s &%X &%X", file_path, (int)*buffer_anchor,
            ((int)*buffer_anchor + flex_size(buffer_anchor)));

    assert(nout >= 0); /* no formatting error */
    assert(nout < command_size); /* no buffer overflow/truncation */
    NOT_USED(nout);

    /* Compress file */
    hourglass_on();
    err = _kernel_oscli(command);
    hourglass_off();

    free(command);

    nobudge_deregister(); /* allow budge */
    if (err == _kernel_ERROR)
      return _kernel_last_oserror(); /* fail */
  }

  /* Set file type */
  return set_file_type(file_path, file_type);
}
#else /* CBLIB_OBSOLETE */
#error Source file FednetComp.c is deprecated
#endif /* CBLIB_OBSOLETE */
