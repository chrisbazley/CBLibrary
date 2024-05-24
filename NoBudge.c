/*
 * CBLibrary: Manage the flex memory heap budge state for multiple clients
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
  CJB: 02-Nov-04: Removed declaration of external shared_err_block (no longer
                  required).
  CJB: 15-Jan-05: Changed to use new DEBUG macro rather than ugly in-line code.
  CJB: 08-Jul-05: Misc minor changes.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 10-Jan-16: Recategorized all debugging output as 'verbose'.
  CJB: 01-Nov-18: Replaced DEBUG_VERBOSE macro usage with DEBUG_VERBOSEF.
  CJB: 13-Apr-19: nobudge_register() no longer tries to allocate memory if
                  flex budge is already disabled or 0 bytes were requested.
*/

/* ISO library headers */
#include <stdlib.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "flex.h"

/* Local headers */
#include "Internal/CBMisc.h"
#include "NoBudge.h"

static int default_state;
static unsigned int num_no_budge;

/* ----------------------------------------------------------------------- */
/*                         Public functions                                */

void nobudge_register(size_t const heap_ensure)
{
  if (++num_no_budge == 1)
  {
    /* We have first client - disable flex budging */
    DEBUG_VERBOSEF("Disabling flex budge (first user registered)\n");
    if (heap_ensure > 0)
    {
      free(malloc(heap_ensure));
    }
    default_state = flex_set_budge(0);
  }
  else
  {
    DEBUG_VERBOSEF("Non budging flex user registered (now %u)\n", num_no_budge);
  }
}

/* ----------------------------------------------------------------------- */

void nobudge_deregister(void)
{
  /* Can't deregister if there are no clients! */
  assert(num_no_budge >= 1);
  if (num_no_budge < 1)
    return; /* bad deregistration */

  if (--num_no_budge == 0)
  {
    /* We have run out of clients - enable flex budging */
    DEBUG_VERBOSEF("Restoring flex budge (last user deregistered)\n");
    flex_set_budge(default_state);
  }
  else
  {
    DEBUG_VERBOSEF("Non budging flex user deregistered (%u remaining)\n", num_no_budge);
  }
}
