/*
 * CBLibrary: Set a volatile boolean variable after a specified delay
 * Copyright (C) 2016 Christopher Bazley
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
  CJB: 25-May-16: Created this source file.
  CJB: 07-Jun-16: Prevented interception of _kernel_swi for error simulation
                  in timer_deregister because it's dangerous to interfere
                  with ticker event deregistration.
 */

/* ISO library headers */
#include <stdbool.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "swis.h"

/* Local headers */
#include "Internal/CBMisc.h"
#include "Timer.h"

extern void timer_set_flag(void);

CONST _kernel_oserror *timer_register(volatile bool *timeup_flag, int wait_time)
{
  _kernel_swi_regs regs;
  *timeup_flag = false;
  regs.r[0] = wait_time;
  regs.r[1] = (int)&timer_set_flag;
  regs.r[2] = (int)timeup_flag;
  return _kernel_swi(OS_CallAfter, &regs, &regs);
}

/* It's dangerous to prevent ticker event deregistration */
#undef _kernel_swi

CONST _kernel_oserror *timer_deregister(volatile bool *timeup_flag)
{
  _kernel_swi_regs regs;
  regs.r[0] = (int)&timer_set_flag;
  regs.r[1] = (int)timeup_flag;
  return _kernel_swi(OS_RemoveTickerEvent, &regs, &regs);
}
