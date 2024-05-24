/*
 * CBLibrary: Look up text messages by token, with parameter substitution
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
  CJB: 29-Feb-04: Added msgs_globalsub() and new internal function
                  msgs_gen_lookupsub() to allow use of global messages that
                  require parameter substitution.
  CJB: 08-Mar-04: Updated to use the new macro names defined in h.Macros.
  CJB: 07-Nov-04: Created new msgs_lookupsubn() function, which is like
                  msgs_lookupsub() except that it accepts a variable no. of
                  arguments which makes it more economical to call.
  CJB: 29-Nov-04: Fixed bug in msgs_lookupsubn() caused by erroneous assumption
                  about order of function argument evaluation (which is in any
                  case implementation defined).
  CJB: 13-Jan-05: Added msgs_error() and msgs_errorsubn() to give access to
                  facilities provided by the SWI MessageTrans_ErrorLookup.
  CJB: 27-Feb-05: Modified to forceably terminate strings read using SWI
                  MessageTrans_Lookup, which seems to return crudely truncated
                  strings if our buffer is too small (no error either).
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 22-Jun-09: Whitespace changes only.
  CJB: 29-Aug-09: Now uses the ARRAY_SIZE macro where appropriate.
  CJB: 14-Oct-09: Rewritten to use new veneers for the MessageTrans SWIs and
                  allow the client to specify the messages file descriptor to
                  be used. Replaced magic values with named constants. Added
                  assertions and debugging output. Made compilation of
                  msgs_get_descriptor, msgs_global, msgs_lookupsubn,
                  msgs_lookupsub, msgs_globalsub and msgs_errorsubn conditional
                  upon CBLIB_OBSOLETE.
  CJB: 28-Dec-14: generic_vlookup now returns a copy of the message token in
                  case of a failed lookup, instead of an empty string.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 18-Apr-16: Cast pointer parameters to void * to match %p.
  CJB: 29-Aug-22: Use size_t rather than unsigned int for nparam.
*/

/* ISO library headers */
#include <stdarg.h>
#include <string.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "swis.h"

/* CBOSLib headers */
#include "MessTrans.h"

/* Local headers */
#include "Internal/CBMisc.h"
#include "Err.h"
#include "MsgTrans.h"

/* Miscellaneous numeric constants */
enum
{
  MessageBufferSize = 256,
  MaxParameters     = 4
};

static MessagesFD *desc = NULL;

/* ----------------------------------------------------------------------- */
/*                       Function prototypes                               */

#ifdef CBLIB_OBSOLETE
static char *generic_lookup(MessagesFD   *mfd,
                            const char   *token,
                            size_t  nparam,
                            ...);
#endif /* CBLIB_OBSOLETE */

static char *generic_vlookup(MessagesFD   *mfd,
                             const char   *token,
                             size_t  nparam,
                             va_list       ap);

/* ----------------------------------------------------------------------- */
/*                         Public functions                                */

CONST _kernel_oserror *msgs_initialise(MessagesFD *mfd)
{
  /* Set the message file descriptor to be used for future look-ups */
  DEBUGF("MsgTrans: Setting messages file descriptor %p\n", (void *)mfd);
  desc = mfd;
  return NULL; /* success */
}

/* ----------------------------------------------------------------------- */

char *msgs_lookup(const char *token)
{
  /* look in application messages file */
  assert(token != NULL);
  return msgs_lookup_subn(token, 0);
}

/* ----------------------------------------------------------------------- */

char *msgs_lookup_subn(const char *token, size_t nparam, ...)
{
  /* look in application messages file, with substitution of a variable
     number of parameters */
  va_list ap;
  char *m;

  assert(token != NULL);
  assert(nparam <= MaxParameters);

  va_start(ap, nparam); /* make ap point to first unnamed argument */
  m = generic_vlookup(desc, token, nparam, ap);
  va_end(ap);

  return m;
}

/* ----------------------------------------------------------------------- */

CONST _kernel_oserror *msgs_error(int errnum, const char *token)
{
  assert(token != NULL);
  return msgs_error_subn(errnum, token, 0);
}

/* ----------------------------------------------------------------------- */

CONST _kernel_oserror *msgs_error_subn(int errnum, const char *token, size_t nparam, ...)
{
  /* look up error message and create error block, with substitution of a
     variable number of parameters */
  va_list ap;
  CONST _kernel_oserror *e;

  assert(token != NULL);
  assert(nparam <= MaxParameters);

  va_start(ap, nparam); /* make ap point to 1st unnamed arg */
  e = messagetrans_error_vlookup(desc, errnum, token, nparam, ap);
  va_end(ap);

  assert(e != NULL);
  return e;
}

#ifdef CBLIB_OBSOLETE
/* ----------------------------------------------------------------------- */
/*                       Deprecated functions                              */

MessagesFD *msgs_get_descriptor(void)
{
  static MessagesFD descriptor;

  /* Assume that if the client requests a descriptor and none has previously
     been set then the internal descriptor will thereafter be valid - yuck! */
  if (desc == NULL)
  {
    desc = &descriptor;
    DEBUGF("MsgTrans: No descriptor has been set - using internal %p\n",
           (void *)desc);
  }

  return desc;
}

/* ----------------------------------------------------------------------- */

char *msgs_global(const char *token)
{
  /* look in global messages file */
  return generic_lookup(NULL, token, 0);
}

/* ----------------------------------------------------------------------- */

char *msgs_lookupsubn(const char *token, size_t nparam, ...)
{
  /* look in application messages file, with substitution of a variable
     number of parameters */
  va_list ap;
  char *m;

  va_start(ap, nparam); /* make ap point to first unnamed argument */
  m = generic_vlookup(desc, token, nparam, ap);
  va_end(ap);

  return m;
}

/* ----------------------------------------------------------------------- */

char *msgs_lookupsub(const char *token, const char *param0, const char *param1, const char *param2, const char *param3)
{
  /* look in application messages file, with substitution of 4 parameters */
  return generic_lookup(desc, token, MaxParameters,
                        param0, param1, param2, param3);
}

/* ----------------------------------------------------------------------- */

char *msgs_globalsub(const char *token, const char *param0, const char *param1, const char *param2, const char *param3)
{
  /* look in global messages file, with substitution of 4 parameters */
  return generic_lookup(NULL, token, MaxParameters,
                        param0, param1, param2, param3);
}

/* ----------------------------------------------------------------------- */

CONST _kernel_oserror *msgs_errorsubn(int errnum, const char *token, size_t nparam, ...)
{
  /* look up error message and create error block, with substitution of a
     variable number of parameters */
  va_list ap;
  CONST _kernel_oserror *e;

  va_start(ap, nparam); /* make ap point to 1st unnamed arg */
  e = messagetrans_error_vlookup(desc, errnum, token, nparam, ap);
  va_end(ap);

  return e;
}

#endif /* CBLIB_OBSOLETE */

/* ----------------------------------------------------------------------- */
/*                         Private functions                               */

#ifdef CBLIB_OBSOLETE
static char *generic_lookup(MessagesFD *mfd, const char *token,
                            size_t  nparam, ...)
{
  va_list ap;
  char *m;

  va_start(ap, nparam); /* make ap point to first unnamed argument */
  m = generic_vlookup(mfd, token, nparam, ap);
  va_end(ap);

  return m;
}
#endif /* CBLIB_OBSOLETE */

/* ----------------------------------------------------------------------- */

static char *generic_vlookup(MessagesFD *mfd, const char *token,
                             size_t nparam, va_list ap)
{
  /* look in application messages file, with substitution of a variable
     number of parameters */
  CONST _kernel_oserror *e;
  static char message_buffer[MessageBufferSize] = "";

  assert(token != NULL);
  assert(nparam <= MaxParameters);

  DEBUGF("MsgTrans: Looking up token '%s' in file %p with %zu parameters\n",
         token, (void *)mfd, nparam);

  e = messagetrans_vlookup(mfd, /* message file descriptor (or NULL) */
                           token,
                           message_buffer,
                           sizeof(message_buffer),
                           NULL, /* not interested in size of result */
                           nparam, /* number of parameters */
                           ap); /* parameters to be substituted */

  if (e != NULL)
  {
    STRCPY_SAFE(message_buffer, token);
    ON_ERR_RPT(e);
  }

  return message_buffer;
}
