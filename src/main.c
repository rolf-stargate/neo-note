// TODO(rolf): Error handle all allocations

#include "raylib.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define INIT_SIZE_PAGE 1
#define GAP_SIZE_PAGE 5

#define INIT_SIZE_LINE 1
#define GAP_SIZE_LINE 5

typedef struct
{
  void *buffer;
  int gap_start;
  int gap_end;
  int buf_size;
} GapBuffer;

typedef struct
{
  char *buffer;
  int gap_start;
  int gap_end;
  int buf_size;
} GapBufferLine;

typedef struct
{
  GapBufferLine **buffer;
  int gap_start;
  int gap_end;
  int buf_size;
} GapBufferPage;

typedef struct
{
  int line;
  int pos;
} Cursor;

typedef enum
{
  OUTSIDE_LEFT,
  OUTSIDE_RIGHT,
  BEFORE_GAP,
  AFTER_GAP,
  GAP_PLUS_ONE,
  GAP_MINUS_ONE,
  INSIDE_GAP_START,
  INSIDE_GAP_INBETWEEN,
} IndexPositionInGapArray;

typedef enum
{
  BEFORE,
  AFTER,
} Direction;

// =============================================================================
// === Utilities
// =============================================================================

IndexPositionInGapArray
get_index_pos_in_gap_array (int index, int gap_start, int gap_end, int buf_size)
{
  if (index < 0)
  {
    return OUTSIDE_LEFT;
  }
  if (index > buf_size - 1)
  {
    return OUTSIDE_RIGHT;
  }
  if (index == gap_start - 1)
  {
    return GAP_MINUS_ONE;
  }
  if (index == gap_end + 1)
  {
    return GAP_PLUS_ONE;
  }
  if (index < gap_start)
  {
    return BEFORE_GAP;
  }
  if (index > gap_end)
  {
    return AFTER_GAP;
  }
  if (gap_start == 0)
  {
    return INSIDE_GAP_START;
  }
  return INSIDE_GAP_INBETWEEN;
}

void
print_line (GapBufferLine *gbl)
{
  printf ("[");
  for (int i = 0; i < gbl->buf_size; i++)
  {
    if (i < gbl->gap_start || i > gbl->gap_end)
      printf ("%c", gbl->buffer[i]);
    else if (i < gbl->buf_size - 1)
      printf ("_");
  }
  printf ("]\n");
}

void
print_page (GapBufferPage *gbp)
{
  printf ("[\n");
  for (int i = 0; i < gbp->buf_size; i++)
  {
    if (i < gbp->gap_start || i > gbp->gap_end)
    {
      print_line (gbp->buffer[i]);
    }
    else
    {
      printf ("_\n");
    }
  }
  printf ("]\n");
}

// =============================================================================
// === DEBUG
// =============================================================================

int count = 0;
int
renderPageDebug (GapBufferPage *gbp, char *buffer, int size, Cursor c)
{
  int pos = 0;
  buffer[pos] = '[';
  pos++;
  buffer[pos] = '\n';
  pos++;

  for (int i = 0; i < gbp->buf_size; i++)
  {
    if (i < gbp->gap_start || i > gbp->gap_end)
    {
      GapBufferLine *gbl = gbp->buffer[i];
      buffer[pos] = '[';
      pos++;

      for (int l = 0; l < gbl->buf_size; l++)
      {
        // Render Char/Gap;
        if (l < gbl->gap_start || l > gbl->gap_end)
        {
          // Account for padding
          if (l < gbl->buf_size - 1)
          {
            buffer[pos] = gbl->buffer[l];
            pos++;
          }
          else if (l == gbl->buf_size - 1)
          {
            buffer[pos] = ':';
            pos++;
          }
        }
        else if (l < gbl->buf_size - 1)
        {
          buffer[pos] = '_';
          pos++;
        }

        // Cursor
        if (i == c.line && l == c.pos)
        {
          if (count++ % 2)
          {
            buffer[pos - 1] = (char)219;
          }
        }
      }
      buffer[pos] = ']';
      pos++;
      buffer[pos] = '\n';
      pos++;
    }
    else
    {
      buffer[pos] = '_';
      pos++;
      buffer[pos] = '\n';
      pos++;
    }
  }
  buffer[pos] = ']';
  pos++;
  buffer[pos] = '\n';
  pos++;
  buffer[pos] = '\0';

  return 1;
}

// =============================================================================
// === Cursor
// =============================================================================

Cursor *
init_cursor (int line, int pos)
{
  Cursor *c = malloc (sizeof (Cursor));
  c->line = line;
  c->pos = pos;

  return c;
}

int
move_cursor_next_line (Cursor *c, GapBufferPage *gbp)
{
  if (c->line < gbp->buf_size - 1)
  {
    GapBufferLine *next_line = gbp->buffer[c->line + 1];
    if (next_line != NULL)
    {
      c->line++;
      return 0;
    }
  }
  return 1;
}

int
move_cursor_previous_line (Cursor *c, GapBufferPage *gbp)
{
  if (c->line > 0)
  {
    GapBufferLine *next_line = gbp->buffer[c->line - 1];
    if (next_line != NULL)
    {
      c->line--;
      return 0;
    }
  }
  return 1;
}

IndexPositionInGapArray
move_cursor (Cursor *c, GapBufferPage *gbp, int new_index)
{
  GapBufferLine *line = gbp->buffer[c->line];
  IndexPositionInGapArray state = get_index_pos_in_gap_array (
      new_index,
      line->gap_start,
      line->gap_end,
      line->buf_size);

  switch (state)
  {
  case OUTSIDE_RIGHT:
    if (line->gap_end == line->buf_size - 1)
    {
      c->pos = line->gap_start - 1;
    }
    else
    {
      c->pos = line->buf_size - 1;
    }
    return state;
  case OUTSIDE_LEFT:
    if (line->gap_start == 0)
    {
      c->pos = line->gap_end + 1;
    }
    else
    {
      c->pos = 0;
    }
    return state;
  case BEFORE_GAP:
  case AFTER_GAP:
  case GAP_PLUS_ONE:
  case GAP_MINUS_ONE:
    c->pos = new_index;
    return state;
  case INSIDE_GAP_START:
    c->pos = line->gap_end + 1;
    return state;
  case INSIDE_GAP_INBETWEEN:
    if (c->pos > new_index)
    {
      c->pos = line->gap_start - 1;
    }
    else
    {
      c->pos = line->gap_end + 1;
    }
    return state;
  }
}

// =============================================================================
// === Gap Buffer
// =============================================================================

void
move_gap (GapBuffer *gb, int index, size_t element_size)
{
  int dest;
  int src;
  int size;
  int gap_size = gb->gap_end - gb->gap_start + 1;

  assert (index < gb->buf_size);

  if (index < 0)
    index = 0;

  if (index > gb->gap_end)
  {
    dest = gb->gap_start;
    src = gb->gap_end + 1;
    size = index - gb->gap_end - 1;

    memmove (
        (char *)gb->buffer + dest * element_size,
        (char *)gb->buffer + src * element_size,
        size * element_size);

    gb->gap_end = index - 1;
    gb->gap_start = gb->gap_end - (gap_size - 1);
  }
  else if (index < gb->gap_end)
  {
    if (index > gb->gap_start)
    {
      dest = gb->gap_start;
      src = gb->gap_end + 1;

      size = index - gb->gap_start;
    }
    else
    {
      dest = index + gap_size;
      src = index;

      size = gb->gap_start - index;
    }
    memmove (
        (char *)gb->buffer + dest * element_size,
        (char *)gb->buffer + src * element_size,
        size * element_size);

    gb->gap_start = index;
    gb->gap_end = index + (gap_size - 1);
  }
}

void
expand_gap (GapBuffer *gb, int size, size_t element_size)
{
  // The gab is a resource we never want to shrink it
  if (size <= (gb->gap_end - gb->gap_start + 1))
    return;

  int new_size = gb->buf_size + size - (gb->gap_end - gb->gap_start + 1);
  char *new_buffer = malloc (new_size * element_size);

  // TODO: Only for debugging
  for (int i = 0; i < new_size; i++)
  {
    new_buffer[i] = 'A';
  }

  memcpy (new_buffer, gb->buffer, gb->gap_start * element_size);
  memcpy (
      // 123_35
      // gap_size=5
      // gap_end=3
      new_buffer + (gb->gap_start + size) * element_size,
      &gb->buffer[(gb->gap_end + 1) * element_size],
      (gb->buf_size - gb->gap_end) * element_size);

  free (gb->buffer);
  gb->buffer = new_buffer;

  gb->gap_end = gb->gap_start + size - 1;
  gb->buf_size = new_size;
}

// =============================================================================
// === Gap Buffer Line
// =============================================================================

GapBufferLine *
init_gap_buffer_line (int initial_size, int gap_size)
{
  GapBufferLine *gbl = malloc (sizeof (GapBufferLine));
  int buf_size = (initial_size + gap_size) * sizeof (char);

  gbl->buffer = malloc (buf_size);
  gbl->gap_start = 0;
  gbl->gap_end = gap_size - 1;
  gbl->buf_size = buf_size;

  assert (gbl->gap_end < gbl->buf_size);

  for (int i = 0; i < gbl->buf_size; i++)
  {
    gbl->buffer[i] = '\0';
  }

  return gbl;
}

void
move_gap_line (GapBufferLine *gbl, int index)
{
  GapBuffer gb = { gbl->buffer, gbl->gap_start, gbl->gap_end, gbl->buf_size };
  move_gap (&gb, index, sizeof (char));
  gbl->gap_start = gb.gap_start;
  gbl->gap_end = gb.gap_end;
}

void
expand_gap_line (GapBufferLine *gbl, int new_gap_size)
{
  GapBuffer gb = { gbl->buffer, gbl->gap_start, gbl->gap_end, gbl->buf_size };
  expand_gap (&gb, new_gap_size, sizeof (char));

  gbl->buffer = gb.buffer;
  gbl->buf_size = gb.buf_size;
  gbl->gap_start = gb.gap_start;
  gbl->gap_end = gb.gap_end;
}

void
insert_single_char (GapBufferLine *gbl, char value)
{
  if (gbl->gap_end == gbl->gap_start)
    expand_gap_line (gbl, GAP_SIZE_LINE);
  gbl->buffer[gbl->gap_start] = value;
  gbl->gap_start++;
}

void
delete_single_char (Cursor *c, GapBufferPage *gbp)
{
  GapBufferLine *line = gbp->buffer[c->line];
  if (c->pos == 0)
  {
    // TODO: Concatenate previous line with current line
    move_cursor_previous_line (c, gbp);
    // delete_single_char (c, gbp);
  }
  else if (c->pos == line->gap_end + 1)
  {
    if (line->gap_start == 0)
    {
      // TODO: Concatenate previous line with current line
      move_cursor_previous_line (c, gbp);
      // delete_single_char (c, gbp);
    }
    else
    {
      line->gap_start--;
    }
  }
  else if (c->pos == line->gap_end + 2)
  {
    line->gap_end++;
  }
  else if (c->pos < line->gap_start)
  {
    move_gap_line (line, c->pos);
    line->gap_start--;
    c->pos = line->gap_end + 1;
  }
  else if (c->pos > line->gap_end + 2)
  {
    move_gap_line (line, c->pos - 1);
    line->gap_start--;
  }
}

// =============================================================================
// === Gab Buffer Page
// =============================================================================

GapBufferPage *
init_gap_buffer_page (int initial_size, int gap_size)
{
  GapBufferPage *gbp = malloc (sizeof (GapBufferPage));
  gbp->buffer = malloc ((initial_size + gap_size) * sizeof (GapBufferLine *));
  gbp->gap_start = 0;
  gbp->gap_end = gap_size - 1;
  gbp->buf_size = initial_size + gap_size;

  for (int i = 0; i < gbp->buf_size; i++)
  {
    gbp->buffer[i] = NULL;
  }

  return gbp;
}

void
move_gap_page (GapBufferPage *gbp, int index)
{
  assert (index >= 0);
  assert (index < gbp->buf_size);

  GapBuffer gb = { gbp->buffer, gbp->gap_start, gbp->gap_end, gbp->buf_size };
  move_gap (&gb, index, sizeof (GapBufferLine *));
  gbp->gap_start = gb.gap_start;
  gbp->gap_end = gb.gap_end;
}

void
expand_gap_page (GapBufferPage *gbp, int new_gap_size)
{
  GapBuffer gb = { gbp->buffer, gbp->gap_start, gbp->gap_end, gbp->buf_size };
  expand_gap (&gb, new_gap_size, sizeof (GapBufferLine *));
  gbp->buffer = gb.buffer;
  gbp->buf_size = gb.buf_size;
  gbp->gap_start = gb.gap_start;
  gbp->gap_end = gb.gap_end;
}

int
insert_single_line (
    GapBufferPage *gbp,
    GapBufferLine *new_line,
    int line_index,
    Direction dir)
{
  if (gbp->gap_end == gbp->gap_start)
    expand_gap_page (gbp, GAP_SIZE_PAGE);

  int index_new_line;

  assert (line_index < gbp->buf_size);
  GapBufferLine *line = gbp->buffer[line_index];

  if (dir == BEFORE)
  {
    if (line_index != gbp->gap_end + 1)
    {
      move_gap_page (gbp, line_index);
    }
    index_new_line = gbp->gap_start;
    gbp->buffer[gbp->gap_start] = new_line;
    gbp->gap_start++;
  }
  else if (dir == AFTER)
  {
    if (line_index != gbp->gap_start - 1)
    {
      if (line_index == gbp->buf_size - 1)
      {
        move_gap_page (
            gbp,
            gbp->buf_size - 1 - (gbp->gap_end - gbp->gap_start));
      }
      else
      {
        move_gap_page (gbp, gbp->gap_start);
      }
    }
    index_new_line = gbp->gap_start;
    gbp->buffer[gbp->gap_start] = new_line;
    gbp->gap_start++;
  }
  return index_new_line;
}

void
delete_single_line (GapBufferPage *gbp)
{
  if (gbp->gap_start > 0)
  {
    gbp->gap_start--;
  }
};
;

// =============================================================================
// === main
// =============================================================================

int
main (void)
{

  // === Initialization
  // ========================================================

  // Raylib
  const int screen_width = 400;
  const int screen_height = 400;

  InitWindow (screen_width, screen_height, "NeoNote");

  SetTargetFPS (60);

  // Page Buffer

  int page_gap_start = 0;
  int line_gap_start = 0;

  GapBufferLine *current_line
      = init_gap_buffer_line (INIT_SIZE_LINE, GAP_SIZE_LINE);
  for (int i = 0; i < current_line->buf_size; i++)
  {
    current_line->buffer[i] = 'A';
  }

  GapBufferPage *page = init_gap_buffer_page (INIT_SIZE_PAGE, GAP_SIZE_PAGE);
  ;
  page->buffer[page->gap_end + 1] = current_line;

  // Init Cursor
  assert (page->gap_end + 1 < page->buf_size);
  assert (current_line->gap_end + 1 < current_line->buf_size);
  Cursor *cursor = init_cursor (page->gap_end + 1, current_line->gap_end + 1);

  // Debug
  char debugTextBuffer[8192] = { 0 };

  // === Main Loop
  // ===========================================================================
  while (!WindowShouldClose ())
  {
    // Upadate
    int key = GetKeyPressed ();
    current_line = page->buffer[cursor->line];

    while (key > 0)
    {
      if ((key >= 32) && (key <= 125))
      {
        if (cursor->pos == current_line->gap_end + 1)
        {
          insert_single_char (current_line, key);
          cursor->pos = current_line->gap_end + 1;
        }
        else
        {
          move_gap_line (current_line, cursor->pos);
          insert_single_char (current_line, key);

          assert (current_line->gap_end + 1 < current_line->buf_size);
          move_cursor (cursor, page, current_line->gap_end + 1);
        }
      }
      if (key == KEY_UP)
      {
        // TODO: Check if it is worth to calculate out the gap size
        if (move_cursor_previous_line (cursor, page) == 0)
        {
          current_line = page->buffer[cursor->line];
          int last_pos = cursor->pos;
          move_cursor (cursor, page, last_pos);
        }
      }

      if (key == KEY_DOWN)
      {
        // TODO: Check if it is worth to calculate out the gap size
        if (move_cursor_next_line (cursor, page) == 0)
        {
          current_line = page->buffer[cursor->line];
          int last_pos = cursor->pos;
          move_cursor (cursor, page, last_pos);
        }
      }

      if (key == KEY_RIGHT)
        if (move_cursor (cursor, page, cursor->pos + 1) == OUTSIDE_RIGHT)
        {
          if (move_cursor_next_line (cursor, page) == 0)
          {
            current_line = page->buffer[cursor->line];
            move_cursor (cursor, page, 0);
          }
        }

      if (key == KEY_LEFT)
      {
        IndexPositionInGapArray state
            = move_cursor (cursor, page, cursor->pos - 1);
        if (state == OUTSIDE_LEFT || state == INSIDE_GAP_START)
        {
          if (move_cursor_previous_line (cursor, page) == 0)
          {
            current_line = page->buffer[cursor->line];
            move_cursor (cursor, page, current_line->buf_size - 1);
          }
        }
      }

      if (key == KEY_BACKSPACE)
      {
        if (cursor->pos != 0
            && (current_line->gap_start != 0
                || cursor->pos > current_line->gap_end))
        {
          delete_single_char (cursor, page);
        }
        else if (cursor->line != 0)
        {
          GapBufferLine *previous_line = page->buffer[cursor->line - 1];
          // move cursor->pos to the end of the previous line
          cursor->pos = previous_line->buf_size - 1;

          // copy current_line to end of privious line
          for (int i = 0; i < current_line->buf_size - 2; i++)
          {
            // TODO: Make a functon to insert more than one char
            //       Maybe copy hole memory area?
            if (i < current_line->gap_start || i > current_line->gap_end)
              insert_single_char (previous_line, current_line->buffer[i]);
          }
          // delete current_line
          move_gap_page (page, cursor->line + 1);
          delete_single_line (page);
          cursor->line--;
        }
      }

      if (key == KEY_ENTER)
      {
        GapBufferLine *new_line
            = init_gap_buffer_line (INIT_SIZE_LINE, GAP_SIZE_LINE);

        // TODO: Only for debug print
        for (int i = 0; i < new_line->buf_size; i++)
        {
          new_line->buffer[i] = 'A';
        }

        // Cursor on start of the line
        // or Cursor one after gap that starts at index 0
        if (cursor->pos == 0
            || (cursor->pos == current_line->gap_end + 1
                && current_line->gap_start == 0))
        {
          int index_new_line
              = insert_single_line (page, new_line, cursor->line, BEFORE);
          cursor->line = index_new_line;
          cursor->pos = new_line->gap_end + 1;
        }
        // Cursor at end of the line
        // (this works because the line should always have padding padding on
        // the end)
        else if (cursor->pos == current_line->buf_size - 1)
        {
          int index_new_line
              = insert_single_line (page, new_line, cursor->line, AFTER);
          cursor->line = index_new_line;
          cursor->pos = new_line->gap_end + 1;
        }
        else
        {
          move_gap_line (new_line, 0);
          for (int i = cursor->pos; i < current_line->buf_size - 2; i++)
          {
            // TODO: Make a functon to insert more than one char
            //       Maybe copy hole memory area?
            if (i < current_line->gap_start || i > current_line->gap_end)
              insert_single_char (new_line, current_line->buffer[i]);
          }
          current_line->gap_start = cursor->pos;
          current_line->gap_end = current_line->buf_size - 2;

          int index_new_line
              = insert_single_line (page, new_line, cursor->line, AFTER);
          cursor->line = index_new_line;
          cursor->pos = 0;
        }
      }

      key = GetKeyPressed ();
    }

    // Draw
    BeginDrawing ();

    ClearBackground (RAYWHITE);

    renderPageDebug (page, debugTextBuffer, 8192, *cursor);
    DrawText (debugTextBuffer, 10, 90, 10, DARKGRAY);
    DrawText (TextFormat ("pos: %d", cursor->pos), 10, 10, 10, DARKGRAY);
    DrawText (TextFormat ("line: %d", cursor->line), 10, 20, 10, DARKGRAY);
    DrawText (
        TextFormat ("page->gap_start: %d", page->gap_start),
        10,
        30,
        10,
        DARKGRAY);
    DrawText (
        TextFormat ("page->gap_end: %d", page->gap_end),
        10,
        40,
        10,
        DARKGRAY);
    DrawText (
        TextFormat ("page->buf_size: %d", current_line->buf_size),
        10,
        50,
        10,
        DARKGRAY);
    DrawText (
        TextFormat ("current_line->gap_start: %d", current_line->gap_start),
        10,
        60,
        10,
        DARKGRAY);
    DrawText (
        TextFormat ("current_line->gap_end: %d", current_line->gap_end),
        10,
        70,
        10,
        DARKGRAY);
    DrawText (
        TextFormat ("current_line->buf_size: %d", current_line->buf_size),
        10,
        80,
        10,
        DARKGRAY);

    EndDrawing ();
  }

  // === De-Initialization
  // ===========================================================================
  CloseWindow ();
  return 0;
}
