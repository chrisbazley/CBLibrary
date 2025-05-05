/*
 * CBLibrary: Manage the event mask for multiple clients requiring null events
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
  CJB: 03-Jul-05: Updated to use bit manipulation macros.
  CJB: 08-Jul-05: Added debugging statements & misc minor changes.
  CJB: 25-Oct-06: Made compilation of this source file conditional upon pre-
                  processor symbol CBLIB_OBSOLETE (the null event mask is now
                  managed by c.Scheduler).
  CJB: 19-Oct-09: Rescinded deprecation of this module for SF3000.
  CJB: 21-Oct-09: Updated debugging output.
  CJB: 23-Dec-14: Apply Fortify to Event library function calls.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
 */

/* ISO library headers */

/* Acorn C/C++ library headers */
#include "event.h"

/* Local headers */
#include "NullPoll.h"
#include "Internal/CBMisc.h"

static int num_null_pollers;

/* ----------------------------------------------------------------------- */
/*                         Public functions                                */

void nullpoll_register(void)
{
  if (++num_null_pollers == 1)
  {
    /* We have first client - unmask null events */
    unsigned int event_mask;

    DEBUGF("NullPoll: Unmasking null events (first client registered)\n");
    event_get_mask(&event_mask);
    CLEAR_BITS(event_mask, Wimp_Poll_NullMask);
    event_set_mask(event_mask);
  }
  else
  {
    DEBUGF("NullPoll: Client registered (now %u)\n", num_null_pollers);
  }
}

/* ----------------------------------------------------------------------- */

void nullpoll_deregister(void)
{
  /* Can't deregister if there are no clients! */
  assert(num_null_pollers >= 1);
  if (num_null_pollers < 1)
    return; /* bad deregistration */

  if (--num_null_pollers == 0)
  {
    /* We have run out of clients - mask null events */
    unsigned int event_mask;

    DEBUGF("NullPoll: Masking null events (last client deregistered)\n");
    event_get_mask(&event_mask);
    SET_BITS(event_mask, Wimp_Poll_NullMask);
    event_set_mask(event_mask);
  }
  else
  {
    DEBUGF("NullPoll: Client deregistered (%u remain)\n", num_null_pollers);
  }
}
