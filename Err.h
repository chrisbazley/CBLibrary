/*
 * CBLibrary: Report errors to the user via a dialogue box
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

/* Err.h declares declares many functions and defines several macros that allow
   a RISC OS application to report fatal or non-fatal errors to a user via
   the window manager's standard error box system. Allowance is made for the
   more limited facilities available on versions of RISC OS earlier than 3.5.

Dependencies: ANSI C library, Acorn library kernel, Acorn's WIMP library.
Message tokens: FatErr, NewErr, OldErr, ErrButtons.
History:
  (Original version by Tony Houghton for !FormText.)
  CJB: 08-Mar-04: Marked macro RE as deprecated; its presence in this header
                  file was always anomalous (unlike EF and E, which are merely
                  synonyms for our function names).
  CJB: 18-Jun-04: The err_check_fatal macro definition is now wrapped in a dummy
                  do-while iterator to allow usage consistent with an expression
                  statement (see h.Macros).
  CJB: 04-Nov-04: Added dependency information and summary text.
  CJB: 05-Nov-04: Removed definition of deprecated macro RE.
  CJB: 06-Nov-04: Added clib-style documentation.
                  Removed err_box_die() from this header - don't believe it is
                  used externally in any programs, questionable if it ever was.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 22-Jun-09: Changes to whitespace only.
  CJB: 13-Oct-09: Added prototype of a new function, err_initialise, which
                  should be used to initialise this module in preference to
                  err_set_taskname. Marked the latter as deprecated.
  CJB: 04-Jul-10: Made inclusion of err_set_taskname prototype conditional.
  CJB: 20-Dec-11: Amended to allow pointers to 'const' _kernel_oserror as
                  function and macro parameters irrespective of CBLIB_OBSOLETE.
  CJB: 11-Dec-20: Deleted redundant uses of the 'extern' keyword.
  CJB: 04-Jun-21: Redefined the macro err_check_fatal() and the function
                  err_check() as inline functions.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
*/

#ifndef Err_h
#define Err_h

/* ISO library headers */
#include <stdbool.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "toolbox.h"

/* Local headers */
#include "Macros.h"

#if !defined(USE_OPTIONAL) && !defined(_Optional)
#define _Optional
#endif

/* Define some abbreviations for 'wrapper' functions that are used to
   check for error return from other functions */
#define EF(func) err_check_fatal(func)
#define E(func) err_check(func)

_Optional CONST _kernel_oserror *err_initialise(const char           * /*name*/,
                                                bool                   /*new_errs*/,
                                                _Optional MessagesFD * /*mfd*/);
   /*
    * Records 'name' as the application name to be used in the title of Wimp
    * error boxes (maximum 31 characters). The value of 'new_errs' determines
    * whether or not the Err component will attempt to use the RISC OS 3.5
    * extensions to the SWI Wimp_ReportError (typically a program should check
    * for Window Manager version >= 321 when calling this function). Unless
    * 'mfd' is a null pointer, the specified messages file will be given
    * priority over the global messages file when looking up text required by
    * this module.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

void err_check_rep(const _kernel_oserror * /*er*/);
   /*
    * Reports the error pointed to by 'er' in a standard non-multitasking Wimp
    * error box. The user will be asked whether to quit or attempt to continue
    * execution. If err_suppress_errors has been called then the error will
    * be recorded rather than reported to the user.
    */

static inline bool err_check(_Optional const _kernel_oserror *const er)
{
  if (er == NULL)
  {
    return false;
  }
  else
  {
    err_check_rep(&*er);
    return true;
  }
}
   /*
    * Checks to see whether or not 'er' is a NULL pointer, and if not reports
    * the error in a standard non-multitasking Wimp error box. The user will
    * be asked whether to quit or attempt to continue execution, so do not use
    * this wrapper around functions that may legitimately fail or when you have
    * made good provision for recovery from the error. The error report will be
    * suppressed if err_suppress_errors has been called.
    * Returns: true if an error was reported and the user did not quit,
    *          otherwise false.
    */

void err_check_fatal_rep(const _kernel_oserror * /*er*/);
   /*
    * Reports the error pointed to by 'er' in a standard non-multitasking Wimp
    * error box, and then quits the application.
    */

static inline void err_check_fatal(_Optional const _kernel_oserror *const er)
{
  if (er != NULL)
  {
    err_check_fatal_rep(&*er);
  }
}
   /*
    * Checks to see whether or not 'er' is a NULL pointer, and if not reports
    * the error in a standard non-multitasking Wimp error box then quits the
    * application.
    */

void err_report(int /*num*/, const char * /*mess*/);	/* OK button only */
   /*
    * Reports the error message 'mess' in a standard non-multitasking Wimp
    * error box, returning when the user clicks the 'Continue' button. This
    * should be used for warnings and other non-fatal errors. 'num' will be used
    * to fill in the first word of the error structure, and may affect the way
    * that the Wimp displays the error if it is deemed serious enough. If
    * err_suppress_errors has been called then the message will be recorded
    * rather than reported to the user.
    */

void err_complain(int /*num*/, const char * /*mess*/);	/* Cancel & OK buttons */
   /*
    * Reports the error message 'mess' in a standard non-multitasking Wimp
    * error box. 'num' will be used to fill in the first word of the error
    * structure, and may affect the way that the Wimp displays the error if it
    * is deemed serious enough. The user will be asked whether to quit or
    * attempt to continue execution. If err_suppress_errors has been called
    * then the error will be recorded rather than reported to the user.
    */

void err_complain_fatal(int /*num*/, const char * /*mess*/);	/* Cancel button only */
   /*
    * Reports the error message 'mess' in a standard non-multitasking Wimp
    * error box, and then quits the program.
    * 'num' will be used to fill in the first word of the error
    * structure, and may affect the way that the Wimp displays the error if it
    * is deemed serious enough.
    */

void err_suppress_errors(void);
   /*
    * Enables suppression of non-fatal errors (i.e. those that would otherwise
    * be reported to the user by err_report, err_complain or err_check_rep)
    * and wipes any existing recorded error so that err_dump_suppressed returns
    * NULL. Note that this function may not be included depending on a build
    * switch when the source code is compiled.
    */

_Optional CONST _kernel_oserror *err_dump_suppressed(void);
   /*
    * Disables suppression of non-fatal errors and returns a pointer to
    * the last error to be suppressed by err_report, err_complain or
    * err_check_rep. Note that this function may not be included depending
    * on a build switch when the source code is compiled.
    * Returns: a pointer to the last error since err_suppress_errors was
    *          called, or NULL if no error has occurred.
    */

#ifdef CBLIB_OBSOLETE
/* The following function is deprecated and should not be used in
   new or updated programs. */
void err_set_taskname(const char * /*name*/, bool /*new_errs*/);
#endif /* CBLIB_OBSOLETE */

#endif
