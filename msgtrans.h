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

/* MsgTrans.h declares several functions and defines some macros that give
  access to the facilities provided by the RISC OS MessageTrans module.

Dependencies: ANSI C library, Acorn library kernel, Acorn's WIMP library.
Message tokens: None.
History:
  (Original version by Tony Houghton for !FormText.)
  CJB: 29-Feb-04: Added declaration of new function msgs_globalsub() and
                  defined corresponding msgs_global_subX macros.
  CJB: 04-Nov-04: Added dependency information and summary text.
  CJB: 07-Nov-04: Added declaration of new function msgs_lookupsubn() and
                  changed the msgs_lookup_sub{1|2|3} macros to make use of it.
                  Added clib-style documentation.
  CJB: 13-Jan-05: Added declarations of new functions msgs_error() and
                  msgs_errorsubn(). Qualified declaration of msgs_lookupsubn()
                  as 'extern'.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 13-Oct-09: Added prototypes of new functions msgs_initialise,
                  msgs_lookup_subn and msgs_error_subn. Marked many of the
                  existing functions and macros as deprecated.
  CJB: 04-Jul-10: Made inclusion of deprecated macro definitions and function
                  prototypes conditional.
  CJB: 11-Dec-20: Deleted redundant uses of the 'extern' keyword.
  CJB: 29-Aug-22: Use size_t rather than unsigned int for nparam.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
*/

#ifndef MsgTrans_h
#define MsgTrans_h

/* Acorn C/C++ library headers */
#include "toolbox.h"
#include "kernel.h"

/* Local headers */
#include "Macros.h"

#if !defined(USE_OPTIONAL) && !defined(_Optional)
#define _Optional
#endif

_Optional CONST _kernel_oserror *msgs_initialise(MessagesFD */*mfd*/);
   /*
    * Sets the descriptor for a messages file which will be be given priority
    * over the global messages file when looking up messages, by all functions
    * except msgs_global and msgs_global_subn. The descriptor must be
    * persistent and should already have been initialised by calling
    * toolbox_initialise() or SWI MessageTrans_OpenFile.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

char *msgs_lookup(const char */*token*/);
   /*
    * Looks up the message associated with the specified token, first in any
    * messages file associated with this module by an earlier call to
    * msgs_set_descriptor and then in the global messages file. No parameter
    * substitution is available. An internal buffer is used to hold the result,
    * which will be overwritten by the next call to this function or
    * msgs_lookup_subn. Messages longer than 255 characters will be truncated.
    * An error will be reported and an empty string returned if the token could
    * not be found.
    * Returns: a pointer to the message string, or an empty string if the token
    *          was not found.
    */

char *msgs_lookup_subn(const char */*token*/, size_t /*nparam*/, ...);
   /*
    * Equivalent to msgs_lookup except that parameter substitution is performed
    * by replacing occurrences of %0 to %3 in the message with the specified
    * strings. The number of parameters to be substituted into the message is
    * given by the 'nparam' argument, which must be followed by the expected
    * number of string pointers (any of which may be null to suppress
    * substitution).
    * Returns: a pointer to the message string, or an empty string if the token
    *          was not found.
    */

CONST _kernel_oserror *msgs_error(int /*errnum*/, const char */*token*/);
   /*
    * Looks up the message associated with the specified token, first in any
    * messages file associated with this module by an earlier call to
    * msgs_set_descriptor and then in the global messages file. No parameter
    * substitution is available. The result will be held in one of several
    * internal buffers, which are continuously recycled. Returns a different
    * error if the token could not be found (instead of reporting an error).
    * Returns: a pointer to an error block, which will contain a different
    *          error number and message if the token was not found.
    */

CONST _kernel_oserror *msgs_error_subn(int          /*errnum*/,
                                       const char  */*token*/,
                                       size_t       /*nparam*/,
                                       ...);
   /*
    * Equivalent to msgs_error except that parameter substitution is performed
    * by replacing occurrences of %0 to %3 in the message with the specified
    * strings. The number of parameters to be substituted into the message is
    * given by the 'nparam' argument, which must be followed by the expected
    * number of string pointers (any of which may be null to suppress
    * substitution).
    * Returns: a pointer to an error block, which will contain a different
    *          error number and message if the token was not found.
    */

#ifdef CBLIB_OBSOLETE

/* The following functions and macros are deprecated and should not be used in
   new or updated programs. */
MessagesFD *msgs_get_descriptor(void);

char *msgs_global(const char */*token*/);

char *msgs_lookupsubn(const char */*token*/, size_t /*nparam*/, ...);

char *msgs_lookupsub(const char */*token*/,
                     const char */*param0*/,
                     const char */*param1*/,
                     const char */*param2*/,
                     const char */*param3*/);

char *msgs_globalsub(const char */*token*/,
                     const char */*param0*/,
                     const char */*param1*/,
                     const char */*param2*/,
                     const char */*param3*/);

_Optional CONST _kernel_oserror *msgs_errorsubn(int           /*errnum*/,
                                      const char   */*token*/,
                                      size_t        /*nparam*/,
                                      ...);

#define msgs_global_sub1(token, param0) \
  msgs_globalsub(token, param0, NULL, NULL, NULL)

#define msgs_global_sub2(token, param0, param1) \
  msgs_globalsub(token, param0, param1, NULL, NULL)

#define msgs_global_sub3(token, param0, param1, param2) \
  msgs_globalsub(token, param0, param1, param2, NULL)

#define msgs_global_sub4(token, param0, param1, param2, param3) \
  msgs_globalsub(token, param0, param1, param2, param3)

#define msgs_lookup_sub1(token, param0) \
  msgs_lookupsub(token, 1, param0, NULL, NULL, NULL)

#define msgs_lookup_sub2(token, param0, param1) \
  msgs_lookupsub(token, 2, param0, param1, NULL, NULL)

#define msgs_lookup_sub3(token, param0, param1, param2) \
  msgs_lookupsub(token, 3, param0, param1, param2, NULL)

#define msgs_lookup_sub4(token, param0, param1, param2, param3) \
  msgs_lookupsub(token, 4, param0, param1, param2, param3)

#endif /* CBLIB_OBSOLETE */

#endif
