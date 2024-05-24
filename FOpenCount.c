/*
 * CBLibrary: Maintain a count of the number of open files
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
  CJB: 31-Oct-06: Added debugging output.
  CJB: 01-Jan-15: Apply Fortify to standard library I/O function calls.
                  The fclose_dec function now decrements the count of open
                  files even if fclose returns a failure indication.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 18-Apr-16: Cast pointer parameters to void * to match %p.
  CJB: 01-Nov-18: Replaced DEBUG macro usage with DEBUGF.
  CJB: 28-Apr-19: Less verbose debugging output.
 */

/* ISO library headers */
#include <stdio.h>
#include <errno.h>

/* Local headers */
#include "Internal/CBMisc.h"
#include "FOpenCount.h"

static unsigned int fopen_count = 0;

FILE *fopen_inc(const char *filename, const char *mode)
{
  /* Open a file, incrementing the counter if successful */
  FILE *const f = fopen(filename, mode);
  if (f != NULL) {
    fopen_count++;
  }

  DEBUGF("FOpenCount: Opened '%s' with access '%s' as stream %p; "
         "%u files are open\n", filename, mode, (void *)f, fopen_count);

  return f;
}

int fclose_dec(FILE *stream)
{
  /* Close a file, decrementing the counter */
  int f = fclose(stream);
  if (f == EOF)
    DEBUGF("fclose %p failed (errno is %d)\n", (void *)stream, errno);

  /* At least on RISC OS, fclose() seems to return failure if the
     error indicator is set for a stream. Decrement the counter
     anyway because otherwise unit tests which rely on an equal
     no. of increments and decrements will fail. */
  assert(fopen_count > 0);
  if (fopen_count > 0) {
    fopen_count--;
  }

  DEBUGF("FOpenCount: Closed %p; %u files are open\n",
    (void *)stream, fopen_count);

  return f;
}

unsigned int fopen_num(void)
{
  /* Return the number of files currently open */
  DEBUGF("FOpenCount: Currently %u files open\n", fopen_count);
  return fopen_count;
}
