/*
 * CBLibrary test: Macro and test suite definitions
 * Copyright (C) 2018 Christopher Bazley
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

#ifndef Tests_h
#define Tests_h

#ifdef FORTIFY
#include "fortify.h"
#endif

#ifdef USE_CBDEBUG

#include "Debug.h"
#include "PseudoKern.h"
#include "PseudoWimp.h"
#include "PseudoTbox.h"
#include "PseudoEvnt.h"
#include "PseudoFlex.h"
#include "PseudoIO.h"

#else /* USE_CBDEBUG */

#include <assert.h>

#define DEBUG_SET_OUTPUT(output_mode, log_name)

#endif /* USE_CBDEBUG */

void DecodeLExe_tests(void);
void DirIter_tests(void);
void IntVector_tests(void);
void Macros_tests(void);
void MakePath_tests(void);
void PathTail_tests(void);
void Timer_tests(void);
void UserData_tests(void);

#endif /* Tests_h */
