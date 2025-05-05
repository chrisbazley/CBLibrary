/*
 * CBLibrary: 256 colour selection dialogue box
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
  CJB: 07-Mar-04: Updated to use the new macro names defined in h.Macros.
  CJB: 24-Mar-04: Reduced overall size of palette from 576 to 512 OS units.
  CJB: 13-Jun-04: Because all macro definitions are now expression statements,
                  have changed those invocations which omitted a trailing ';'.
  CJB: 13-Jan-05: Changed to use new msgs_error() function, hence no
                  longer requires external error block 'shared_err_block'.
  CJB: 15-Jan-05: Changed to use new DEBUG macro rather than ugly in-line code.
  CJB: 04-Feb-05: Fixed longstanding memory leak - _Pal256_deleted() was
                  failing to free memory claimed for the Pal256Data block.
                  Removed unused 'caretstore' element from Pal256Data struct.
  CJB: 05-Jul-05: Changed type of Pal256_colour_brightness argument to unsigned
                  long.
  CJB: 06-Aug-05: Amended _Pal256_deleted() to use event library functions that
                  automatically deregister all handlers for a given object.
                  This function is now called from Pal256_initialise() on error.
  CJB: 06-Oct-06: Updated to support keyboard control of the dialogue box,
                  instant selection (by double click), and drags. Moved the
                  code for decoding a mouse click from _Pal256_clickhandler()
                  to a new function _Pal256_decode_mouse_pos(). Moved the code
                  for raising a Pal256_ColourSelected event from
                  _Pal256_buttonhandler() to a new function _Pal256_confirmed.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 18-Oct-06: A redesign of the dbox to include a number range gadget has
                  necessitated a handler for NumberRange_ValueChanged events.
                  The alternative key-handling code has been made conditional
                  upon pre-processor symbol KEY_CONTROL, since the input focus
                  will be in the number range gadget.
  CJB: 22-Jun-09: Use variable name rather than type with 'sizeof' operator,
                  tweaked spacing and asserted function arguments not NULL.
  CJB: 29-Aug-09: Employed branch-to-exit to reduce the code size of several
                  functions.
  CJB: 09-Sep-09: Stop using many reserved identifiers which start with an
                  underscore followed by a capital letter (_Pal_256...).
  CJB: 14-Oct-09: Renamed some event handlers. Updated to use new SWI veneers
                  colourtrans_set_gcol, os_plot and os_read_vdu_variables.
                  Replaced 'magic' values with named constants and macro values
                  with 'enum'. Use 'uint8_t' instead of 'char' for small ints
                  in structs and 'unsigned int' for speed elsewhere. Removed
                  dependency on MsgTrans and Err modules by storing pointers to
                  a messages file descriptor and an error-reporting callback
                  upon initialisation. Moved Pal256_colour_brightness to a new
                  module and deprecated the old entrypoint. Callers of
                  decode_pointer_pos now pass a WimpGetWindowStateBlock pointer
                  to avoid multiple calls to wimp_get_window_state when
                  handling start of drag. Rewrote the redraw handler to use
                  unsigned ints, which means it now draws upward instead of
                  downward. This function and update_window are now based
                  around 'for' loops.
  CJB: 16-Oct-09: Arguments to colourtrans_set_gcol were passed in the wrong
                  order when drawing the border of a colour.
  CJB: 23-Dec-14: Apply Fortify to Toolbox, Event & Wimp library function calls.
  CJB: 02-Jan-15: Got rid of goto statements.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 01-Nov-18: Replaced DEBUG macro usage with DEBUGF.
  CJB: 29-Aug-20: Deleted redundant static function pre-declarations.
  CJB: 28-May-22: Allow initialisation with a 'const' palette array.
  CJB: 03-May-25: Fix #include filename case.
  CJB: 09-May-25: Dogfooding the _Optional qualifier.
 */

/* ISO library headers */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "swis.h"
#include "wimp.h"
#include "wimplib.h"
#include "toolbox.h"
#include "event.h"
#include "window.h"
#include "gadgets.h"

/* CBOSLib headers */
#include "MessTrans.h"
#include "PalEntry.h"
#include "ClrTrans.h"
#include "OSVDU.h"

/* Local headers */
#include "Pal256.h"
#include "scheduler.h"
#ifdef CBLIB_OBSOLETE
#include "msgtrans.h"
#include "Err.h"
#endif /* CBLIB_OBSOLETE */
#include "Internal/CBMisc.h"

#undef CancelDrag /* definition in "wimplib.h" is wrong! */
#define CancelDrag ((WimpDragBox *)-1)

/* Window component IDs */
enum
{
  ComponentId_Cancel_ActButton = 0x100,
  ComponentId_OK_ActButton     = 0x101,
  ComponentId_Palette_Button   = 0x104,
  ComponentId_Colour_Button    = 0x105,
  ComponentId_Colour_NumRange  = 0x106
};

#ifdef KEY_CONTROL
/* Toolbox event codes */
enum
{
  EventCode_Up = 0x1000,
  EventCode_Down,
  EventCode_Left,
  EventCode_Right,
  EventCode_JumpTop,
  EventCode_JumpBottom,
  EventCode_JumpLeft,
  EventCode_JumpRight,
  EventCode_TopLeft,
  EventCode_BottomRight
};
#endif /* KEY_CONTROL */

/* Other constant numeric values */
enum
{
  MaxValidationLen     = 15,
  XOrigin              = 28,
  YOrigin              = -540,
  NumRows              = 16,
  NumColumns           = 16,
  CellWidth            = 32,
  CellHeight           = 32,
  Width                = CellWidth * NumColumns,
  Height               = CellHeight * NumRows,
  BrightThreshold      = 128,
  DarkBorder           = 0x000000, /* black */
  LightBorder          = 0xffffff, /* white */
  DragPollFreq         = 10, /* in centiseconds */
  ButtonModifierDrag   = 16,
  ButtonModifierSingle = 256
};

typedef struct
{
  ObjectId      window_id; /* Toolbox window Id */
  int           wimp_handle; /* Wimp window handle */
  uint8_t       orig_col; /* as specified by client */
  uint8_t       orig_row;
  uint8_t       current_col; /* as displayed in dialogue */
  uint8_t       current_row;
  bool          dragging;
  PaletteEntry const *palette;
}
Pal256Data;

/* Keep this enumeration synchronised with mode_vars[] */
enum
{
  VarIndex_XEigFactor,
  VarIndex_YEigFactor,
  VarIndex_LAST
};

/* Keep this array synchronised with the enumeration above */
static const VDUVar mode_vars[VarIndex_LAST + 1] =
{
  (VDUVar)ModeVar_XEigFactor,
  (VDUVar)ModeVar_YEigFactor,
  VDUVar_EndOfList
};
static _Optional MessagesFD *desc;
#ifndef CBLIB_OBSOLETE
static void (*report)(CONST _kernel_oserror *);
#endif

/* ----------------------------------------------------------------------- */
/*                       Function prototypes                               */

static ToolboxEventHandler actionbutton_selected, object_deleted;
#ifdef KEY_CONTROL
static ToolboxEventHandler key_handler;
#else /* KEY_CONTROL */
static ToolboxEventHandler numberrange_value_changed;
#endif /* KEY_CONTROL */
static WimpEventHandler redraw_window, mouse_click, user_drag;
static _Optional CONST _kernel_oserror *display_colour(Pal256Data *pal_data, unsigned int col, unsigned int row, bool update_number);
static _Optional CONST _kernel_oserror *draw_colour(PaletteEntry palette_entry, int left_x, int bottom_y, bool selected);
static _Optional CONST _kernel_oserror *update_window(const Pal256Data *pal_data, unsigned int col, unsigned int row, bool selected);
static _Optional CONST _kernel_oserror *apply_selection(Pal256Data *pal_data);
static bool decode_pointer_pos(const WimpGetWindowStateBlock *window_state, int mouse_x, int mouse_y, unsigned int *row, unsigned int *col);
static CONST _kernel_oserror *lookup_error(const char *token);

/*----------------------------------------------------------------------- */
/*                        Public functions                                */

_Optional CONST _kernel_oserror *Pal256_initialise(
                         ObjectId             object,
                         PaletteEntry const   palette[]
#ifndef CBLIB_OBSOLETE
                        ,_Optional MessagesFD *mfd,
                         void                 (*report_error)(CONST _kernel_oserror *)
#endif
)
{
  _Optional Pal256Data *pal_data;
  _Optional CONST _kernel_oserror *e = NULL;

  assert(palette != NULL);

  /* Store pointers to messages file descriptor and error-reporting function */
#ifdef CBLIB_OBSOLETE
  desc = msgs_get_descriptor();
#else
  desc = mfd;
  report = report_error;
#endif

  /* Create associated data block */
  pal_data = malloc(sizeof(*pal_data));
  if (pal_data == NULL)
  {
    e = lookup_error("NoMem");
  }
  else
  {
    /* Initialise data block */
    pal_data->window_id = object;
    pal_data->current_col = pal_data->orig_col = 0;
    pal_data->current_row = pal_data->orig_row = NumRows - 1;
    pal_data->dragging = false;
    pal_data->palette = palette;

    /* Stash the Wimp handle of the window underlying the Toolbox object */
    e = window_get_wimp_handle(0, object, &pal_data->wimp_handle);

    /* Set the Toolbox object's client handle so that we can access its data
       given only its object ID */
    if (e == NULL)
      e = toolbox_set_client_handle(0, object, &*pal_data);

    /* Register Wimp event handlers */
    if (e == NULL)
      e = event_register_wimp_handler(object,
                                      Wimp_ERedrawWindow,
                                      redraw_window,
                                      &*pal_data);
    if (e == NULL)
      e = event_register_wimp_handler(object,
                                      Wimp_EMouseClick,
                                      mouse_click,
                                      &*pal_data);
    if (e == NULL)
      e = event_register_wimp_handler(-1,
                                      Wimp_EUserDrag,
                                      user_drag,
                                      &*pal_data);

    /* Register Toolbox event handlers */
    if (e == NULL)
      e = event_register_toolbox_handler(object,
                                         ActionButton_Selected,
                                         actionbutton_selected,
                                         &*pal_data);

#ifdef KEY_CONTROL
    if (e == NULL)
      e = event_register_toolbox_handler(object,
                                         -1,
                                         key_handler,
                                         &*pal_data);
#else /* KEY_CONTROL */
    if (e == NULL)
      e = event_register_toolbox_handler(object,
                                         NumberRange_ValueChanged,
                                         numberrange_value_changed,
                                         &*pal_data);
#endif /* KEY_CONTROL */
    if (e == NULL)
      e = event_register_toolbox_handler(object,
                                         Toolbox_ObjectDeleted,
                                         object_deleted,
                                         &*pal_data);

    if (e != NULL)
      object_deleted(0, &(ToolboxEvent){0}, &(IdBlock){0}, &*pal_data);
  }

  return e;
}

/* ----------------------------------------------------------------------- */

_Optional CONST _kernel_oserror *Pal256_set_colour(ObjectId object, unsigned int c)
{
  /* Set the currently selected colour */
  Pal256Data *pal_data;

  ON_ERR_RTN_E(toolbox_get_client_handle(0, object, (void **)&pal_data));

  DEBUGF("Pal256: Displaying colour %u\n", c);
  pal_data->orig_row = NumRows - 1 - (c / NumColumns);
  pal_data->orig_col = c % NumColumns;

  return display_colour(pal_data, pal_data->orig_col, pal_data->orig_row, true);
}

/* ----------------------------------------------------------------------- */

#ifdef CBLIB_OBSOLETE
/* The following function is deprecated; use palette_entry_brightness(). */
char Pal256_colour_brightness(unsigned long colour)
{
  return palette_entry_brightness((PaletteEntry)colour);
}
#endif /* CBLIB_OBSOLETE */

/* ----------------------------------------------------------------------- */
/*                         Private functions                               */

static void check_error(_Optional CONST _kernel_oserror *e)
{
#ifdef CBLIB_OBSOLETE
  (void)err_check(e);
#else
  if (e != NULL && report)
    report(&*e);
#endif
}

/* ----------------------------------------------------------------------- */

static CONST _kernel_oserror *lookup_error(const char *token)
{
  /* Look up error message from the token, outputting to an internal buffer */
  return messagetrans_error_lookup(desc, DUMMY_ERRNO, token, 0);
}

/* ----------------------------------------------------------------------- */

static SchedulerTime track_pointer(void *handle, SchedulerTime time_now, const volatile bool *time_up)
{
  Pal256Data *pal_data = handle;
  int mouse_x, mouse_y, but;
  ObjectId window;
  ComponentId comp;
  _Optional CONST _kernel_oserror *e = NULL;

  NOT_USED(time_up);

  DEBUGF("Pal256: idle function called\n");
  assert(pal_data != NULL);
  assert(pal_data->dragging);

  e = window_get_pointer_info(0, &mouse_x, &mouse_y, &but, &window, &comp);
  if (e == NULL)
  {
    if (TEST_BITS(but, Window_GetPointerNotToolboxWindow) ||
        window != pal_data->window_id ||
        comp != ComponentId_Palette_Button)
    {
      DEBUGF("Pal256: Pointer outside gadget\n");
    }
    else
    {
      WimpGetWindowStateBlock window_state;
      unsigned int row, col;

      window_state.window_handle = pal_data->wimp_handle;
      e = wimp_get_window_state(&window_state);
      if (e == NULL &&
          decode_pointer_pos(&window_state, mouse_x, mouse_y, &row, &col))
      {
        e = display_colour(pal_data, col, row, true);
      }
    }
  }
  check_error(e);
  return time_now + DragPollFreq; /* defer next call to this function */
}

/* ----------------------------------------------------------------------- */

static _Optional CONST _kernel_oserror *display_colour(Pal256Data *pal_data, unsigned int col, unsigned int row, bool update_number)
{
  /* Change the displayed colour */
  char validation[MaxValidationLen + 1];
  unsigned int colour = col + (NumRows - 1 - row) * NumColumns;

  assert(pal_data != NULL);
#ifdef KEY_CONTROL
  NOT_USED(update_number);
#endif /* KEY_CONTROL */

  if (pal_data->current_col == col && pal_data->current_row == row)
    return NULL; /* colour already displayed - nothing to do */

  ON_ERR_RTN_E(update_window(pal_data,
                             pal_data->current_col,
                             pal_data->current_row,
                             false));

  ON_ERR_RTN_E(update_window(pal_data, col, row, true));

  pal_data->current_col = col;
  pal_data->current_row = row;

  sprintf(validation,
          "R2;C/%X", pal_data->palette[colour] >> PaletteEntry_RedShift);

  ON_ERR_RTN_E(button_set_validation(0,
                                     pal_data->window_id,
                                     ComponentId_Colour_Button,
                                     validation));

#ifndef KEY_CONTROL
  if (update_number)
    ON_ERR_RTN_E(numberrange_set_value(0,
                                       pal_data->window_id,
                                       ComponentId_Colour_NumRange,
                                       colour));
#endif /* KEY_CONTROL */
  return NULL; /* success */
}

/* ----------------------------------------------------------------------- */
#ifdef KEY_CONTROL
static int key_handler(int event_code, ToolboxEvent *event, IdBlock *id_block, void *handle)
{
  Pal256Data *pal_data = handle;
  unsigned int col, row;
  NOT_USED(id_block);
  NOT_USED(event);

  assert(event != NULL);
  assert(id_block != NULL);
  assert(pal_data != NULL);

  col = pal_data->current_col;
  row = pal_data->current_row;

  switch (event_code)
  {
    case EventCode_Up:
      if (row < NumRows - 1)
        row++;
      break;

    case EventCode_Down:
      if (row > 0)
        row--;
      break;

    case EventCode_Left:
      if (col > 0)
        col--;
      break;

    case EventCode_Right:
      if (col < NumColumns - 1)
        col++;
      break;

    case EventCode_JumpTop:
      row = NumRows - 1;
      break;

    case EventCode_JumpBottom:
      row = 0;
      break;

    case EventCode_JumpLeft:
      col = 0;
      break;

    case EventCode_JumpRight:
      col = NumColumns - 1;
      break;

    case EventCode_TopLeft:
      row = NumRows - 1;
      col = 0;
      break;

    case EventCode_BottomRight:
      row = 0;
      col = NumColumns - 1;
      break;

    default:
      return 0; /* unknown event not handled */
  }
  /* Select colour */
  check_error(display_colour(pal_data, col, row, true));
  return 1; /* claim event */
}
#else /* KEY_CONTROL */

/* ----------------------------------------------------------------------- */

static int numberrange_value_changed(int event_code, ToolboxEvent *event, IdBlock *id_block, void *handle)
{
  Pal256Data *pal_data = handle;
  NumberRangeValueChangedEvent *nrvce = (NumberRangeValueChangedEvent *)event;
  unsigned int row, col;
  NOT_USED(event_code);

  assert(nrvce != NULL);
  assert(id_block != NULL);
  assert(pal_data != NULL);

  if (id_block->self_component != ComponentId_Colour_NumRange)
    return 0; /* unknown gadget */

  row = NumRows - 1 - (nrvce->new_value / NumColumns);
  col = nrvce->new_value % NumColumns;
  check_error(display_colour(pal_data, col, row, false));

  return 1; /* claim event */
}
#endif /* KEY_CONTROL */
/* ----------------------------------------------------------------------- */

static int actionbutton_selected(int event_code, ToolboxEvent *event, IdBlock *id_block, void *handle)
{
  _Optional CONST _kernel_oserror *e = NULL;
  Pal256Data *pal_data = handle;

  NOT_USED(event_code);
  assert(event != NULL);
  assert(id_block != NULL);
  assert(pal_data != NULL);

  switch (id_block->self_component)
  {
    case ComponentId_Cancel_ActButton:
      if (TEST_BITS(event->hdr.flags, ActionButton_Selected_Adjust))
      {
        /* restore default colour rather than closing dbox */
        e = display_colour(pal_data,
                           pal_data->orig_col,
                           pal_data->orig_row,
                           true);
      }
      break;

    case ComponentId_OK_ActButton:
      e = apply_selection(pal_data);
      break;

    default:
      return 0; /* not interested in this button */
  }

  check_error(e);
  return 1; /* claim event */
}

/* ----------------------------------------------------------------------- */

static int mouse_click(int event_code, WimpPollBlock *event, IdBlock *id_block, void *handle)
{
  /* In order that the pseudo-transient dbox mechanism can work
     we pass mouse click events on rather than claiming them */
  WimpMouseClickEvent *wmce = (WimpMouseClickEvent *)event;
  Pal256Data *pal_data = handle;
  unsigned int row, col;
  _Optional CONST _kernel_oserror *e = NULL;
  WimpGetWindowStateBlock window_state;

  NOT_USED(event_code);
  assert(wmce != NULL);
  assert(id_block != NULL);
  assert(pal_data != NULL);

  DEBUGF("Pal256: Mouse click %d on component &%x\n", wmce->buttons,
        id_block->self_component);

  if (!TEST_BITS(wmce->buttons,
                 Wimp_MouseButtonSelect |
                   Wimp_MouseButtonSelect * ButtonModifierSingle |
                   Wimp_MouseButtonSelect * ButtonModifierDrag) ||
      id_block->self_component != ComponentId_Palette_Button)
  {
    DEBUGF("Pal256: Ignoring click\n");
    return 0; /* pass event on */
  }

  window_state.window_handle = pal_data->wimp_handle;
  e = wimp_get_window_state(&window_state);
  if (e == NULL)
  {
    if (decode_pointer_pos(&window_state,
                           wmce->mouse_x,
                           wmce->mouse_y,
                           &row,
                           &col))
    {
      switch (wmce->buttons)
      {
        case Wimp_MouseButtonSelect: /* Double click */
          /* Select colour and close dialogue box */
          {
            unsigned int last_col = pal_data->current_col,
                         last_row = pal_data->current_row;

            e = display_colour(pal_data, col, row, true);
            if (e != NULL)
              break;

            /* Guard against unintended double-click activations
               (e.g. clicking too quickly in a neighbouring colour) */
            if (last_col == col && last_row == row)
            {
              e = apply_selection(pal_data);
              if (e == NULL)
                e = toolbox_hide_object(0, id_block->self_id);
            }
          }
          break;

        case Wimp_MouseButtonSelect * ButtonModifierDrag:
          {
            WimpDragBox drag_box;
            int colsleft_scrx, colsbot_scry;
            int eigen_factors[VarIndex_LAST];

            e = os_read_vdu_variables(mode_vars, eigen_factors);
            if (e != NULL)
              break;

            colsleft_scrx = window_state.visible_area.xmin - window_state.xscroll +
                            XOrigin;

            colsbot_scry = window_state.visible_area.ymax - window_state.yscroll +
                           YOrigin;

            drag_box.drag_type = Wimp_DragBox_DragPoint;
            drag_box.parent_box.xmin = colsleft_scrx;

            drag_box.parent_box.xmax = colsleft_scrx + Width -
                                       (1 << eigen_factors[VarIndex_XEigFactor]);

            drag_box.parent_box.ymin = colsbot_scry;

            drag_box.parent_box.ymax = colsbot_scry + Height -
                                       (1 << eigen_factors[VarIndex_YEigFactor]);

            e = wimp_drag_box(&drag_box);
            if (e != NULL)
              break;

            e = scheduler_register_delay(track_pointer,
                                         pal_data,
                                         0,
                                         SchedulerPriority_Min);
            if (e == NULL)
            {
              DEBUGF("Pal256: Idle function scheduled\n");
              pal_data->dragging = true;
            }
            else
            {
              wimp_drag_box(CancelDrag);
            }
          }
          break;

        case Wimp_MouseButtonSelect * ButtonModifierSingle:
          /* Select colour */
          e = display_colour(pal_data, col, row, true);
          break;
      }
    }
  }

  check_error(e);
  return 0; /* pass event on */
}

/* ----------------------------------------------------------------------- */

static int user_drag(int event_code, WimpPollBlock *event, IdBlock *id_block, void *handle)
{
  Pal256Data *pal_data = handle;

  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(id_block);

  assert(event != NULL);
  assert(id_block != NULL);
  assert(pal_data != NULL);

  if (pal_data->dragging)
  {
    DEBUGF("Pal256: Drag ended - removing our idle function\n");
    pal_data->dragging = false;
    scheduler_deregister(track_pointer, pal_data);
    return 1; /* claim event */
  }
  return 0; /* pass event on */
}

/* ----------------------------------------------------------------------- */

static int redraw_window(int event_code, WimpPollBlock *event, IdBlock *id_block, void *handle)
{
  /* Custom redraw for colour palette */
  const Pal256Data *pal_data = handle;
  _Optional CONST _kernel_oserror *e = NULL;
  int more, colsleft_scrx, colsbot_scry;
  WimpRedrawWindowBlock block;

  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(id_block);
  assert(handle != NULL);

  block.window_handle = pal_data->wimp_handle;

  /* Process each redraw rectangle in turn */
  for (e = wimp_redraw_window(&block, &more);
       e == NULL && more != 0;
       e = wimp_get_rectangle(&block, &more))
  {
    unsigned int min_column, max_column, row, min_row, max_row;

    DEBUGF("Pal256: redraw rectangle xmin:%d ymin:%d xmax:%d ymax:%d\n",
           block.redraw_area.xmin, block.redraw_area.ymin,
           block.redraw_area.xmax, block.redraw_area.ymax);

    assert(block.redraw_area.xmin <= block.redraw_area.xmax);
    assert(block.redraw_area.ymin <= block.redraw_area.ymax);

    /* Calculate palette origin in screen coordinates. Origin at the top left
       would require use of y eigen factor to avoid OS-to-pixel coordinate
       rounding errors. */
    colsleft_scrx = block.visible_area.xmin - block.xscroll + XOrigin;
    colsbot_scry = block.visible_area.ymax - block.yscroll + YOrigin;

    DEBUGF("Pal256: palette rectangle xmin:%d ymin:%d xmax:%d ymax:%d\n",
           colsleft_scrx, colsbot_scry,
           colsleft_scrx + Width, colsbot_scry + Height);

    /* Does the redraw area overlap the palette area? */
    if (block.redraw_area.xmin >= colsleft_scrx + Width ||
        block.redraw_area.xmax <= colsleft_scrx ||
        block.redraw_area.ymin >= colsbot_scry + Height ||
        block.redraw_area.ymax <= colsbot_scry)
    {
      DEBUGF("Nothing to redraw\n");
      continue;
    }

    /* Calculate rows and columns to draw */
    if (block.redraw_area.xmin <= colsleft_scrx)
      min_column = 0;
    else
      min_column = (unsigned)(block.redraw_area.xmin - colsleft_scrx) /
                   CellWidth;

    if (block.redraw_area.xmax >= colsleft_scrx + Width)
    {
      max_column = NumColumns - 1;
    }
    else
    {
      assert(block.redraw_area.xmax > colsleft_scrx);
      max_column = (unsigned)(block.redraw_area.xmax - colsleft_scrx) /
                   CellWidth;
      /* xmax is exclusive, so don't draw column that coincides exactly with it
       */
      if (max_column > 0 &&
          (unsigned)(block.redraw_area.xmax - colsleft_scrx) % CellWidth == 0)
      {
        max_column--;
      }
    }

    if (block.redraw_area.ymin <= colsbot_scry)
      min_row = 0;
    else
      min_row = (unsigned)(block.redraw_area.ymin - colsbot_scry) / CellHeight;

    if (block.redraw_area.ymax >= colsbot_scry + Height)
    {
      max_row = NumRows - 1;
    }
    else
    {
      assert(block.redraw_area.ymax > colsbot_scry);
      max_row = (unsigned)(block.redraw_area.ymax - colsbot_scry) / CellHeight;
      /* ymax is exclusive, so don't draw row that coincides exactly with it */
      if (max_row > 0 &&
          (unsigned)(block.redraw_area.ymax - colsbot_scry) % CellHeight == 0)
      {
        max_row--;
      }
    }

    DEBUGF("Pal256: start row:%u end row:%u start col:%u end col:%u\n",
          max_row, min_row, min_column, max_column);

    /* Draw palette */
    for (row = min_row;
         e == NULL && row <= max_row;
         row++)
    {
      unsigned int column,
                   colour = (NumRows - 1 - row) * NumColumns + min_column;

      for (column = min_column;
           e == NULL && column <= max_column;
           column++, colour++)
      {
        e = draw_colour(pal_data->palette[colour],
                        colsleft_scrx +
                          column * CellWidth,
                        colsbot_scry + row * CellHeight,
                        pal_data->current_col == column &&
                          pal_data->current_row == row);
      } /* next column */
    } /* next row */

    if (e != NULL)
      break;
  } /* next redraw rectangle */

  check_error(e);
  return 1; /* claim event */
}

/* ----------------------------------------------------------------------- */

static _Optional CONST _kernel_oserror *draw_colour(PaletteEntry colour,
                                          int          left_x,
                                          int          bottom_y,
                                          bool         selected)
{
  /* Plot filled colour square... */
  ON_ERR_RTN_E(colourtrans_set_gcol(ColourTrans_SetGCOL_UseECF,
                                    GCOLAction_OpaqueBG + GCOLAction_Overwrite,
                                    colour));

  ON_ERR_RTN_E(os_plot(PlotOp_SolidInclBoth + PlotOp_MoveAbs,
                       left_x,
                       bottom_y));

  ON_ERR_RTN_E(os_plot(PlotOp_RectangleFill + PlotOp_PlotFGRel,
                       CellWidth - 1,
                       CellHeight - 1));

  if (selected)
  {
    /* Plot b/w border if colour is selected.. */
    PaletteEntry border;

    if (palette_entry_brightness(colour) > BrightThreshold)
      border = (PaletteEntry)DarkBorder << PaletteEntry_RedShift;
    else
      border = (PaletteEntry)LightBorder << PaletteEntry_RedShift;

    ON_ERR_RTN_E(colourtrans_set_gcol(0,
                                      GCOLAction_OpaqueBG +
                                        GCOLAction_Overwrite,
                                      border));

    ON_ERR_RTN_E(os_plot(PlotOp_SolidExclEnd + PlotOp_PlotFGRel,
                         - (CellWidth - 1),
                         0));

    ON_ERR_RTN_E(os_plot(PlotOp_SolidExclEnd + PlotOp_PlotFGRel,
                         0,
                         - (CellHeight - 1)));

    ON_ERR_RTN_E(os_plot(PlotOp_SolidExclEnd + PlotOp_PlotFGRel,
                         (CellWidth - 1),
                         0));

    ON_ERR_RTN_E(os_plot(PlotOp_SolidExclEnd + PlotOp_PlotFGRel,
                         0,
                         CellHeight - 1));
  }

  return NULL; /* success */
}

/* ----------------------------------------------------------------------- */

static int object_deleted(int           event_code,
                          ToolboxEvent *event,
                          IdBlock      *id_block,
                          void         *handle)
{
  /* Remove handlers */
  Pal256Data *pal_data = handle;

  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(id_block);
  assert(handle != NULL);

  /* Remember id_block will not be valid if called from Pal256_initialise */
  check_error(
    event_deregister_toolbox_handlers_for_object(pal_data->window_id));

  check_error(event_deregister_wimp_handlers_for_object(pal_data->window_id));

  if (pal_data->dragging)
  {
    DEBUGF("Pal256: Object deleted - removing our idle function\n");
    scheduler_deregister(track_pointer, pal_data);
  }

  free(pal_data);

  return 1; /* claim event */
}

/* ----------------------------------------------------------------------- */

static _Optional CONST _kernel_oserror *update_window(const Pal256Data *pal_data,
                                                      unsigned int      col,
                                                      unsigned int      row,
                                                      bool              selected)
{
  /* Calculate redraw rectangle in work area (relative) coordinates */
  int left_x = XOrigin + col * CellWidth;
  int bottom_y = YOrigin + row * CellHeight;
  WimpRedrawWindowBlock block;
  int more;
  _Optional CONST _kernel_oserror *e = NULL;

  assert(pal_data != NULL);

  block.window_handle = pal_data->wimp_handle;
  block.visible_area.xmin = left_x;
  block.visible_area.ymin = bottom_y;
  block.visible_area.xmax = left_x + CellWidth;
  block.visible_area.ymax = bottom_y + CellHeight;

  DEBUGF("Pal256: Updating rectangle x: %d,%d y: %d,%d\n",
        block.visible_area.xmin,
        block.visible_area.xmax,
        block.visible_area.ymin,
        block.visible_area.ymax);

  /* Process each redraw rectangle in turn */
  for (e = wimp_update_window(&block, &more);
       e == NULL && more != 0;
       e = wimp_get_rectangle(&block, &more))
  {
    /* N.B. draw_colour requires absolute coordinates */
    e = draw_colour(pal_data->palette[col + (NumRows - 1 - row) * NumColumns],
                    block.visible_area.xmin - block.xscroll + left_x,
                    block.visible_area.ymax - block.yscroll + bottom_y,
                    selected);
    if (e != NULL)
      break;
  } /* endwhile (more) */

  return e;
}

/* ----------------------------------------------------------------------- */

static _Optional CONST _kernel_oserror *apply_selection(Pal256Data *pal_data)
{
  Pal256ColourSelectedEvent warn_client;

  assert(pal_data != NULL);

  /* This becomes the new default colour */
  pal_data->orig_col = pal_data->current_col;
  pal_data->orig_row = pal_data->current_row;

  /* Raise event to tell client of colour selection */
  warn_client.hdr.size = sizeof(warn_client);
  warn_client.hdr.event_code = Pal256_ColourSelected;
  warn_client.hdr.flags = 0;
  warn_client.colour_number = pal_data->current_col +
                              (NumRows - 1 - pal_data->current_row) *
                                NumColumns;
  DEBUGF("Pal256: Colour %u selected\n", warn_client.colour_number);

  return toolbox_raise_toolbox_event(0,
                                     pal_data->window_id,
                                     NULL_ComponentId,
                                     (ToolboxEvent *)&warn_client);
}

/* ----------------------------------------------------------------------- */

static bool decode_pointer_pos(const WimpGetWindowStateBlock *window_state,
                               int                            mouse_x,
                               int                            mouse_y,
                               unsigned int                  *row,
                               unsigned int                  *col)
{
  /* Decode pointer position relative to palette window. Returns false if the
     pointer is outside the palette area. */
  bool success = false; /* pointer outside palette */

  assert(window_state != NULL);
  DEBUGF("Pal256: decoding pointer position %d,%d\n", mouse_x, mouse_y);

  mouse_x -= (window_state->visible_area.xmin - window_state->xscroll +
              XOrigin);
  if (mouse_x >= 0 && mouse_x < Width)
  {
    mouse_y -= (window_state->visible_area.ymax - window_state->yscroll +
               YOrigin);
    if (mouse_y >= 0 && mouse_y < Height)
    {
      unsigned int c = (unsigned)mouse_x / CellWidth;
      unsigned int r = (unsigned)mouse_y / CellHeight;
      DEBUGF("Pal256: pointer is in column %u of row %u\n", c, r);

      /* Output the row and column numbers, if requested */
      if (row != NULL)
        *row = r;

      if (col != NULL)
        *col = c;

      success = true; /* pointer within palette */
    }
  }
  if (!success)
  {
    DEBUGF("Pal256: pointer not over the palette\n");
  }
  return success;
}
