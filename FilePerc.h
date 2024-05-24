/*
 * CBLibrary: Display progress and check for ESCAPE during file operations
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

/* FilePerc.h declares two functions and an enumerated type that allow an
   application to load or save data whilst displaying an hourglass to
   indicate the percentage done and allowing the user to abort by pressing
   ESCAPE.

Dependencies: ANSI C library, Acorn library kernel, Acorn's flex library.
Message tokens: NoMem, Escape, OpenInFail, ReadFail, OpenOutFail, WriteFail and
                (if using FilePercOp_Decomp) BitStream.
History:
  CJB: 05-Nov-04: Added clib-style documentation and dependency information.
  CJB: 06-Feb-06: Switched to using an enumerated type to specify the kind of
                  file operation instead of #define'd constants.
                  Added symbolic definition of flag in bit 31 of file_type.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 17-Oct-06: Added declarations and descriptions of new functions
                  file_perc_load() and file_perc_save(). Marked old function
                  perc_operation() and its flag as deprecated.
  CJB: 27-Oct-06: Minor changes to documentation.
  CJB: 13-Oct-09: Renamed the enumerated type 'perc_op_type' and its values.
                  Added prototype of a new function, file_perc_initialise,
                  which should be called to initialise this module. Added
                  "NoMem" to lists of required message tokens.
  CJB: 26-Jun-10: Made definition of deprecated type and constant names
                  conditional upon definition of CBLIB_OBSOLETE.
  CJB: 11-Dec-20: Deleted redundant uses of the 'extern' keyword.
*/

#ifndef FilePerc_h
#define FilePerc_h

/* Acorn C/C++ library headers */
#include "toolbox.h"
#include "kernel.h"
#include "flex.h"

/* Local headers */
#include "Macros.h"

typedef enum
{
  FilePercOp_Load = 1,/* Load a plain file */
  FilePercOp_Save,    /* Save data as a plain file */
  FilePercOp_Decomp,  /* Load a file in Fednet compressed format */
  FilePercOp_Comp     /* Save data in Fednet compressed format */
}
FilePercOp;

CONST _kernel_oserror *file_perc_initialise(MessagesFD */*mfd*/);
   /*
    * Initialises the FilePerc module. Unless 'mfd' is a null pointer, the
    * specified messages file will be given priority over the global messages
    * file when looking up text required by this module.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

CONST _kernel_oserror *file_perc_load(FilePercOp /*type*/, const char */*file_path*/, flex_ptr /*buffer_anchor*/);
   /*
    * Does a file input operation specified by the 'type' argument (which must
    * be FilePercOp_Load or FilePercOp_Decomp). The contents of the file
    * specified by 'file_path' will be loaded into a new flex block anchored
    * at 'buffer_anchor'.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

CONST _kernel_oserror *file_perc_save(FilePercOp /*type*/, const char */*file_path*/, unsigned int /*file_type*/, flex_ptr /*buffer_anchor*/, unsigned int /*start_offset*/, unsigned int /*end_offset*/);
   /*
    * Does a file output operation specified by the 'type' argument (which must
    * be FilePercOp_Save or FilePercOp_Comp). The contents of flex block
    * 'buffer_anchor' between 'start_offset' (inclusive) and 'end_offset'
    * (exclusive) will be saved in a new file at 'file_path'. The new file will
    * be assigned the RISC OS file type 'file_type'.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

/* Obsolete flag for use with perc_operation() */
#define FILEPERC_SPRITEAREA (1u<<31)

CONST _kernel_oserror *perc_operation(FilePercOp /*type*/, const char * /*file_path*/, unsigned int /*file_type*/, flex_ptr /*buffer_anchor*/);
   /*
    * This function is deprecated - you should use 'file_perc_load' or
    * 'file_perc_save' instead.
    */

#ifdef CBLIB_OBSOLETE
/* Deprecated type and enumeration constant names */
#define perc_op_type       FilePercOp
#define FILEPERC_OP_LOAD   FilePercOp_Load
#define FILEPERC_OP_SAVE   FilePercOp_Save
#define FILEPERC_OP_DECOMP FilePercOp_Decomp
#define FILEPERC_OP_COMP   FilePercOp_Comp
#endif /* CBLIB_OBSOLETE */

#endif /* FilePerc_h */
