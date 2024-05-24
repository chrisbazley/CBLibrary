;
; CBLibrary: OS ticker event callback routine
; Copyright (C) 2003  Chris Bazley
;
; This library is free software; you can redistribute it and/or
; modify it under the terms of the GNU Lesser General Public
; License as published by the Free Software Foundation; either
; version 2.1 of the License, or (at your option) any later version.
;
; This library is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
; Lesser General Public License for more details.
;
; You should have received a copy of the GNU Lesser General Public
; License along with this library; if not, write to the Free Software
; Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
;

; History:
; 11.09.03 CJB Added BOOL_8_BIT variable to control width of type 'bool'.
; 28.05.16 CJB Renamed from Timer.s and deleted the SWI veneer functions.

  EXPORT timer_set_flag

  AREA |C$$code|, CODE, READONLY

timer_set_flag
  ; Called in SVC mode with interrupts disabled
  ; Must preserve all registers and return using MOV PC,R14
  ; R12 = pointer to timeup flag

  STR R14,[R13,#-4]! ; push return address

  ; This should work whether typedef 'bool' aliases an 8- or 32-bit type.
  MOV R14,#1
  STRB R14,[R12] ; set flag byte

  LDR PC,[R13],#4 ; pull return address

  END
