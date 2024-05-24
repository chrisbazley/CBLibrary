/*
 * CBLibrary: Canonicalise a file path
 * Copyright (C) 2004 Christopher Bazley
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
  CJB: 13-Jun-04: Created this source file by moving the loader_canonicalise()
                  function out of the Loader component.
  CJB: 13-Jan-05: Changed to use new msgs_error() function, hence no longer
                  requires external error block 'shared_err_block'.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointer as 'const'.
  CJB: 18-Oct-06: Added debugging output.
  CJB: 06-Sep-09: C89 automatic variable declarations.
  CJB: 09-Sep-09: Added assertions.
  CJB: 13-Oct-09: Modified to use new veneer for SWI OS_FSControl 37 and
                  remove dependency on the MsgTrans module.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 21-Apr-16: Modified format strings to avoid GNU C compiler warnings.
  CJB: 01-Nov-18: Replaced DEBUG macro usage with DEBUGF.
  CJB: 25-Aug-20: Fixed null pointers instead of strings passed to DEBUGF.
  CJB: 11-Dec-20: Prefer to declare variable with initializer.
 */

/* ISO library headers */
#include <stdlib.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "swis.h"

/* CBOSLib headers */
#include "MessTrans.h"
#include "OSFSCntrl.h"

/* Local headers */
#include "Internal/CBMisc.h"
#include "Macros.h"
#include "FileUtils.h"

/* ----------------------------------------------------------------------- */
/*                         Public functions                                */

CONST _kernel_oserror *canonicalise(char **b, const char *pv, const char *ps, const char *f)
{
  assert(b != NULL);
  assert(f != NULL);
  DEBUGF("Canonical: About to do path '%s' with variable '%s' and string '%s'\n",
        f, STRING_OR_NULL(pv), STRING_OR_NULL(ps));

  /* First pass - determine buffer size needed */
  size_t nbytes;
  CONST _kernel_oserror *e = os_fscontrol_canonicalise(
                                 NULL, 0, pv, ps, f, &nbytes);
  if (e == NULL)
  {
    char *result;

    DEBUGF("Canonical: Allocating string buffer of %zu bytes\n", nbytes);
    result = malloc(nbytes);
    if (result == NULL)
    {
      e = messagetrans_error_lookup(NULL, DUMMY_ERRNO, "NoMem", 0);
    }
    else
    {
      /* Second pass - write canonicalised path */
      e = os_fscontrol_canonicalise(result, nbytes, pv, ps, f, NULL);
      if (e == NULL)
      {
        DEBUGF("Canonical: result is '%s'\n", result);
        *b = result;
      }
      else
      {
        DEBUGF("Canonical: SWI error 0x%x '%s' (2)\n", e->errnum, e->errmess);
        free(result);
      }
    }
  }
  else
  {
    DEBUGF("Canonical: SWI error 0x%x '%s' (1)\n", e->errnum, e->errmess);
  }

  return e;
}
