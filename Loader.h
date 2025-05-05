/*
 * CBLibrary: Begin the receiver's half of the data transfer message protocol
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

/* Loader.h declares several types and functions that allow a client
   program to register and deregister interest in receiving data
   of given type(s) when file icons are dragged to a particular location
   or double-clicked in a directory display. Client-supplied handler
   functions are called when data is received. This module is DEPRECATED in
   favour of Loader2 (less powerful but more flexible).

Dependencies: ANSI C library, Acorn library kernel, Acorn's WIMP, toolbox,
              event & flex libraries.
Message tokens: NoMem, LoadFail, FileNotPerm, NoSuchFileType, NoSuchDropZone,
                WrongZone.
History:
  CJB: 31-Oct-04: Added warning about deprecation of function
                  loader_canonicalise().
  CJB: 04-Nov-04: Changed name of the constant #defined to prevent this header
                  being included multiple times to match that of the file.
                  Added clib-style documentation and dependency information.
  CJB: 31-Dec-04: Modified definition of LoaderFileHandler to accept constant
                  strings.
  CJB: 25-Jan-05: Added declaration of loader_deregister_listeners_for_object()
                  and type/symbol definitions to support new pre-filter
                  facility of loader_register_listener(). Qualified the strings
                  passed to a LoaderFileHandler or LoaderFinishedHandler as
                  'const'.
  CJB: 05-Mar-05: Updated documentation on loader_register_listener and
                  loader_deregister_listener. Removed BadListReg from list of
                  required messages.
  CJB: 01-Feb-06: Completely changed the arguments of a LoaderPreFilter; the
                  listener's client handle is now passed, as is an indication of
                  whether the drag destination is within the listener's drop
                  zone. Whether the data source is 'safe' is now implicit in
                  whether a file path is passed.
                  Made recognised return values into an enumerated type.
  CJB: 03-Sep-06: We no longer require message 'TransUXreq'.
  CJB: 04-Sep-06: Added 'your ref' from initial DataSave/Load/Open message as
                  an argument to LoaderFinishedHandler.
  CJB: 11-Sep-06: Added the drop coordinates from initial DataSave/Load message
                  as an argument to LoaderFinishedHandler.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 25-Oct-06: Function type LoaderFileHandler is now a synonym for
                  LoaderFileHandler2. Marked function loader_buffer_file()
                  as obsolete. Updated documentation on function
                  loader_register_listener(). The ID of the DataSave (or
                  equivalent) message originally received is no longer passed to
                  a LoaderFinishedHandler; if you require this then you should
                  register a DataSave message handler in which to record the ID
                  and then call loader2_receive_data() directly.
  CJB: 27-Oct-06: Minor changes to documentation.
  CJB: 28-Oct-06: Qualified the pointer to a list of ComponentId's passed to
                  loader_register_listener() as 'const'.
  CJB: 06-Oct-09: Added "NoMem" to list of required message tokens.
  CJB: 26-Jun-10: Made compilation of this file conditional upon definition of
                  CBLIB_OBSOLETE.
  CJB: 11-Dec-14: Deleted redundant brackets from function type definitions.
  CJB: 11-Dec-20: Deleted redundant uses of the 'extern' keyword.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
*/

#ifndef Loader_h
#define Loader_h

#ifdef CBLIB_OBSOLETE

/* ISO library headers */
#include <stdbool.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "flex.h"
#include "toolbox.h"

/* Local headers */
#include "Loader2.h"
#include "Macros.h"

#if !defined(USE_OPTIONAL) && !defined(_Optional)
#define _Optional
#endif

/* ---------------- Client-supplied participation routines ------------------ */

typedef enum {
  LOADER_PREFILTER_BADTYPE=0, /* ignore data transfer request (bad file type) */
  LOADER_PREFILTER_CLAIM, /* claim data transfer request for this listener */
  LOADER_PREFILTER_REJECT, /* reject data transfer request */
  LOADER_PREFILTER_IGNORE /* ignore data transfer request (outside dropzone) */
} loader_pf_result;
/* If a LoaderPreFilter returns LOADER_PREFILTER_BADTYPE or
   LOADER_PREFILTER_IGNORE then the data transfer request is offered to any
   other registered listeners. Standard error messages ensue if none found. */

typedef loader_pf_result LoaderPreFilter (const char * /*file_path*/,
                                          int          /*file_type*/,
                                          bool         /*inside_zone*/,
                                          void       * /*client_handle*/);

typedef Loader2FileHandler LoaderFileHandler;

typedef void LoaderFinishedHandler (int          /*drop_x*/,
                                    int          /*drop_y*/,
                                    const char * /*title*/,
                                    bool         /*data_saved*/,
                                    flex_ptr     /*buffer*/,
                                    int          /*file_type*/,
                                    void       * /*client_handle*/);
/*
  If 'data_saved' is set then 'title' will be the full CANONICALISED
  path of the file from which the data was loaded. The case of 'title'
  is not defined.
*/

/* --------------------------- Library functions ---------------------------- */

_Optional CONST _kernel_oserror *loader_initialise(unsigned int /*flags*/);
   /*
    * Initialises the Loader component and sets up WIMP message handlers for
    * DataLoad, DataSave and (unless 'flags' bit 0 is set) DataOpen. These
    * handlers are used to intercept data transfer requests on behalf of the
    * client application. The event library's mask is manipulated to allow
    * UserMessage, UserMessageRecorded and UserMessageAcknowledge events.
    * Certain user warnings may be suppressed by setting bits 1-3 of 'flags'
    * (see below).
    * YOU MUST INITIALISE 'LOADER' BEFORE 'LOADER2' OR 'ENTITY' OR ELSE IT
    *     WILL INTERCEPT MESSAGES REQUIRED BY THOSE LIBRARY COMPONENTS!
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

/* Client-supplied flags for loader_initialise() */
#define LOADER_IGNOREBCASTS  1  /* Ignore all DataOpen messages */
#define LOADER_QUIET_BADTYPE 2  /* Suppress various errors... */
#define LOADER_QUIET_BADDROP 4
#define LOADER_QUIET_NOTPERM 8

_Optional CONST _kernel_oserror *loader_finalise(void);
   /*
    * Removes the Loader component's message handlers and causes all registered
    * listeners to be forgotten (thus releasing any memory used). Any data
    * transfers still in progress will be terminated abruptly. Note that this
    * function is not normally included in pre-built library distributions.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

_Optional CONST _kernel_oserror *loader_register_listener(
                              unsigned int            /*flags*/,
                              int                     /*file_type*/,
                              ObjectId                /*drop_object*/,
                              const ComponentId     * /*drop_gadgets*/,
                              LoaderFileHandler     * /*loader_method*/,
                              LoaderFinishedHandler * /*finished_method*/,
                              void                  * /*client_handle*/);
   /*
    * Registers an interest in receiving data of type 'file_type' (may also be
    * FILETYPE_ALL, or a LoaderPreFilter function pointer if flags bit 3 set).
    * A G.U.I. location at which to listen for drag termination is specified
    * in terms of a Toolbox object Id 'drop_object' (NULL_ObjectId means any
    * object) and an array of component Ids 'drop_gadgets' (NULL means any
    * component) terminated by NULL_ComponentId. If specified then the
    * 'drop_gadgets' array must exist for the lifetime of the listener.
    * The passed 'flags' value carries additional information - see below.
    * If a LoaderFileHandler is supplied then this will be invoked to load
    * any files received. This is deprecated because it prevents RAM transfer
    * between tasks. The LoaderFinishedHandler function will be called when an
    * incoming data transfer has successfully concluded.
    * This function may cause abnormal program termination if the supplied key
    * (type, object, gadgets) matches an existing listener.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

/* Client-supplied flags for loader_register_listener() */
#define LISTENER_CLAIM       1 /* Should we also claim double-clicks */
#define LISTENER_FILEONLY    2 /* Only load permanent files */
#define LISTENER_SPRITEAREAS 4 /* Load sprite files as sprite areas */
#define LISTENER_FILTER      8 /* Use filter function instead of file type */
#define FILETYPE_ALL         -1

_Optional CONST _kernel_oserror *loader_deregister_listener(
                                          int           /*file_type*/,
                                          ObjectId      /*drop_object*/,
                                          const ComponentId * /*drop_gadgets*/);
   /*
    * Removes a previously registered listener. The three parameters must
    * exactly match those passed to the registration function (together, they
    * serve to uniquely identify a given listener). Any data transfers
    * instigated by the specified listener will be terminated abruptly.
    * May cause abnormal program termination if no such listener exists.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */


_Optional CONST _kernel_oserror *loader_deregister_listeners_for_object(
                                          ObjectId /*drop_object*/);
   /*
    * Removes any listeners that have been registered for the specified object.
    * It is not an error if none exist.
    * Returns: a pointer to an OS error block, or else NULL for success.
    */

_Optional CONST _kernel_oserror *loader_buffer_file(const char * /*file_path*/,
                                                    flex_ptr     /*buffer*/,
                                                    bool         /*sprite_file*/);
   /*
    * This function is deprecated - you should use 'loader2_buffer_file'
    * instead.
    */

_Optional CONST _kernel_oserror *loader_canonicalise(char **      /*buffer*/,
                                                     _Optional const char * /*path_var*/,
                                                     _Optional const char * /*path_string*/,
                                                     const char * /*file_path*/);
   /*
    * This function is deprecated - you should use 'canonicalise' instead.
    */

#else /* CBLIB_OBSOLETE */
#error Header file Loader.h is deprecated
#endif /* CBLIB_OBSOLETE */

#endif
