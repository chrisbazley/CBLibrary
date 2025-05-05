/*
 * CBLibrary: Useful macro definitions
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

/* Macros.h declares macros for bitwise operations, detection & reporting of
   errors (veneers to Err.h or MsgTrans.h functions, or combinations thereof),
   common RISC OS file types and other miscellany.

Dependencies: ANSI C library, Acorn library kernel, Acorn's WIMP library
              (varies according to which macros are used).
Message tokens: FatErr, NewErr, OldErr, ErrButtons
                (if using ON_ERR_..., RPT_... or WARN_.. macros).
History:
  CJB: 20-Feb-04: Now forceably terminates strings that may have been truncated
                  by strncpy().
  CJB: 29-Feb-04: Added STRCPY_SAFE_SUB1 macro to give simple interface to new
                  msgtrans function msgs_globalsub().
  CJB: 07-Mar-04: Other headers are no longer #included from this one, because
                  which functions are needed depends on which macros are used.
                  Added new string-copying macros STRCPY_SAFE and CLONE_STR.
                  Amended the macros ABSDIFF and FLAG_SET to cope with
                  non-parenthesised expressions as input arguments.
                  Added verbose alternatives to many macro names that were
                  felt to be concise to the point of obfuscation.
  CJB: 08-Mar-04: Added new macro ON_ERR_RPT, which supercedes the RE macro
                  defined in err.h.
  CJB: 23-May-04: Macros that were formerly defined as compound statements are
                  now iteration statements which execute once only. This allows
                  usage consistent with an expression statement (note that
                  all macro invocations must now include a trailing ';').
  CJB: 05-Nov-04: Changed name of the constant #defined to prevent this header
                  being included multiple times to match that of the file.
                  Grouped all deprecated definitions at the end of this file.
                  Added RE as a synonym for ON_ERR_RPT (no longer defined in
                  Err.h).
  CJB: 06-Nov-04: Added dependency information and summary text.
  CJB: 25-Feb-05: Moved deprecated macro definitions out to separate file.
  CJB: 20-Apr-05: Added SYSTEM_BEEP macro definition.
  CJB: 28-Apr-05: Changed definition of TEST_BITS slightly to return 'true'
                  instead of non-zero if any bits are set in both operands.
  CJB: 29-Apr-05: Substituted '\a' for 7 in SYSTEM_BEEP. Fixed TEST_BITS. Added
                  new LOWEST and HIGHEST macros.
  CJB: 15-May-05: Changed SYSTEM_BEEP to use putchar instead of _kernel_oswrch.
  CJB: 03-Jul-05: Optimised WORD_ALIGN to use bitwise AND instead of shifts.
  CJB: 15-Jul-05: Added ARRAY_SIZE macro definition.
  CJB: 01-Feb-06: Added definition of FILETYPE_NONE for untyped files (used by
                  Loader component and in DataOpen message).
  CJB: 07-Jun-06: Added definition of FILETYPE_OBEY.
  CJB: 13-Oct-06: Modified error detection macros to use 'const' _kernel_oserror
                  pointers.
  CJB: 22-Oct-06: Added definition of FILETYPE_CSV.
  CJB: 23-Dec-06: Made definition of FILETYPE_SPRITE conditional (also defined
                  in h.SprFormats).
  CJB: 10-Jan-07: Added definition of mathematical constant PI.
  CJB: 11-Mar-07: Added SIGNED_R_SHIFT and SIGNED_L_SHIFT macros because C
                  doesn't usually allow shifts by negative values.
  CJB: 17-Mar-07: Fixed SIGNED_R_SHIFT and SIGNED_L_SHIFT macros.
  CJB: 14-Apr-07: Added definition of MERGE_ERR macro and additional comments.
  CJB: 14-Jun-09: Fixed a serious flaw in the ON_ERR_RPT_RTN_V macro, which was
                  evaluating 'error_source' twice unless it evaluated as NULL
                  the first time (typically calls the same function twice).
  CJB: 22-Jun-09: Changes to whitespace only.
  CJB: 30-Sep-09: Parenthesised the arguments in the definitions of macros
                  LOWEST and HIGHEST to ensure correct order of evaluation.
                  Redefined DUMMY_ERRNO and file types as enumerated constants
                  rather than macro values. Added a new constant FileType_Null
                  for use in place of -1 (commonly used to terminate lists
                  of file types).
  CJB: 19-Oct-09: Banished the various WARN_GLOB and RPT_GLOB_ERR macro
                  definitions because they rely upon a deprecated function.
  CJB: 26-Jun-10: Made definition of deprecated macros conditional upon
                  definition of CBLIB_OBSOLETE. Parenthesised the arguments in
                  the definitions of various error macros and ARRAY_SIZE.
  CJB: 12-Jan-11: Made const-ness of OS errors conditional upon CBLIB_OBSOLETE.
  CJB: 20-Dec-11: Amended to allow pointers to 'const' _kernel_oserror as
                  macro parameters irrespective of CBLIB_OBSOLETE, if consumed.
  CJB: 08-Apr-12: New macro to substitute an empty string for a null pointer.
                  Defined filing system object types as enumerated constants.
  CJB: 18-Apr-15: New macro to convert numeric constants to string literals.
  CJB: 09-Apr-16: Added extra brackets to the definition of WORD_ALIGN to
                  avoid GNU C compiler warnings.
                  Defined a new macro CHECK_PRINTF, for use when declaring
                  printf-like functions. It tells the GNU C compiler to check
                  the argument values passed to such functions.
  CJB: 31-Oct-18: Moved definitions of file types and object types to CBOSLib.
                  Modified the definitions of MERGE_ERR, RPT_ERR_RTN_V,
                  WARN_RTN_V, STRCPY_SAFE, FREE_SAFE and SWAP to ensure that
                  macro arguments are bracketed.
                  Modified the definitions of ABSDIFF and PI to ensure that the
                  expanded expression is bracketed.
                  Made the definitions of STRINGIFY and CHECK_PRINTF conditional
                  because they are also defined by CBDebugLib's "Debug.h".
  CJB: 21-Sep-19: Added the definition of CONTAINER_OF.
  CJB: 23-May-22: Redefined the ON_ERR_RPT macro as a simple call to err_check()
                  that discards its result, since err_check() is now defined
                  inline.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
*/

#ifndef Macros_h
#define Macros_h

/* I believe that using error number 0 can have unpleasant side-effects */
enum
{
  DUMMY_ERRNO = 255
};

/* OS error type has been made stricter */
#ifdef CBLIB_OBSOLETE
#define CONST
#else /* CBLIB_OBSOLETE */
#define CONST const
#endif /* CBLIB_OBSOLETE */

/* --- Detecting and/or reporting errors  --- */

/* Check for an internal error; if detected then respond in one of a number of
  generic ways. For more complex situations you will have to use if (E(...)) */

/* Check whether an error pointer is NULL; if not then report the error to
 * the user.
 */
#define ON_ERR_RPT(error_source) (void)err_check(error_source)

/* Check whether an error pointer is NULL; if not then return from the current
 * function with the error. It is not reported, which is useful for library
 * functions and module code.
 */
#define ON_ERR_RTN_E(error_source) do { \
  _Optional CONST _kernel_oserror *error = (error_source); \
  if (error != NULL) \
    return error; \
} while (0)

/* Merge two error pointers by overwriting the first with the second, if the
 * first is NULL (i.e. no error occurred previously). This is useful when it
 * is desirable for execution to continue without forgetting the earliest error
 * to occur.
 */
#define MERGE_ERR(current_error, error_source) do { \
  _Optional CONST _kernel_oserror *error = (error_source); \
  if ((current_error) == NULL) \
    (current_error) = error; \
} while (0)

/* Check whether an error pointer is NULL; if not then report the error to
 * the user and return the specified value from the current function.
 */
#define ON_ERR_RPT_RTN_V(error_source, return_value) do { \
  _Optional const _kernel_oserror *error = (error_source); \
  if (error != NULL) { \
    err_check_rep (&*error); \
    return (return_value); \
  } \
} while (0)

/* Check whether an error pointer is NULL; if not then report the error to
 * the user and return from the current function.
 */
#define ON_ERR_RPT_RTN(error_source) do { \
  _Optional const _kernel_oserror *error = (error_source); \
  if (error != NULL) { \
    err_check_rep (&*error); \
    return; \
  } \
} while (0)

/* Report an error to the user using a message looked up from the specified
 * token, and offer to quit the application.
 */
#define RPT_ERR(token) \
  err_complain (DUMMY_ERRNO, msgs_lookup(token))

/* Report an error to the user using a message looked up from the specified
 * token, and offer to quit the application. If continuing execution then
 * return from the current function.
 */
#define RPT_ERR_RTN(token) do { \
  err_complain (DUMMY_ERRNO, msgs_lookup(token)); \
  return; \
} while (0)

/* Report an error to the user using a message looked up from the specified
 * token, and offer to quit the application. If continuing execution then
 * return the specified value from the current function.
 */
#define RPT_ERR_RTN_V(token, value) do { \
  err_complain (DUMMY_ERRNO, msgs_lookup(token)); \
  return (value); \
} while (0)

/* Report an error to the user using a message looked up from the specified
 * token.
 */
#define WARN(token) \
  err_report (DUMMY_ERRNO, msgs_lookup(token))

/* Report an error to the user using a message looked up from the specified
 * token (don't offer to quit) and then return from the current function.
 */
#define WARN_RTN(token) do { \
  err_report (DUMMY_ERRNO, msgs_lookup(token)); \
  return; \
} while (0)

/* Report an error to the user using a message looked up from the specified
 * token (don't offer to quit) and then return the specified value from the
 * current function.
 */
#define WARN_RTN_V(token, value) do { \
  err_report (DUMMY_ERRNO, msgs_lookup(token)); \
  return (value); \
} while (0)


/* --- String copying --- */

/* Copy a string into a character array of known size, truncating it to fit if
 * necessary. Unlike strncpy(), this macro ensures that the copied string is NUL
 * terminated if it has to be truncated.
 */
#define STRCPY_SAFE(string_1, string_2) do { \
  strncpy((string_1), (string_2), sizeof(string_1) - 1); \
  (string_1)[sizeof(string_1) - 1]='\0'; \
} while (0)


/* --- Miscellaneous useful macros --- */

/* Write a VDU bell character to the standard output stream. */
#define SYSTEM_BEEP() putchar('\a')

/* Free a malloc block and set the pointer to NULL so that subsequent attempts
 * to free it fail harmlessly.
 */
#define FREE_SAFE(memptr) do { \
  free(memptr); \
  (memptr) = NULL; \
} while (0)

/* Return the nearest word aligned value greater than or equal to a given
 * expression (useful for sprite widths, which must include right hand wastage).
 */
#define WORD_ALIGN(value) (((value) + 3) & ~3)

/* Suppress compiler warnings about an unused function argument. */
#define NOT_USED(x) ((void)(x))

/* Return a value scaled by a given percentage. */
#define SCALE(value, perc) (((value) * (perc)) / 100)

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

/* Swap the contents of two l-values. */
#define SWAP(a, b) do { \
  int temp; \
  temp = (a); \
  (a) = (b); \
  (b) = temp; \
} while (0)

#define LOWEST(a, b) ((a) < (b) ? (a) : (b))
#define HIGHEST(a, b) ((a) > (b) ? (a) : (b))

/* Assign to an l-value the unsigned difference between two expressions.
   This compiles to very efficient in-line ARM code, unlike abs(x - y). */
#define ABSDIFF(lvalue, x, y) \
  lvalue = (((x) > (y)) ? (x) - (y) : (y) - (x))

#define PI (3.1415926535897896)

/* Convert a null pointer into an empty string. */
#define STRING_OR_NULL(s) ((s) == NULL ? "" : &*(s))

/* --- Bitwise manipulation (e.g. flags) --- */

/* Return true if one or more of the specified bits are set. */
#define TEST_BITS(value, bits) (((value) & (bits)) != 0)

/* Clear certain bits of an l-value. */
#define CLEAR_BITS(lvalue, bits) lvalue &= ~(bits)

/* Set certain bits of an l-value. */
#define SET_BITS(lvalue, bits) lvalue |= (bits)

/* Return a value shifted right by a given no. of binary places,
 * or left-shifted if the second argument is negative.
 */
#define SIGNED_R_SHIFT(value, shift) \
  ((shift) >= 0 ? (value) >> (shift) : (value) << -(shift))

/* Return a value shifted left by a given no. of binary places,
 * or right-shifted if the second argument is negative.
 */
#define SIGNED_L_SHIFT(value, shift) \
  ((shift) >= 0 ? (value) << (shift) : (value) >> -(shift))

/* Convert a built-in  (e.g. __LINE__) to a string literal.
 */
#ifndef STRINGIFY
#define STRINGIFY2(n) #n
#define STRINGIFY(n) STRINGIFY2(n)
#endif /* STRINGIFY */

/* Check the arguments passed to a printf-like function.
 * string_index is the index of the format string argument (starting from 1).
 * arg_index is the index of the substitution arguments to be checked.
 * Set arg_index to 0 if the substitution arguments cannot be checked at
 * compile-time (e.g. because they are supplied as a va_list).
 */
#ifndef CHECK_PRINTF
#ifdef __GNUC__
#define CHECK_PRINTF(string_index, arg_index) \
  __attribute__((format(printf, (string_index), (arg_index))))
#else
#define CHECK_PRINTF(string_index, arg_index)
#endif
#endif /* CHECK_PRINTF */

#define CONTAINER_OF(addr, type, member) \
  ((type *)(((char *)(addr)) - offsetof(type, member)))

/* --- RISC OS object types --- */
#include "OSFile.h"

/* --- Common RISC OS file types --- */
#include "FileTypes.h"

#ifdef CBLIB_OBSOLETE
/* Consider removing this at some point: */
#include "Deprecated.h"
#endif /* CBLIB_OBSOLETE */

#endif /* Macros_h */
