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

/* History:
  CJB: 07-May-01: Records errors for SFtoSpr.
  CJB: 08-Jun-01: Error block made global to save space.
  CJB: 03-Jul-01: Needs messages "IntErr1", "IntErr2".
  CJB: 25-Jul-01: Considerable optimisation of fancy message construction
                  uses RISC OS 3.5 extensions.
                  No longer tries to prepend "Internal Error" (pretentious).
  CJB: 04-Oct-01: Removed strncpycr() 'cos saw no real need for it.
                  Made various variables static and publicised others.
  CJB: 12-Dec-01: Removed calls to msgs_lookup() to prevent re-entrancy via
                  err_check().
                  Failure to find message tokens is now fatal error reported in
                  simplest possible manner.
  CJB: 14-Dec-01: Functions that construct errors now use a local erblk
  CJB: 25-May-02: Added signal handler and separate fancy fatal error message.
  CJB: 13-Nov-02: Separate signal handler for SIGSTAK that uses static error
                  message and doesn't perform stack limit checking
  CJB: 14-Nov-02: Removed signal handlers, since the new shared C Library does
                  a better job than the old one.
  CJB: 27-Apr-03: erblk was unnecessarily being inited twice in err_complain().
                  No return from err_box_die().
                  err_box_continue() only returns if 'Continue' selected.
                  A tiddly bit smaller.
  CJB: 05-Aug-03: Eliminated strcpy(x, "") in favour of fast x[0] = '/0';
  CJB: 20-Feb-04: Now forceably terminates strings that may have been truncated
                  by strncpy().
  CJB: 07-Mar-04: Now uses new STRCPY_SAFE macro in place of custom code (should
                  behave the same).
  CJB: 21-Apr-04: Cosmetic source code change - moved err_box_die() out of
                  'Private functions' section (because it isn't one).
  CJB: 26-Sep-04: Now uses abort() rather than exit(EXIT_FAILURE), which has
                  the advantage that it gives the user the option of a stack
                  backtrace.
  CJB: 06-Nov-04: Made err_box_die() static - don't believe it is used
                  externally in any programs, questionable if it ever was.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 14-Oct-09: Removed dependency upon MsgTrans module (except when compiled
                  with CBLIB_OBSOLETE) by registering a pointer to a messages
                  file descriptor upon initialisation. Replaced 'magic' values
                  with named constants. Now generates debugging output.
                  err_set_taskname has been superseded by err_initialise.
                  Utilise the new SWI veneer function messagetrans_lookup.
  CJB: 20-Dec-11: Amended to allow pointers to 'const' _kernel_oserror as
                  function parameters irrespective of CBLIB_OBSOLETE.
  CJB: 27-Dec-14: Failure to look up error prefix or buttons string messages
                  no longer abnormally terminates execution.
  CJB: 28-Dec-14: The first error suppressed no longer turns off error
                  suppression: subsequent errors aren't reported either.
  CJB: 01-Feb-16: The "ErrButtons" message is now looked up on initialization.
  CJB: 18-Apr-16: Cast pointer parameters to void * to match %p.
  CJB: 22-Nov-20: Clarified the debug messages from err_dump_suppressed().
  CJB: 25-Apr-21: Shuffle the static function definition order to avoid the
                  need for pre-declarations.
                  Optimise comparison with the empty string in
                  err_dump_suppressed().
                  Add 'const' qualifiers to immutable function arguments.
                  Refactored err_complain(), err_report() and err_check_rep()
                  to share instead of duplicate error suppression/recording.
                  Rewrote fancy_error() to use an internal buffer allocated
                  by messagetrans_error_lookup() instead of by its caller.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
*/

/* ISO library headers */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* Acorn C/C++ library headers */
#include "wimplib.h"
#include "kernel.h"
#include "swis.h"
#include "toolbox.h"

/* CBOSLib headers */
#include "MessTrans.h"
#include "WimpExtra.h"

/* Local headers */
#include "Err.h"
#ifdef CBLIB_OBSOLETE
#include "msgtrans.h"
#endif /* CBLIB_OBSOLETE */
#include "Internal/CBMisc.h"

/* Constant numeric values */
enum
{
  MaxTaskNameLen = 31,
  MaxButtonsLen  = 31
};

#ifndef NO_RECORD_ERR
static bool suppress_errors = false;
static _kernel_oserror recorded_error;
#endif
static bool riscos_350 = false;
static char err_taskname[MaxTaskNameLen + 1] = "application";
static _Optional MessagesFD *desc;
static char err_buttons[MaxButtonsLen + 1];

/* ----------------------------------------------------------------------- */
/*                         Private functions                               */

static _kernel_oserror *fancy_error(int const errnum, const char *const errmess,
  bool const fatal)
{
  char const *token = "FatErr";
  if (!fatal)
  {
    token = riscos_350 ? "NewErr" : "OldErr";
  }
  return messagetrans_error_lookup(desc, errnum, token, 1, errmess);
}

/* ----------------------------------------------------------------------- */

static void err_box_continue(_kernel_oserror *const er)
{
  int button;
  DEBUGF("Err: Reporting non-fatal error 0x%x '%s'\n", er->errnum, er->errmess);

  if (riscos_350)
  {
    /* Nice error box */
    button = wimp_report_error(er,
                               Wimp_ReportError_OK | Wimp_ReportError_UseCategory,
                               err_taskname,
                               NULL,
                               NULL,
                               err_buttons);
  }
  else
  {
    /* Simple error box for backwards compatibility */
    button = wimp_report_error(er,
                               Wimp_ReportError_OK | Wimp_ReportError_Cancel,
                               err_taskname);
  }

  DEBUGF("Err: User selection %d\n", button);
  if (button == Wimp_ReportError_OK)
    return; /* we live on */

  abort(); /* die (message not found, or user quit) */
}

/* ----------------------------------------------------------------------- */

static void err_box_die(_kernel_oserror *const er)
{
  DEBUGF("Err: Reporting fatal error 0x%x '%s'\n", er->errnum, er->errmess);
  if (riscos_350)
  {
    /* Nice error box */
    wimp_report_error(er,
                      Wimp_ReportError_UseCategory,
                      err_taskname,
                      NULL,
                      NULL,
                      err_buttons);
  }
  else
  {
    /* Simple error box for backwards compatibility */
    wimp_report_error(er, Wimp_ReportError_Cancel, err_taskname);
  }
  abort();
}

/* ----------------------------------------------------------------------- */

static bool err_was_suppressed(int const num, const char *const mess)
{
  assert(mess);
  NOT_USED(mess);
  NOT_USED(num);
#ifndef NO_RECORD_ERR
  /* Should we preserve the error for posterity? */
  if (suppress_errors)
  {
    DEBUGF("Err: Suppressing error 0x%x '%s'\n", num, mess);
    if (recorded_error.errmess[0] == '\0')
    {
      recorded_error.errnum = num;
      STRCPY_SAFE(recorded_error.errmess, mess);
    }
    return true;
  }
#endif
  return false;
}

/* ----------------------------------------------------------------------- */
/*                         Public functions                                */

#ifndef NO_RECORD_ERR
void err_suppress_errors(void)
{
  DEBUGF("Err: Suppressing errors\n");
  suppress_errors = true;
  recorded_error.errnum = 255;
  recorded_error.errmess[0] = '\0';
}

/* ----------------------------------------------------------------------- */

_Optional CONST _kernel_oserror *err_dump_suppressed(void)
{
  suppress_errors = false;
  if (recorded_error.errmess[0] == '\0')
  {
    DEBUGF("Err: No suppressed error to dump\n");
    return NULL;
  }

  DEBUGF("Err: Dumping suppressed error 0x%x '%s'\n",
         recorded_error.errnum, recorded_error.errmess);

  return &recorded_error;
}
#endif

/* ----------------------------------------------------------------------- */

_Optional CONST _kernel_oserror *err_initialise(const char *const name, bool const new_errs,
  _Optional MessagesFD *const mfd)
{
  DEBUGF("Err: Initialising for task '%s', new errors %s, "
         "messages file descriptor %p\n",
         name, new_errs ? "enabled" : "disabled", (void *)mfd);

  STRCPY_SAFE(err_taskname, name);
  riscos_350 = new_errs;
  desc = mfd;

  return messagetrans_lookup(desc,
                             "ErrButtons",
                             err_buttons,
                             sizeof(err_buttons),
                             NULL, /* not interested in required size */
                             0);
}

/* ----------------------------------------------------------------------- */

#ifdef CBLIB_OBSOLETE
void err_set_taskname(const char *const name, bool const new_errs)
{
  (void)err_initialise(name, new_errs, msgs_get_descriptor());
}
#endif /* CBLIB_OBSOLETE */

/* ----------------------------------------------------------------------- */

void err_check_rep(const _kernel_oserror *const er)
{
  if (!err_was_suppressed(er->errnum, er->errmess))
  {
    err_box_continue(fancy_error(er->errnum, er->errmess, false));
  }
}

/* ----------------------------------------------------------------------- */

void err_check_fatal_rep(const _kernel_oserror *const er)
{
  err_box_die(fancy_error(er->errnum, er->errmess, true));
}

/* ----------------------------------------------------------------------- */

void err_report(int const num, const char *const mess)
{
  if (!err_was_suppressed(num, mess))
  {
    _kernel_oserror erblk;
    erblk.errnum = num;
    STRCPY_SAFE(erblk.errmess, mess);
    wimp_report_error(&erblk, Wimp_ReportError_OK, err_taskname);
  }
}

/* ----------------------------------------------------------------------- */

void err_complain(int const num, const char *const mess)
{
  if (!err_was_suppressed(num, mess))
  {
    err_box_continue(fancy_error(num, mess, false));
  }
}

/* ----------------------------------------------------------------------- */

void err_complain_fatal(int const num, const char *const mess)
{
  err_box_die(fancy_error(num, mess, true));
}
