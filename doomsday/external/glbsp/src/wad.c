//------------------------------------------------------------------------
// WAD : WAD read/write functions.
//------------------------------------------------------------------------
//
//  GL-Friendly Node Builder (C) 2000-2007 Andrew Apted
//
//  Based on 'BSP 2.3' by Colin Reed, Lee Killough and others.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//------------------------------------------------------------------------

#include "system.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>
#include <errno.h>

#include <zlib.h>

#include "blockmap.h"
#include "level.h"
#include "node.h"
#include "seg.h"
#include "structs.h"
#include "util.h"
#include "wad.h"


static FILE *in_file = NULL;
static FILE *out_file = NULL;


#define DEBUG_DIR   0
#define DEBUG_LUMP  0
#define DEBUG_KEYS  0

#define APPEND_BLKSIZE  256
#define LEVNAME_BUNCH   20

#define ALIGN_LEN(len)  ((((len) + 3) / 4) * 4)


// current wad info
static wad_t wad;


/* ---------------------------------------------------------------- */


#define NUM_LEVEL_LUMPS  12
#define NUM_GL_LUMPS     5

static const char *level_lumps[NUM_LEVEL_LUMPS]=
{
  "THINGS", "LINEDEFS", "SIDEDEFS", "VERTEXES", "SEGS",
  "SSECTORS", "NODES", "SECTORS", "REJECT", "BLOCKMAP",
  "BEHAVIOR",  // <-- hexen support
  "SCRIPTS"  // -JL- Lump with script sources
};

static const char *gl_lumps[NUM_GL_LUMPS]=
{
  "GL_VERT", "GL_SEGS", "GL_SSECT", "GL_NODES",
  "GL_PVS"  // -JL- PVS (Potentially Visible Set) lump
};

static const char align_filler[4] = { 0, 0, 0, 0 };


//
// CheckMagic
//
static int CheckMagic(const char type[4])
{
  if ((type[0] == 'I' || type[0] == 'P') &&
       type[1] == 'W' && type[2] == 'A' && type[3] == 'D')
  {
    return TRUE;
  }

  return FALSE;
}


//
// CheckLevelName
//
static int CheckLevelName(const char *name)
{
  int n;

  for (n=0; n < wad.num_level_names; n++)
  {
    if (strcmp(wad.level_names[n], name) == 0)
      return TRUE;
  }

  return FALSE;
}


//
// CheckLevelLumpName
//
// Tests if the entry name is one of the level lumps.
// Returns index after header (1..N), or zero if no match.
//
static int CheckLevelLumpName(const char *name)
{
  int i;

  for (i=0; i < NUM_LEVEL_LUMPS; i++)
  {
    if (strcmp(name, level_lumps[i]) == 0)
      return 1 + i;
  }

  return 0;
}


//
// CheckGLLumpName
//
// Tests if the entry name matches GL_ExMy or GL_MAPxx, or one of the
// GL lump names.
//
static int CheckGLLumpName(const char *name)
{
  int i;

  if (name[0] != 'G' || name[1] != 'L' || name[2] != '_')
    return FALSE;

  for (i=0; i < NUM_GL_LUMPS; i++)
  {
    if (strcmp(name, gl_lumps[i]) == 0)
      return TRUE;
  }

  return CheckLevelName(name+3);
}


//
// Level name helper
//
static INLINE_G void AddLevelName(const char *name)
{
  if ((wad.num_level_names % LEVNAME_BUNCH) == 0)
  {
    wad.level_names = (const char **) UtilRealloc((void *)wad.level_names,
        (wad.num_level_names + LEVNAME_BUNCH) * sizeof(const char *));
  }

  wad.level_names[wad.num_level_names] = UtilStrDup(name);
  wad.num_level_names++;
}


//
// NewLevel
//
// Create new level information
//
static level_t *NewLevel(int flags)
{
  level_t *cur;

  cur = UtilCalloc(sizeof(level_t));

  cur->flags = flags;

  return cur;
}


//
// NewLump
//
// Create new lump.  'name' must be allocated storage.
//
static lump_t *NewLump(char *name)
{
  lump_t *cur;

  cur = UtilCalloc(sizeof(lump_t));

  cur->name = name;
  cur->start = cur->new_start = -1;
  cur->flags = LUMP_NEW;
  cur->length = 0;
  cur->space = 0;
  cur->data = NULL;
  cur->lev_info = NULL;

  return cur;
}


static void FreeLump(lump_t *lump);

//
// FreeWadLevel
//
static void FreeWadLevel(level_t *level)
{
  while (level->children)
  {
    lump_t *head = level->children;
    level->children = head->next;

    // the ol' recursion trick... :)
    FreeLump(head);
  }

  UtilFree(level);
}

//
// FreeLump
//
static void FreeLump(lump_t *lump)
{
  // free level lumps, if any
  if (lump->lev_info)
  {
    FreeWadLevel(lump->lev_info);
  }

  // check 'data' here, since it gets freed in WriteLumpData()
  if (lump->data)
    UtilFree(lump->data);

  UtilFree(lump->name);
  UtilFree(lump);
}


//
// ReadHeader
//
// Returns TRUE if successful, or FALSE if there was a problem (in
// which case the error message as been setup).
//
static int ReadHeader(const char *filename)
{
  size_t len;
  raw_wad_header_t header;

  len = fread(&header, sizeof(header), 1, in_file);

  if (len != 1)
  {
    SetErrorMsg("Trouble reading wad header for %s [%s]", 
      filename, strerror(errno));

    return FALSE;
  }

  if (! CheckMagic(header.type))
  {
    SetErrorMsg("%s does not appear to be a wad file (bad magic)", 
        filename);

    return FALSE;
  }

  wad.kind = (header.type[0] == 'I') ? IWAD : PWAD;

  wad.num_entries = UINT32(header.num_entries);
  wad.dir_start   = UINT32(header.dir_start);

  // initialise stuff
  wad.dir_head = NULL;
  wad.dir_tail = NULL;
  wad.current_level = NULL;
  wad.level_names = NULL;
  wad.num_level_names = 0;

  return TRUE;
}


//
// ReadDirEntry
//
static void ReadDirEntry(void)
{
  size_t len;
  raw_wad_entry_t entry;
  lump_t *lump;

  DisplayTicker();

  len = fread(&entry, sizeof(entry), 1, in_file);

  if (len != 1)
    FatalError("Trouble reading wad directory");

  lump = NewLump(UtilStrNDup(entry.name, 8));

  lump->start  = UINT32(entry.start);
  lump->length = UINT32(entry.length);

# if DEBUG_DIR
  PrintDebug("Read dir... %s\n", lump->name);
# endif

  // link it in
  lump->next = NULL;
  lump->prev = wad.dir_tail;

  if (wad.dir_tail)
    wad.dir_tail->next = lump;
  else
    wad.dir_head = lump;

  wad.dir_tail = lump;
}

//
// DetermineLevelNames
//
static void DetermineLevelNames(void)
{
  lump_t *L, *N;
  int i;

  for (L=wad.dir_head; L; L=L->next)
  {
    int matched = 0;

    // skip known lumps (these are never valid level names)
    if (CheckLevelLumpName(L->name))
      continue;

    // check if the next four lumps after the current lump match the
    // level-lump names. Order doesn't matter, but repeats do.
    for (i=0, N=L->next; (i < 4) && N; i++, N=N->next)
    {
      int idx = CheckLevelLumpName(N->name);

      if (!idx || idx > 8 /* SECTORS */ || (matched & (1<<idx)))
        break;

      matched |= (1<<idx);
    }

    if (i != 4)
      continue;

#   if DEBUG_DIR
    PrintDebug("Found level name: %s\n", L->name);
#   endif

    // check for duplicate levels (ignored)
    if (CheckLevelName(L->name))
    {
      PrintWarn("Level name '%s' found twice in wad - Skipped\n", L->name);
      continue;
    }

    // check for long names
    if (strlen(L->name) > 5)
      PrintWarn("Long level name '%s' found in wad\n", L->name);

    AddLevelName(L->name);
  }
}

//
// ProcessDirEntry
//
static void ProcessDirEntry(lump_t *lump)
{
  DisplayTicker();

  // ignore previous GL lump info
  if (CheckGLLumpName(lump->name))
  {
#   if DEBUG_DIR
    PrintDebug("Discarding previous GL info: %s\n", lump->name);
#   endif

    FreeLump(lump);
    wad.num_entries--;

    return;
  }

  // mark the lump as 'ignorable' when in GWA mode.
  if (cur_info->gwa_mode)
    lump->flags |= LUMP_IGNORE_ME;

  // --- LEVEL MARKERS ---

  if (CheckLevelName(lump->name))
  {
    /* NOTE !  Level marks can have data (in Hexen anyway) */
    if (cur_info->load_all)
      lump->flags |= LUMP_READ_ME;
    else
      lump->flags |= LUMP_COPY_ME;

    // OK, start a new level

    lump->lev_info = NewLevel(0);

    wad.current_level = lump;

#   if DEBUG_DIR
    PrintDebug("Process dir... %s :\n", lump->name);
#   endif

    // link it in
    lump->next = NULL;
    lump->prev = wad.dir_tail;

    if (wad.dir_tail)
      wad.dir_tail->next = lump;
    else
      wad.dir_head = lump;

    wad.dir_tail = lump;

    return;
  }

  // --- LEVEL LUMPS ---

  if (wad.current_level)
  {
    if (CheckLevelLumpName(lump->name))
    {
      // check for duplicates
      if (FindLevelLump(lump->name))
      {
        PrintWarn("Duplicate entry '%s' ignored in %s\n",
            lump->name, wad.current_level->name);

        FreeLump(lump);
        wad.num_entries--;

        return;
      }

#     if DEBUG_DIR
      PrintDebug("Process dir... |--- %s\n", lump->name);
#     endif

      // mark it to be loaded
      lump->flags |= LUMP_READ_ME;

      // link it in
      lump->next = wad.current_level->lev_info->children;
      lump->prev = NULL;

      if (lump->next)
        lump->next->prev = lump;

      wad.current_level->lev_info->children = lump;
      return;
    }

    // OK, non-level lump.  End the previous level.

    wad.current_level = NULL;
  }

  // --- ORDINARY LUMPS ---

# if DEBUG_DIR
  PrintDebug("Process dir... %s\n", lump->name);
# endif

  if (CheckLevelLumpName(lump->name))
    PrintWarn("Level lump '%s' found outside any level\n", lump->name);

  // maybe load data
  if (cur_info->load_all)
    lump->flags |= LUMP_READ_ME;
  else
    lump->flags |= LUMP_COPY_ME;

  // link it in
  lump->next = NULL;
  lump->prev = wad.dir_tail;

  if (wad.dir_tail)
    wad.dir_tail->next = lump;
  else
    wad.dir_head = lump;

  wad.dir_tail = lump;
}

//
// ReadDirectory
//
static void ReadDirectory(void)
{
  int i;
  int total_entries = wad.num_entries;
  lump_t *prev_list;

  fseek(in_file, wad.dir_start, SEEK_SET);

  for (i=0; i < total_entries; i++)
  {
    ReadDirEntry();
  }

  DetermineLevelNames();

  // finally, unlink all lumps and process each one in turn

  prev_list = wad.dir_head;
  wad.dir_head = wad.dir_tail = NULL;

  while (prev_list)
  {
    lump_t *cur = prev_list;
    prev_list = cur->next;

    ProcessDirEntry(cur);
  }
}


//
// ReadLumpData
//
static void ReadLumpData(lump_t *lump)
{
  size_t len;

  cur_comms->file_pos++;
  DisplaySetBar(1, cur_comms->file_pos);
  DisplayTicker();

# if DEBUG_LUMP
  PrintDebug("Reading... %s (%d)\n", lump->name, lump->length);
# endif

  if (lump->length == 0)
    return;

  lump->data = UtilCalloc(lump->length);

  fseek(in_file, lump->start, SEEK_SET);

  len = fread(lump->data, lump->length, 1, in_file);

  if (len != 1)
  {
    if (wad.current_level)
      PrintWarn("Trouble reading lump '%s' in %s\n",
          lump->name, wad.current_level->name);
    else
      PrintWarn("Trouble reading lump '%s'\n", lump->name);
  }

  lump->flags &= ~LUMP_READ_ME;
}


//
// ReadAllLumps
//
// Returns number of entries read.
//
static int ReadAllLumps(void)
{
  lump_t *cur, *L;
  int count = 0;

  for (cur=wad.dir_head; cur; cur=cur->next)
  {
    count++;

    if (cur->flags & LUMP_READ_ME)
      ReadLumpData(cur);

    if (cur->lev_info && ! (cur->lev_info->flags & LEVEL_IS_GL))
    {
      for (L=cur->lev_info->children; L; L=L->next)
      {
        count++;

        if (L->flags & LUMP_READ_ME)
          ReadLumpData(L);
      }
    }
  }

  return count;
}


//
// CountLumpTypes
//
// Returns number of entries with matching flags.  Used to predict how
// many ReadLumpData() or WriteLumpData() calls will be made (for the
// progress bars).
//
static int CountLumpTypes(int flag_mask, int flag_match)
{
  lump_t *cur, *L;
  int count = 0;

  for (cur=wad.dir_head; cur; cur=cur->next)
  {
    if ((cur->flags & flag_mask) == flag_match)
      count++;

    if (cur->lev_info)
    {
      for (L=cur->lev_info->children; L; L=L->next)
        if ((L->flags & flag_mask) == flag_match)
          count++;
    }
  }

  return count;
}


/* ---------------------------------------------------------------- */


//
// WriteHeader
//
static void WriteHeader(void)
{
  size_t len;
  raw_wad_header_t header;

  switch (wad.kind)
  {
    case IWAD:
      strncpy(header.type, "IWAD", 4);
      break;

    case PWAD:
      strncpy(header.type, "PWAD", 4);
      break;
  }

  header.num_entries = UINT32(wad.num_entries);
  header.dir_start   = UINT32(wad.dir_start);

  len = fwrite(&header, sizeof(header), 1, out_file);

  if (len != 1)
    PrintWarn("Trouble writing wad header\n");
}


//
// CreateGLMarker
//
lump_t *CreateGLMarker(void)
{
  lump_t *level = wad.current_level;
  lump_t *cur;

  char name_buf[32];
  boolean_g long_name = FALSE;

  if (strlen(level->name) <= 5)
  {
    sprintf(name_buf, "GL_%s", level->name);
  }
  else
  {
    // support for level names longer than 5 letters
    strcpy(name_buf, "GL_LEVEL");
    long_name = TRUE;
  }

  cur = NewLump(UtilStrDup(name_buf));

  cur->lev_info = NewLevel(LEVEL_IS_GL);

  // link it in
  cur->next = level->next;
  cur->prev = level;

  if (cur->next)
    cur->next->prev = cur;

  level->next = cur;
  level->lev_info->buddy = cur;

  if (long_name)
  {
    AddGLTextLine("LEVEL", level->name);
  }

  return cur;
}


//
// SortLumps
//
// Algorithm is pretty simple: for each of the names, if such a lump
// exists in the list, move it to the head of the list.  By going
// backwards through the names, we ensure the correct order.
//
static void SortLumps(lump_t ** list, const char **names, int count)
{
  int i;
  lump_t *cur;

  for (i=count-1; i >= 0; i--)
  {
    for (cur=(*list); cur; cur=cur->next)
    {
      if (strcmp(cur->name, names[i]) != 0)
        continue;

      // unlink it
      if (cur->next)
        cur->next->prev = cur->prev;

      if (cur->prev)
        cur->prev->next = cur->next;
      else
        (*list) = cur->next;

      // add to head
      cur->next = (*list);
      cur->prev = NULL;

      if (cur->next)
        cur->next->prev = cur;

      (*list) = cur;

      // continue with next name (important !)
      break;
    }
  }
}


//
// RecomputeDirectory
//
// Calculates all the lump offsets for the directory.
//
static void RecomputeDirectory(void)
{
  lump_t *cur, *L;
  level_t *lev;

  wad.num_entries = 0;
  wad.dir_start = sizeof(raw_wad_header_t);

  // run through all the lumps, computing the 'new_start' fields, the
  // number of lumps in the directory, the directory starting pos, and
  // also sorting the lumps in the levels.

  for (cur=wad.dir_head; cur; cur=cur->next)
  {
    if (cur->flags & LUMP_IGNORE_ME)
      continue;

    cur->new_start = wad.dir_start;

    wad.dir_start += ALIGN_LEN(cur->length);
    wad.num_entries++;

    lev = cur->lev_info;

    if (lev)
    {
      if (lev->flags & LEVEL_IS_GL)
        SortLumps(&lev->children, gl_lumps, NUM_GL_LUMPS);
      else
        SortLumps(&lev->children, level_lumps, NUM_LEVEL_LUMPS);

      for (L=lev->children; L; L=L->next)
      {
        if (L->flags & LUMP_IGNORE_ME)
          continue;

        L->new_start = wad.dir_start;

        wad.dir_start += ALIGN_LEN(L->length);
        wad.num_entries++;
      }
    }
  }
}


//
// WriteLumpData
//
static void WriteLumpData(lump_t *lump)
{
  size_t len;
  int align_size;

  cur_comms->file_pos++;
  DisplaySetBar(1, cur_comms->file_pos);
  DisplayTicker();

# if DEBUG_LUMP
  if (lump->flags & LUMP_COPY_ME)
    PrintDebug("Copying... %s (%d)\n", lump->name, lump->length);
  else
    PrintDebug("Writing... %s (%d)\n", lump->name, lump->length);
# endif

  if (ftell(out_file) != lump->new_start)
    PrintWarn("Consistency failure writing %s (%08lX, %08X\n", 
      lump->name, ftell(out_file), lump->new_start);

  if (lump->length == 0)
    return;

  if (lump->flags & LUMP_COPY_ME)
  {
    lump->data = UtilCalloc(lump->length);

    fseek(in_file, lump->start, SEEK_SET);

    len = fread(lump->data, lump->length, 1, in_file);

    if (len != 1)
      PrintWarn("Trouble reading lump %s to copy\n", lump->name);
  }

  len = fwrite(lump->data, lump->length, 1, out_file);

  if (len != 1)
    PrintWarn("Trouble writing lump %s\n", lump->name);

  align_size = ALIGN_LEN(lump->length) - lump->length;

  if (align_size > 0)
    fwrite(align_filler, align_size, 1, out_file);

  UtilFree(lump->data);

  lump->data = NULL;
}


//
// WriteAllLumps
//
// Returns number of entries written.
//
static int WriteAllLumps(void)
{
  lump_t *cur, *L;
  int count = 0;

  for (cur=wad.dir_head; cur; cur=cur->next)
  {
    if (cur->flags & LUMP_IGNORE_ME)
      continue;

    WriteLumpData(cur);
    count++;

    if (cur->lev_info)
    {
      for (L=cur->lev_info->children; L; L=L->next)
      {
        if (L->flags & LUMP_IGNORE_ME)
          continue;

        WriteLumpData(L);
        count++;
      }
    }
  }

  fflush(out_file);

  return count;
}


//
// WriteDirEntry
//
static void WriteDirEntry(lump_t *lump)
{
  size_t len;
  raw_wad_entry_t entry;

  DisplayTicker();

  strncpy(entry.name, lump->name, 8);

  entry.start  = UINT32(lump->new_start);
  entry.length = UINT32(lump->length);

  len = fwrite(&entry, sizeof(entry), 1, out_file);

  if (len != 1)
    PrintWarn("Trouble writing wad directory\n");
}


//
// WriteDirectory
//
// Returns number of entries written.
//
static int WriteDirectory(void)
{
  lump_t *cur, *L;
  int count = 0;

  if (ftell(out_file) != wad.dir_start)
    PrintWarn("Consistency failure writing lump directory "
      "(%08lX,%08X)\n", ftell(out_file), wad.dir_start);

  for (cur=wad.dir_head; cur; cur=cur->next)
  {
    if (cur->flags & LUMP_IGNORE_ME)
      continue;

    WriteDirEntry(cur);
    count++;

#   if DEBUG_DIR
    if (cur->lev_info)
      PrintDebug("Write dir... %s :\n", cur->name);
    else
      PrintDebug("Write dir... %s\n", cur->name);
#   endif

    if (cur->lev_info)
    {
      for (L=cur->lev_info->children; L; L=L->next)
      {
        if (cur->flags & LUMP_IGNORE_ME)
          continue;

#       if DEBUG_DIR
        PrintDebug("Write dir... |--- %s\n", L->name);
#       endif

        WriteDirEntry(L);
        count++;
      }
    }
  }

  fflush(out_file);

  return count;
}


/* ---------------------------------------------------------------- */


//
// CheckExtension
//
int CheckExtension(const char *filename, const char *ext)
{
  int A = (int)strlen(filename) - 1;
  int B = (int)strlen(ext) - 1;

  for (; B >= 0; B--, A--)
  {
    if (A < 0)
      return FALSE;

    if (toupper(filename[A]) != toupper(ext[B]))
      return FALSE;
  }

  return (A >= 1) && (filename[A] == '.');
}

//
// ReplaceExtension
//
char *ReplaceExtension(const char *filename, const char *ext)
{
  char *dot_pos;
  char buffer[512];

  strcpy(buffer, filename);

  dot_pos = strrchr(buffer, '.');

  if (dot_pos)
    dot_pos[1] = 0;
  else
    strcat(buffer, ".");

  strcat(buffer, ext);

  return UtilStrDup(buffer);
}


//
// CreateLevelLump
//
lump_t *CreateLevelLump(const char *name)
{
  lump_t *cur;

# if DEBUG_DIR
  PrintDebug("Create Lump... %s\n", name);
# endif

  // already exists ?
  for (cur=wad.current_level->lev_info->children; cur; cur=cur->next)
  {
    if (strcmp(name, cur->name) == 0)
      break;
  }

  if (cur)
  {
    if (cur->data)
      UtilFree(cur->data);

    cur->data = NULL;
    cur->length = 0;
    cur->space  = 0;

    return cur;
  }

  // nope, allocate new one
  cur = NewLump(UtilStrDup(name));

  // link it in
  cur->next = wad.current_level->lev_info->children;
  cur->prev = NULL;

  if (cur->next)
    cur->next->prev = cur;

  wad.current_level->lev_info->children = cur;

  return cur;
}


//
// CreateGLLump
//
lump_t *CreateGLLump(const char *name)
{
  lump_t *cur;
  lump_t *gl_level;

# if DEBUG_DIR
  PrintDebug("Create GL Lump... %s\n", name);
# endif

  // create GL level marker if necessary
  if (! wad.current_level->lev_info->buddy)
    CreateGLMarker();

  gl_level = wad.current_level->lev_info->buddy;

  // check if already exists
  for (cur=gl_level->lev_info->children; cur; cur=cur->next)
  {
    if (strcmp(name, cur->name) == 0)
      break;
  }

  if (cur)
  {
    if (cur->data)
      UtilFree(cur->data);

    cur->data = NULL;
    cur->length = 0;
    cur->space  = 0;

    return cur;
  }

  // nope, allocate new one
  cur = NewLump(UtilStrDup(name));

  // link it in
  cur->next = gl_level->lev_info->children;
  cur->prev = NULL;

  if (cur->next)
    cur->next->prev = cur;

  gl_level->lev_info->children = cur;

  return cur;
}


//
// AppendLevelLump
//
void AppendLevelLump(lump_t *lump, const void *data, int length)
{
  if (length == 0)
    return;

  if (lump->length == 0)
  {
    lump->space = MAX(length, APPEND_BLKSIZE);
    lump->data = UtilCalloc(lump->space);
  }
  else if (lump->space < length)
  {
    lump->space = MAX(length, APPEND_BLKSIZE);
    lump->data = UtilRealloc(lump->data, lump->length + lump->space);
  }

  memcpy(((char *)lump->data) + lump->length, data, length);

  lump->length += length;
  lump->space  -= length;
}


//
// AddGLTextLine
//
void AddGLTextLine(const char *keyword, const char *value)
{
  lump_t *gl_level;

  // create GL level marker if necessary
  if (! wad.current_level->lev_info->buddy)
    CreateGLMarker();

  gl_level = wad.current_level->lev_info->buddy;

# if DEBUG_KEYS
  PrintDebug("[%s] Adding: %s=%s\n", gl_level->name, keyword, value);
# endif

  AppendLevelLump(gl_level, keyword, strlen(keyword));
  AppendLevelLump(gl_level, "=", 1);

  AppendLevelLump(gl_level, value, strlen(value));
  AppendLevelLump(gl_level, "\n", 1);
}


//
// CountLevels
//
int CountLevels(void)
{
  lump_t *cur;
  int result = 0;

  for (cur=wad.dir_head; cur; cur=cur->next)
  {
    if (cur->lev_info && ! (cur->lev_info->flags & LEVEL_IS_GL))
      result++;
  }

  return result;
}

//
// FindNextLevel
//
int FindNextLevel(void)
{
  lump_t *cur;

  if (wad.current_level)
    cur = wad.current_level->next;
  else
    cur = wad.dir_head;

  while (cur && ! (cur->lev_info && ! (cur->lev_info->flags & LEVEL_IS_GL)))
    cur=cur->next;

  wad.current_level = cur;

  return (cur != NULL);
}

//
// GetLevelName
//
const char *GetLevelName(void)
{
  if (!wad.current_level)
    InternalError("GetLevelName: no current level");

  return wad.current_level->name;
}

//
// FindLevelLump
//
lump_t *FindLevelLump(const char *name)
{
  lump_t *cur = wad.current_level->lev_info->children;

  while (cur && (strcmp(cur->name, name) != 0))
    cur=cur->next;

  return cur;
}

//
// CheckLevelLumpZero
//
int CheckLevelLumpZero(lump_t *lump)
{
  int i;

  if (lump->length == 0)
    return TRUE;

  // ASSERT(lump->data)

  for (i=0; i < lump->length; i++)
  {
    if (((uint8_g *)lump->data)[i])
      return FALSE;
  }

  return TRUE;
}

//
// ReadWadFile
//
glbsp_ret_e ReadWadFile(const char *filename)
{
  int check;
  char *read_msg;

  // open input wad file & read header
  in_file = fopen(filename, "rb");

  if (! in_file)
  {
    if (errno == ENOENT)
      SetErrorMsg("Cannot open WAD file: %s", filename); 
    else
      SetErrorMsg("Cannot open WAD file: %s [%s]", filename, 
          strerror(errno));

    return GLBSP_E_ReadError;
  }

  if (! ReadHeader(filename))
  {
    fclose(in_file);
    return GLBSP_E_ReadError;
  }

  PrintMsg("Opened %cWAD file : %s\n", (wad.kind == IWAD) ? 'I' : 'P',
      filename);
  PrintVerbose("Reading %d dir entries at 0x%X\n", wad.num_entries,
      wad.dir_start);

  // read directory
  ReadDirectory();

  DisplayOpen(DIS_FILEPROGRESS);
  DisplaySetTitle("glBSP Reading Wad");

  read_msg = UtilFormat("Reading: %s", filename);

  DisplaySetBarText(1, read_msg);
  DisplaySetBarLimit(1, CountLumpTypes(LUMP_READ_ME, LUMP_READ_ME));
  DisplaySetBar(1, 0);

  UtilFree(read_msg);

  cur_comms->file_pos = 0;

  // now read lumps
  check = ReadAllLumps();

  if (check != wad.num_entries)
    InternalError("Read directory count consistency failure (%d,%d)",
      check, wad.num_entries);

  wad.current_level = NULL;

  DisplayClose();

  return GLBSP_E_OK;
}


//
// WriteWadFile
//
glbsp_ret_e WriteWadFile(const char *filename)
{
  int check1, check2;
  char *write_msg;

  PrintMsg("\n");
  PrintMsg("Saving WAD as %s\n", filename);

  if (cur_info->gwa_mode)
    wad.kind = PWAD;

  RecomputeDirectory();

  // create output wad file & write the header
  out_file = fopen(filename, "wb");

  if (! out_file)
  {
    SetErrorMsg("Cannot create WAD file: %s [%s]", filename,
        strerror(errno));

    return GLBSP_E_WriteError;
  }

  WriteHeader();

  DisplayOpen(DIS_FILEPROGRESS);
  DisplaySetTitle("glBSP Writing Wad");

  write_msg = UtilFormat("Writing: %s", filename);

  DisplaySetBarText(1, write_msg);
  DisplaySetBarLimit(1, CountLumpTypes(LUMP_IGNORE_ME, 0));
  DisplaySetBar(1, 0);

  UtilFree(write_msg);

  cur_comms->file_pos = 0;

  // now write all the lumps to the output wad
  check1 = WriteAllLumps();

  DisplayClose();

  // finally, write out the directory
  check2 = WriteDirectory();

  if (check1 != wad.num_entries || check2 != wad.num_entries)
    InternalError("Write directory count consistency failure (%d,%d,%d)",
      check1, check2, wad.num_entries);

  return GLBSP_E_OK;
}


//
// DeleteGwaFile
//
void DeleteGwaFile(const char *base_wad_name)
{
  char *gwa_file = ReplaceExtension(base_wad_name, "gwa");

  if (remove(gwa_file) == 0)
    PrintMsg("Deleted GWA file: %s\n", gwa_file);

  UtilFree(gwa_file);
}


//
// CloseWads
//
void CloseWads(void)
{
  int i;

  if (in_file)
  {
    fclose(in_file);
    in_file = NULL;
  }

  if (out_file)
  {
    fclose(out_file);
    out_file = NULL;
  }

  /* free directory entries */
  while (wad.dir_head)
  {
    lump_t *head = wad.dir_head;
    wad.dir_head = head->next;

    FreeLump(head);
  }

  /* free the level names */
  if (wad.level_names)
  {
    for (i=0; i < wad.num_level_names; i++)
      UtilFree((char *) wad.level_names[i]);

    UtilFree((void *)wad.level_names);
    wad.level_names = NULL;
  }
}


/* ---------------------------------------------------------------- */
/** Disabled all zlib/zdoom related functions, we don't support compressed nodes

static lump_t  *zout_lump;
static z_stream zout_stream;
static Bytef    zout_buffer[1024];

//
// ZLibBeginLump
//
void ZLibBeginLump(lump_t *lump)
{
  zout_lump = lump;

  zout_stream.zalloc = (alloc_func)0;
  zout_stream.zfree  = (free_func)0;
  zout_stream.opaque = (voidpf)0;

  if (Z_OK != deflateInit(&zout_stream, Z_DEFAULT_COMPRESSION))
    FatalError("Trouble setting up zlib compression");

  zout_stream.next_out  = zout_buffer;
  zout_stream.avail_out = sizeof(zout_buffer);
}

//
// ZLibAppendLump
//
void ZLibAppendLump(const void *data, int length)
{
  // ASSERT(zout_lump)
  // ASSERT(length > 0)

  zout_stream.next_in  = (Bytef*)data;   // const override
  zout_stream.avail_in = length;

  while (zout_stream.avail_in > 0)
  {
    int err = deflate(&zout_stream, Z_NO_FLUSH);

    if (err != Z_OK)
      FatalError("Trouble compressing %d bytes (zlib)\n", length);

    if (zout_stream.avail_out == 0)
    {
      AppendLevelLump(zout_lump, zout_buffer, sizeof(zout_buffer));

      zout_stream.next_out  = zout_buffer;
      zout_stream.avail_out = sizeof(zout_buffer);
    }
  }
}

//
// ZLibFinishLump
//
void ZLibFinishLump(void)
{
  int left_over;

  // ASSERT(zout_stream.avail_out > 0)

  zout_stream.next_in  = Z_NULL;
  zout_stream.avail_in = 0;

  for (;;)
  {
    int err = deflate(&zout_stream, Z_FINISH);

    if (err == Z_STREAM_END)
      break;

    if (err != Z_OK)
      FatalError("Trouble finishing compression (zlib)\n");

    if (zout_stream.avail_out == 0)
    {
      AppendLevelLump(zout_lump, zout_buffer, sizeof(zout_buffer));

      zout_stream.next_out  = zout_buffer;
      zout_stream.avail_out = sizeof(zout_buffer);
    }
  }

  left_over = sizeof(zout_buffer) - zout_stream.avail_out;

  if (left_over > 0)
    AppendLevelLump(zout_lump, zout_buffer, left_over);

  deflateEnd(&zout_stream);
  zout_lump = NULL;
}

*/

/* ---------------------------------------------------------------- */


//
// Mark failure routines
//
void MarkSoftFailure(int soft)
{
  wad.current_level->lev_info->soft_limit |= soft;
}

void MarkHardFailure(int hard)
{
  wad.current_level->lev_info->hard_limit |= hard;
}

void MarkV5Switch(int v5)
{
  wad.current_level->lev_info->v5_switch |= v5;
}

void MarkZDSwitch(void)
{
  level_t *lev = wad.current_level->lev_info;

  lev->v5_switch |= LIMIT_ZDBSP;

  lev->soft_limit &= ~ (LIMIT_VERTEXES);
  lev->hard_limit &= ~ (LIMIT_VERTEXES);
}


//
// ReportOneOverflow(
//
void ReportOneOverflow(const lump_t *lump, int limit, boolean_g hard)
{
  const char *msg = hard ? "overflowed the absolute limit" :
    "overflowed the original limit";

  PrintMsg("%-8s: ", lump->name);

  switch (limit)
  {
    case LIMIT_VERTEXES: PrintMsg("Number of Vertices %s.\n", msg); break;
    case LIMIT_SECTORS:  PrintMsg("Number of Sectors %s.\n", msg); break;
    case LIMIT_SIDEDEFS: PrintMsg("Number of Sidedefs %s\n", msg); break;
    case LIMIT_LINEDEFS: PrintMsg("Number of Linedefs %s\n", msg); break;

    case LIMIT_SEGS:     PrintMsg("Number of Segs %s.\n", msg); break;
    case LIMIT_SSECTORS: PrintMsg("Number of Subsectors %s.\n", msg); break;
    case LIMIT_NODES:    PrintMsg("Number of Nodes %s.\n", msg); break;

    case LIMIT_GL_VERT:  PrintMsg("Number of GL vertices %s.\n", msg); break;
    case LIMIT_GL_SEGS:  PrintMsg("Number of GL segs %s.\n", msg); break;
    case LIMIT_GL_SSECT: PrintMsg("Number of GL subsectors %s.\n", msg); break;
    case LIMIT_GL_NODES: PrintMsg("Number of GL nodes %s.\n", msg); break;

    case LIMIT_BAD_SIDE:   PrintMsg("One or more linedefs has a bad sidedef.\n"); break;
    case LIMIT_BMAP_TRUNC: PrintMsg("Blockmap area was too big - truncated.\n"); break;
    case LIMIT_BLOCKMAP:   PrintMsg("Blockmap lump %s.\n", msg); break;

    default:
      InternalError("UNKNOWN LIMIT BIT: 0x%06x", limit);
  }
}

// ReportOverflows
//
void ReportOverflows(boolean_g hard)
{
  lump_t *cur;

  if (hard)
  {
    PrintMsg(
      "ERRORS.  The following levels failed to be built, and won't\n"
      "work in any Doom port (and may even crash it).\n\n"
    );
  }
  else  // soft
  {
    PrintMsg(
      "POTENTIAL FAILURES.  The following levels should work in a\n"
      "modern Doom port, but may fail (or even crash) in older ports.\n\n"
    );
  }

  for (cur=wad.dir_head; cur; cur=cur->next)
  {
    level_t *lev = cur->lev_info;

    int limits, one_lim;

    if (! (lev && ! (lev->flags & LEVEL_IS_GL)))
      continue;

    limits = hard ? lev->hard_limit : lev->soft_limit;

    if (limits == 0)
      continue;

    for (one_lim = (1<<20); one_lim; one_lim >>= 1)
    {
      if (limits & one_lim)
        ReportOneOverflow(cur, one_lim, hard);
    }
  }
}

//
// ReportV5Switches
//
void ReportV5Switches(void)
{
  lump_t *cur;

  int saw_zdbsp = FALSE;

  PrintMsg(
    "V5 FORMAT UPGRADES.  The following levels require a Doom port\n"
    "which supports V5 GL-Nodes, otherwise they will fail (or crash).\n\n"
  );

  for (cur=wad.dir_head; cur; cur=cur->next)
  {
    level_t *lev = cur->lev_info;

    if (! (lev && ! (lev->flags & LEVEL_IS_GL)))
      continue;

    if (lev->v5_switch == 0)
      continue;

    if ((lev->v5_switch & LIMIT_ZDBSP) && ! saw_zdbsp)
    {
      PrintMsg("ZDBSP FORMAT has also been used for regular nodes.\n\n");
      saw_zdbsp = TRUE;
    }

    if (lev->v5_switch & LIMIT_VERTEXES)
    {
      PrintMsg("%-8s: Number of Vertices overflowed the limit.\n", cur->name);
    }

    if (lev->v5_switch & LIMIT_GL_SSECT)
    {
      PrintMsg("%-8s: Number of GL segs overflowed the limit.\n", cur->name);
    }
  }
}

//
// ReportFailedLevels
//
void ReportFailedLevels(void)
{
  lump_t *cur;
  int lev_count = 0;

  int fail_soft = 0;
  int fail_hard = 0;
  int fail_v5   = 0;

  boolean_g need_spacer = FALSE;

  for (cur=wad.dir_head; cur; cur=cur->next)
  {
    if (! (cur->lev_info && ! (cur->lev_info->flags & LEVEL_IS_GL)))
      continue;

    lev_count++;

    if (cur->lev_info->soft_limit != 0) fail_soft++;
    if (cur->lev_info->hard_limit != 0) fail_hard++;
    if (cur->lev_info->v5_switch  != 0) fail_v5++;
  }

  PrintMsg("\n");

  if (fail_soft + fail_hard + fail_v5 == 0)
  {
    PrintMsg("All levels were built successfully.\n");
    return;
  }

  PrintMsg("*** Problem Report ***\n\n");

  if (fail_soft > 0)
  {
    ReportOverflows(FALSE);
    need_spacer = TRUE;
  }

  if (fail_v5 > 0)
  {
    if (need_spacer)
      PrintMsg("\n");

    ReportV5Switches();
    need_spacer = TRUE;
  }

  if (fail_hard > 0)
  {
    if (need_spacer)
      PrintMsg("\n");

    ReportOverflows(TRUE);
  }

  PrintMsg("\nEnd of problem report.\n");
}

