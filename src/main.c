// TODO(rolf): Error handle all allocations
#include "raylib.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define INIT_SIZE_PAGE 1
#define INIT_SIZE_LINE 1
#define GAP_SIZE 5

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
} PositionInGapArray;

typedef enum
{
  BEFORE,
  AFTER,
} DIRECTION;

typedef enum
{
  GAP_END,
  GAP_START
} GAP_POSITION;

// =============================================================================
// === Utilities
// =============================================================================

PositionInGapArray
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
void
render_page_debug (GapBufferPage *gbp, char *buffer, int size, Cursor c)
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
    if (c->line + 1 == gbp->gap_start)
    {
      if (gbp->gap_end != gbp->buf_size - 1)
      {
        c->line = gbp->gap_end + 1;
        return 0;
      }
    }
    else
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
    if (c->line - 1 == gbp->gap_end)
    {
      if (gbp->gap_start != 0)
      {
        c->line = gbp->gap_start - 1;
        return 0;
      }
    }
    else
    {
      c->line--;
      return 0;
    }
  }
  return 1;
}

PositionInGapArray
move_cursor (Cursor *c, GapBufferPage *gbp, int new_index)
{
  GapBufferLine *line = gbp->buffer[c->line];
  PositionInGapArray state = get_index_pos_in_gap_array (
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
move_gap_start (GapBuffer *gb, int index, size_t element_size)
{
  int dest;
  int src;
  int size;
  int gap_size = gb->gap_end - gb->gap_start + 1;

  assert (index < gb->buf_size);
  if (index < 0)
    index = 0;

  // Index before gap
  if (index < gb->gap_start)
  {
    dest = index + gap_size;
    src = index;
    size = gb->gap_start - index;
  }
  // index after gap
  else if (index > gb->gap_end)
  {
    // Not enough space for gap after index
    if (gb->buf_size - gap_size < index)
    {
      index = gb->buf_size - gap_size;
    }
    dest = gb->gap_start;
    src = gb->gap_end + 1;
    size = index - gb->gap_start + 1;
  }
  // index inside gap
  else
  {
    // Not enough space for gap after index
    if (gb->buf_size - gap_size < index)
    {
      index = gb->buf_size - gap_size;
    }
    dest = gb->gap_start;
    src = gb->gap_end + 1;
    size = index - gb->gap_start;
  }

  memmove (
      (char *)gb->buffer + dest * element_size,
      (char *)gb->buffer + src * element_size,
      size * element_size);

  gb->gap_start = index;
  gb->gap_end = index + (gap_size - 1);
}

void
move_gap_end (GapBuffer *gb, int index, size_t element_size)
{
  int dest;
  int src;
  int size;
  int gap_size = gb->gap_end - gb->gap_start + 1;

  assert (index < gb->buf_size);
  if (index < 0)
    index = 0;

  // Index before gap
  if (index < gb->gap_start)
  {

    // Not enough space for gap before index
    if (gb->buf_size - gap_size < index)
      if (index < gap_size - 1)
      {
        index = gap_size - 1;
      }

    dest = index + 1;
    src = index - (gap_size - 1);
    size = gb->gap_start - src;
  }
  // index after gap
  else if (index > gb->gap_end)
  {
    dest = gb->gap_start;
    src = gb->gap_end + 1;
    size = index - gb->gap_end;
  }
  // index inside gap
  else
  {
    // Not enough space for gap before index
    if (index < gap_size - 1)
    {
      index = gap_size - 1;
    }

    dest = index + 1;
    src = index - (gap_size - 1);
    size = gb->gap_start - src;
  }

  memmove (
      (char *)gb->buffer + dest * element_size,
      (char *)gb->buffer + src * element_size,
      size * element_size);

  gb->gap_end = index;
  gb->gap_start = index - (gap_size - 1);
}

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

  // if goal is after gap
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

  // NOTE: Only for debugging
  for (int i = 0; i < new_size; i++)
  {
    new_buffer[i] = 'A';
  }

  memcpy (new_buffer, gb->buffer, gb->gap_start * element_size);
  memcpy (
      new_buffer + (gb->gap_start + size) * element_size,
      &gb->buffer[(gb->gap_end + 1) * element_size],
      (gb->buf_size - gb->gap_end) * element_size);

  free (gb->buffer);
  gb->buffer = new_buffer;

  gb->gap_end = gb->gap_start + size - 1;
  gb->buf_size = new_size;
}

void
insert_in_gap (GapBuffer *gb, char *buffer_ptr, int count, size_t element_size)
{
  int gap_size = gb->gap_end - gb->gap_start + 1;

  if (gap_size <= count)
  {
    expand_gap (gb, count + GAP_SIZE, element_size);
  }
  memcpy (gb->buffer, buffer_ptr, count * element_size);

  gb->gap_start = gb->gap_start + count;
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
move_gap_line (GapBufferLine *gbl, int index, GAP_POSITION gap_pos)
{
  GapBuffer gb = { gbl->buffer, gbl->gap_start, gbl->gap_end, gbl->buf_size };

  switch (gap_pos)
  {
  case GAP_START:
    move_gap_start (&gb, index, sizeof (char));
    break;
  case GAP_END:
    move_gap_end (&gb, index, sizeof (char));
    break;
  }

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
insert_single_char (GapBufferLine *gbl, char value, GAP_POSITION gap_pos)
{
  if (gbl->gap_end == gbl->gap_start)
    expand_gap_line (gbl, GAP_SIZE);
  switch (gap_pos)
  {
  case GAP_START:
    gbl->buffer[gbl->gap_start] = value;
    gbl->gap_start++;
    break;
  case GAP_END:
    gbl->buffer[gbl->gap_end] = value;
    gbl->gap_end--;
    break;
  }
}

void
insert_in_gap_line (GapBufferLine *gbl, char *buffer_ptr, int count)
{
  GapBuffer gb = { gbl->buffer, gbl->gap_start, gbl->gap_end, gbl->buf_size };
  insert_in_gap (&gb, buffer_ptr, count, sizeof (GapBufferLine *));
  gbl->buffer = gb.buffer;
  gbl->buf_size = gb.buf_size;
  gbl->gap_start = gb.gap_start;
  gbl->gap_end = gb.gap_end;
}

void
delete_single_char (GapBufferLine *gbl, GAP_POSITION gap_pos)
{
  switch (gap_pos)
  {
  case GAP_START:
    if (gbl->gap_start > 0)
    {
      gbl->gap_start--;
    }
    break;
  case GAP_END:
    if (gbl->gap_end < gbl->buf_size - 1)
    {
      gbl->gap_end++;
    }
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
move_gap_page (GapBufferPage *gbp, int index, GAP_POSITION gap_pos)
{
  assert (index >= 0);
  assert (index < gbp->buf_size);

  GapBuffer gb = { gbp->buffer, gbp->gap_start, gbp->gap_end, gbp->buf_size };

  switch (gap_pos)
  {
  case GAP_START:
    move_gap_start (&gb, index, sizeof (GapBufferLine *));
    break;
  case GAP_END:
    move_gap_end (&gb, index, sizeof (GapBufferLine *));
    break;
  }

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
    DIRECTION dir)
{
  if (gbp->gap_end == gbp->gap_start)
    expand_gap_page (gbp, GAP_SIZE);

  int index_new_line;

  assert (line_index < gbp->buf_size);
  GapBufferLine *line = gbp->buffer[line_index];

  if (dir == BEFORE)
  {
    if (line_index != gbp->gap_end + 1)
    {
      move_gap_page (gbp, line_index, GAP_END);
    }
    index_new_line = gbp->gap_start;
    gbp->buffer[gbp->gap_start] = new_line;
    gbp->gap_start++;
  }
  else if (dir == AFTER)
  {
    if (line_index != gbp->gap_start - 1)
    {
      move_gap_page (
          gbp,
          gbp->buf_size - 1 - (gbp->gap_end - gbp->gap_start),
          GAP_START);
    }
    index_new_line = gbp->gap_start;
    gbp->buffer[gbp->gap_start] = new_line;
    gbp->gap_start++;
  }
  return index_new_line;
}

void
insert_in_gap_page (GapBufferPage *gbp, char *buffer_ptr, int count)
{
  GapBuffer gb = { gbp->buffer, gbp->gap_start, gbp->gap_end, gbp->buf_size };
  insert_in_gap (&gb, buffer_ptr, count, sizeof (GapBufferLine *));
  gbp->buffer = gb.buffer;
  gbp->buf_size = gb.buf_size;
  gbp->gap_start = gb.gap_start;
  gbp->gap_end = gb.gap_end;
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
// === Render Functions
// =============================================================================
/*
   [ lkjlkj___ljlkj ]
*/
int
render_string_from_page (GapBufferPage *gbp, char *buffer, int size)
{
  int line_count = 0;
  int pos = 0;
  for (int line = 0; line < gbp->buf_size; line++)
  {
    if (line < gbp->gap_start || line > gbp->gap_end)
    {
      for (int ch = 0; ch < gbp->buffer[line]->buf_size; ch++)
      {
        GapBufferLine *l = gbp->buffer[line];
        if (ch != l->buf_size - 1)
        {
          if (ch < l->gap_start || ch > l->gap_end)
          {
            buffer[pos] = l->buffer[ch];
            pos++;
          }
        }
        else
        {
          buffer[pos] = '\n';
          pos++;
          line_count++;
        }
      }
    }
  }
  buffer[pos] = '\0';
  return line_count;
}

typedef struct
{
  Vector2 page_pos;
  float width;
  float height;
} CursorProps;

CursorProps
render_cursor_pos_from_page (Cursor c, GapBufferPage page, Font font)
{
  Vector2 pos = { 0 };
  if (c.line < page.gap_start)
  {
    pos.y = c.line * (font.baseSize + 3);
  }
  else
  {
    pos.y
        = (c.line - (page.gap_end - page.gap_start + 1)) * (font.baseSize + 3);
  }
  int last_size = 0;
  for (int ch = 0; ch <= c.pos; ch++)
  {
    if ((ch < page.buffer[c.line]->gap_start
         || ch > page.buffer[c.line]->gap_end))
    {
      int glyph = page.buffer[c.line]->buffer[ch];
      int glyph_index = glyph - 32;
      pos.x += last_size;
      last_size = font.glyphs[glyph_index].advanceX + 2;
    }
  }

  CursorProps result;

  result.page_pos = pos;
  result.width = (float)last_size;
  result.height = (float)font.baseSize;

  return result;
}

// =============================================================================
// === main
// =============================================================================

int
main (void)
{

  // === Initialization
  // ========================================================

  // Raylib -- init
  const int screen_width = 400;
  const int screen_height = 400;

  InitWindow (screen_width, screen_height, "NeoNote");

  SetTargetFPS (60);

  // Raylib -- fonts
  // TODO: Add multiple fonts and find out how to handle dynamic line spacing
  Font font_ttf = LoadFontEx ("fonts/jpos_sans_serif_regular.ttf", 13, 0, 94);
  SetTextLineSpacing (16);

  // Page Buffer

  int page_gap_start = 0;
  int line_gap_start = 0;

  GapBufferLine *current_line = init_gap_buffer_line (INIT_SIZE_LINE, GAP_SIZE);
  for (int i = 0; i < current_line->buf_size; i++)
  {
    current_line->buffer[i] = 'A';
  }

  GapBufferPage *page = init_gap_buffer_page (INIT_SIZE_PAGE, GAP_SIZE);

  page->buffer[page->gap_end + 1] = current_line;

  // Init Cursor
  assert (page->gap_end + 1 < page->buf_size);
  assert (current_line->gap_end + 1 < current_line->buf_size);
  Cursor *cursor = init_cursor (page->gap_end + 1, current_line->gap_end + 1);

  // Debug
  char debugTextBuffer[8192] = { 0 };
  char textBuffer[1024] = { 0 };
  double last_time = GetTime ();
  double curr_time;

  // === Main Loop
  // ===========================================================================
  while (!WindowShouldClose ())
  {
    curr_time = GetTime ();
    // Upadate
    current_line = page->buffer[cursor->line];
    int _char = GetCharPressed ();
    int key = GetKeyPressed ();

    while (_char > 0)
    {
      if ((_char >= 32) && (_char <= 125))
      {
        if (cursor->pos == current_line->gap_end + 1)
        {
          /* NOTE: Do this because gap could be expanded, messing up the cursor
             position. Maybe handle this automaticly inside insert_single_char?
          */
          insert_single_char (current_line, _char, GAP_START);
          cursor->pos = current_line->gap_end + 1;
        }
        else if (cursor->pos < current_line->gap_start)
        {
          move_gap_line (current_line, cursor->pos, GAP_START);
          insert_single_char (current_line, _char, GAP_START);
          /* NOTE: Do this because gap could be expanded, messing up the cursor
             position. Maybe handle this automaticly inside insert_single_char?
          */
          cursor->pos = current_line->gap_end + 1;
        }
        else
        {
          move_gap_line (current_line, cursor->pos - 1, GAP_END);
          insert_single_char (current_line, _char, GAP_START);
          /* NOTE: Do this because gap could be expanded, messing up the cursor
             position. Maybe handle this automaticly inside insert_single_char?
          */
          cursor->pos = current_line->gap_end + 1;
        }
      }

      _char = GetCharPressed ();
    }
    while (key > 0)
    {
      if (key == KEY_UP)
      {
        if (move_cursor_previous_line (cursor, page) == 0)
        {
          int last_pos = cursor->pos;
          int new_pos = last_pos;

          if (last_pos > current_line->gap_end)
            new_pos = last_pos
                      - (current_line->gap_end - current_line->gap_start + 1);

          current_line = page->buffer[cursor->line];

          if (new_pos >= current_line->gap_start)
            new_pos = new_pos
                      + (current_line->gap_end - current_line->gap_start + 1);

          move_cursor (cursor, page, new_pos);
        }
      }

      if (key == KEY_DOWN)
      {
        if (move_cursor_next_line (cursor, page) == 0)
        {
          int last_pos = cursor->pos;
          int new_pos = last_pos;

          if (last_pos > current_line->gap_end)
            new_pos = last_pos
                      - (current_line->gap_end - current_line->gap_start + 1);

          current_line = page->buffer[cursor->line];

          if (new_pos >= current_line->gap_start)
            new_pos = new_pos
                      + (current_line->gap_end - current_line->gap_start + 1);

          move_cursor (cursor, page, new_pos);
          // NEXT
        }
      }

      if (key == KEY_RIGHT)
      {
        if (move_cursor (cursor, page, cursor->pos + 1) == OUTSIDE_RIGHT)
        {
          if (move_cursor_next_line (cursor, page) == 0)
          {
            current_line = page->buffer[cursor->line];
            move_cursor (cursor, page, 0);
          }
        }
      }

      if (key == KEY_LEFT)
      {
        PositionInGapArray state = move_cursor (cursor, page, cursor->pos - 1);
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
          if (cursor->pos == current_line->gap_end + 1)
          {
            delete_single_char (current_line, GAP_START);
          }
          else if (cursor->pos < current_line->gap_start)
          {
            move_gap_line (current_line, cursor->pos - 1, GAP_START);
            delete_single_char (current_line, GAP_END);
            move_cursor (cursor, page, current_line->gap_end + 1);
          }
          else if (cursor->pos > current_line->gap_end)
          {
            move_gap_line (current_line, cursor->pos - 1, GAP_END);
            delete_single_char (current_line, GAP_START);
            move_cursor (cursor, page, current_line->gap_end + 1);
          }
        }
        else if (cursor->line != 0)
        {
          // NOTE: CONTINUE FROM HERE
          GapBufferLine *previous_line = page->buffer[cursor->line - 1];
          // move cursor->pos to the end of the previous line
          cursor->pos = previous_line->buf_size - 1;

          // copy current_line to end of privious line
          for (int i = 0; i < current_line->buf_size - 2; i++)
          {
            if (i < current_line->gap_start || i > current_line->gap_end)
              insert_single_char (
                  previous_line,
                  current_line->buffer[i],
                  GAP_START);
          }
          // delete current_line
          move_gap_page (page, cursor->line + 1, GAP_START);
          delete_single_line (page);
          cursor->line--;
          cursor->pos--;
        }
      }

      if (key == KEY_ENTER)
      {
        GapBufferLine *new_line
            = init_gap_buffer_line (INIT_SIZE_LINE, GAP_SIZE);

        // NOTE: Only for debug print
        for (int i = 0; i < new_line->buf_size; i++)
        {
          new_line->buffer[i] = 'A';
        }

        // Cursor on start of the line
        if (cursor->pos == 0)
        {
          int index_new_line
              = insert_single_line (page, new_line, cursor->line, BEFORE);
          cursor->line = index_new_line;
          cursor->pos = new_line->gap_end + 1;
        }
        // Cursor at end of the line
        // (this works because the line should always have padding on the end)
        else if (cursor->pos == current_line->buf_size - 1)
        {
          int index_new_line
              = insert_single_line (page, new_line, cursor->line, AFTER);
          cursor->line = index_new_line;
          cursor->pos = new_line->gap_end + 1;
        }
        // Cursor in the middle of the line
        else
        {
          move_gap_line (new_line, 0, GAP_START);
          for (int i = cursor->pos; i < current_line->buf_size - 2; i++)
          {
            // TODO: Make a functon to insert more than one char
            //       Maybe copy hole memory area?
            if (i < current_line->gap_start || i > current_line->gap_end)
              insert_single_char (new_line, current_line->buffer[i], GAP_START);
          }
          current_line->gap_start = cursor->pos;
          current_line->gap_end = current_line->buf_size - 2;

          move_gap_page (page, cursor->line + 1, GAP_START);
          int index_new_line
              = insert_single_line (page, new_line, page->gap_start, AFTER);
          cursor->line = index_new_line;
          cursor->pos = 0;
        }
        current_line = page->buffer[cursor->line];
      }

      key = GetKeyPressed ();
    }

    // Draw
    BeginDrawing ();

    ClearBackground (RAYWHITE);

    int line_count = render_string_from_page (page, textBuffer, 1024);
    CursorProps cursor_pos
        = render_cursor_pos_from_page (*cursor, *page, font_ttf);
    // TODO: Check if I can simply add two Vector2's
    Vector2 padding = { 20.0f, 20.0f };
    cursor_pos.page_pos.x = cursor_pos.page_pos.x + padding.x;
    cursor_pos.page_pos.y = cursor_pos.page_pos.y + padding.y;

    DrawTextEx (
        font_ttf,
        textBuffer,
        padding,
        (float)font_ttf.baseSize,
        2,
        MAROON);

    if (curr_time - last_time > 0.5f)
    {
      DrawRectangleV (
          cursor_pos.page_pos,
          (Vector2){ cursor_pos.width, cursor_pos.height },
          GREEN);
      if (curr_time - last_time > 1.0f)
        last_time = curr_time;
    }

    render_page_debug (page, debugTextBuffer, 8192, *cursor);
    int debug_offset = line_count * font_ttf.baseSize + 20;
    DrawText (debugTextBuffer, 10, debug_offset + 90, 10, DARKGRAY);
    DrawText (
        TextFormat ("pos: %d", cursor->pos),
        10,
        debug_offset + 10,
        10,
        DARKGRAY);
    DrawText (
        TextFormat ("line: %d", cursor->line),
        10,
        debug_offset + 20,
        10,
        DARKGRAY);
    DrawText (
        TextFormat ("page->gap_start: %d", page->gap_start),
        10,
        debug_offset + 30,
        10,
        DARKGRAY);
    DrawText (
        TextFormat ("page->gap_end: %d", page->gap_end),
        10,
        debug_offset + 40,
        10,
        DARKGRAY);
    DrawText (
        TextFormat ("page->buf_size: %d", current_line->buf_size),
        10,
        debug_offset + 50,
        10,
        DARKGRAY);
    DrawText (
        TextFormat ("current_line->gap_start: %d", current_line->gap_start),
        10,
        debug_offset + 60,
        10,
        DARKGRAY);
    DrawText (
        TextFormat ("current_line->gap_end: %d", current_line->gap_end),
        10,
        debug_offset + 70,
        10,
        DARKGRAY);
    DrawText (
        TextFormat ("current_line->buf_size: %d", current_line->buf_size),
        10,
        debug_offset + 80,
        10,
        DARKGRAY);

    EndDrawing ();
  }

  // === De-Initialization
  // ===========================================================================
  CloseWindow ();
  return 0;
}
