/*
 * CBLibrary: Deprecated macro definitions
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

/* Deprecated.h is included by Macros.h to define deprecated macros. It may
   eventually be deleted.

Dependencies: None
Message tokens: None
History:
  CJB: 25-Feb-05: Created this header.
  CJB: 22-Jun-09: Changes to whitespace only.
  CJB: 01-Oct-09: Added macro definitions of deprecated file type names.
  CJB: 19-Oct-09: Moved the various WARN_GLOB and RPT_GLOB_ERR macro definitions
                  here because they rely upon a deprecated function.
*/

/* Clone a string (superseded by strdup function) */
#define CLONE_STR(lvalue, string) do { \
  lvalue = malloc(strlen(string)+1); \
  if (lvalue != NULL) \
    strcpy(lvalue, string); \
} while (0)

/* Synonyms to allow older programs to compile (deprecated as obfuscatory) */
#define RE         ON_ERR_RPT
#define THROW      ON_ERR_RTN_E
#define E_RETV     ON_ERR_RPT_RTN_V
#define E_RET      ON_ERR_RPT_RTN
#define M          WARN
#define M_RET      WARN_RTN
#define M_RETV     WARN_RTN_V
#define MG         WARN_GLOB
#define MG_RET     WARN_GLOB_RTN
#define MG_RETV    WARN_GLOB_RTN_V
#define R          RPT_ERR
#define R_RET      RPT_ERR_RTN
#define R_RETV     RPT_ERR_RTN_V
#define RG         RPT_GLOB_ERR
#define RG_RET     RPT_GLOB_ERR_RTN
#define RG_RETV    RPT_GLOB_ERR_RTN_V
#define FREE       FREE_SAFE
#define FLAG_SET   TEST_BITS
#define CLEAR_FLAG CLEAR_BITS
#define SET_FLAG   SET_BITS

/* Write an error message to an OS error block
   (deprecated as obfuscatory - use STRCPY_SAFE) */
#define WRITE_ERR(b,t) STRCPY_SAFE(b.errmess, msgs_lookup(t))
#define WRITE_GERR(b,t) STRCPY_SAFE(b.errmess, msgs_global(t))
#define WRITE_ERR_SUB1(b,t,s) STRCPY_SAFE(b.errmess, msgs_lookup_sub1(t,s))
#define WRITE_GERR_SUB1(b,t,s) STRCPY_SAFE(b.errmess, msgs_global_sub1(t, s))

/* Deprecated file type names */
#define FILETYPE_SQUASH FileType_Squash
#ifndef FILETYPE_SPRITE
#define FILETYPE_SPRITE FileType_Sprite
#endif /* FILETYPE_SPRITE */
#define FILETYPE_DATA   FileType_Data
#define FILETYPE_TEXT   FileType_Text
#define FILETYPE_CSV    FileType_CSV
#define FILETYPE_OBEY   FileType_Obey
#define FILETYPE_DIR    FileType_Directory
#define FILETYPE_APP    FileType_Application
#define FILETYPE_NONE   FileType_None

/* Macro veneers for a deprecated message lookup function */
#define WARN_GLOB(token) \
  err_report (DUMMY_ERRNO, msgs_global(token))

#define WARN_GLOB_RTN(token) do { \
  err_report (DUMMY_ERRNO, msgs_global(token)); \
  return; \
} while (0)

#define WARN_GLOB_RTN_V(token, value) do { \
  err_report (DUMMY_ERRNO, msgs_global(token)); \
  return value; \
} while (0)

#define RPT_GLOB_ERR(token) \
  err_complain (DUMMY_ERRNO, msgs_global(token))

#define RPT_GLOB_ERR_RTN(token) do { \
  err_complain (DUMMY_ERRNO, msgs_global(token)); \
  return; \
} while (0)

#define RPT_GLOB_ERR_RTN_V(token, value) do { \
  err_complain (DUMMY_ERRNO, msgs_global(token)); \
  return value; \
} while (0)
