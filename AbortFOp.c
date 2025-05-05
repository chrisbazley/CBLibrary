/*
 * CBLibrary: Abort an interruptible file operation
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
  CJB: 04-Jan-11: Rewrote abort_file_op to use struct fileop_common and call
                  the associated destructor function, if any.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 28-Oct-20: A destructor called by abort_file_op may require the file
                  handle to be open so don't close it.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
*/

/* ISO library headers */
#include <stdlib.h>
#include <stdio.h>

/* Local headers */
#include "FOpenCount.h"
#include "AbortFOp.h"
#include "Internal/FOpPrivate.h"
#include "Internal/CBMisc.h"

void abort_file_op(FILE *_Optional **handle)
{
  _Optional fileop_common *fop;

  assert(handle != NULL);
  fop = (fileop_common *)*handle;
  if (fop != NULL)
  {
    if (fop->destructor)
    {
      fop->destructor(&*fop);
    }
    else
    {
      if (fop->f != NULL)
      {
        fclose_dec(&*fop->f);
      }
      free(fop);
    }

    *handle = NULL; /* write back NULL pointer */
  }
}
