//------------------------------------------------------------------------
// MAIN : Main program for glBSP
//------------------------------------------------------------------------
//
//  GL-Friendly Node Builder (C) 2000-2002 Andrew Apted
//
//  Based on `BSP 2.3' by Colin Reed, Lee Killough and others.
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
#include <assert.h>

#include "blockmap.h"
#include "level.h"
#include "node.h"
#include "seg.h"
#include "structs.h"
#include "util.h"
#include "wad.h"


const nodebuildinfo_t *cur_info = NULL;
const nodebuildfuncs_t *cur_funcs = NULL;
volatile nodebuildcomms_t *cur_comms = NULL;

int cur_build_pos;
int cur_file_pos;

static char glbsp_message_buf[1024];


const nodebuildinfo_t default_buildinfo =
{
  NULL,    // input_file
  NULL,    // output_file

  DEFAULT_FACTOR,  // factor

  FALSE,   // no_reject
  FALSE,   // no_progress
  FALSE,   // mini_warnings
  FALSE,   // force_hexen
  FALSE,   // pack_sides
  FALSE,   // v1_vert

  FALSE,   // load_all
  FALSE,   // no_gl
  FALSE,   // no_normal
  FALSE,   // force_normal
  FALSE,   // gwa_mode
  FALSE,   // keep_sect
  FALSE,   // no_prune

  DEFAULT_BLOCK_LIMIT,   // block_limit

  FALSE,   // missing_output
  FALSE    // same_filenames
};

const nodebuildcomms_t default_buildcomms =
{
  NULL,   // message
  FALSE   // cancelled
};


/* ----- option parsing ----------------------------- */

static void SetMessage(const char *str)
{
  GlbspFree(cur_comms->message);

  cur_comms->message = GlbspStrDup(str);
}

#define HANDLE_BOOLEAN(name, field)  \
    if (StrCaseCmp(opt_str, name) == 0)  \
    {  \
      info->field = TRUE;  \
      argv++; argc--;  \
      continue;  \
    }

glbsp_ret_e GlbspParseArgs(nodebuildinfo_t *info, 
    volatile nodebuildcomms_t *comms,
    const char ** argv, int argc)
{
  const char *opt_str;
  int num_files = 0;

  cur_comms = comms;
  SetMessage(NULL);

  while (argc > 0)
  {
    if (argv[0][0] != '-')
    {
      // --- ORDINARY FILENAME ---

      if (num_files >= 1)
      {
        SetMessage("Too many filenames.  Use the -o option");
        cur_comms = NULL;
        return GLBSP_E_BadArgs;
      }

      GlbspFree(info->input_file);
      info->input_file = GlbspStrDup(argv[0]);
      num_files++;

      argv++; argc--;
      continue;
    }

    // --- AN OPTION ---

    opt_str = &argv[0][1];

    // handle GNU style options beginning with `--'
    if (opt_str[0] == '-')
      opt_str++;

    if (StrCaseCmp(opt_str, "o") == 0)
    {
      if (argc < 2)
      {
        SetMessage("Missing filename for the -o option");
        cur_comms = NULL;
        return GLBSP_E_BadArgs;
      }

      GlbspFree(info->output_file);
      info->output_file = GlbspStrDup(argv[1]);

      argv += 2; argc -= 2;
      continue;
    }

    if (StrCaseCmp(opt_str, "factor") == 0)
    {
      if (argc < 2)
      {
        SetMessage("Missing factor value");
        cur_comms = NULL;
        return GLBSP_E_BadArgs;
      }

      info->factor = (int) strtol(argv[1], NULL, 10);

      argv += 2; argc -= 2;
      continue;
    }

    if (StrCaseCmp(opt_str, "maxblock") == 0)
    {
      if (argc < 2)
      {
        SetMessage("Missing maxblock value");
        cur_comms = NULL;
        return GLBSP_E_BadArgs;
      }

      info->block_limit = (int) strtol(argv[1], NULL, 10);

      argv += 2; argc -= 2;
      continue;
    }

    HANDLE_BOOLEAN("noreject",    no_reject)
    HANDLE_BOOLEAN("noprog",      no_progress)
    HANDLE_BOOLEAN("warn",        mini_warnings)
    HANDLE_BOOLEAN("packsides",   pack_sides)
    HANDLE_BOOLEAN("v1",          v1_vert)

    HANDLE_BOOLEAN("loadall",     load_all)
    HANDLE_BOOLEAN("nogl",        no_gl)
    HANDLE_BOOLEAN("nonormal",    no_normal)
    HANDLE_BOOLEAN("forcenormal", force_normal)
    HANDLE_BOOLEAN("forcegwa",    gwa_mode)
    HANDLE_BOOLEAN("keepsect",    keep_sect)
    HANDLE_BOOLEAN("noprune",     no_prune)

    // The -hexen option is only kept for backwards compat.
    HANDLE_BOOLEAN("hexen",       force_hexen)

    sprintf(glbsp_message_buf, "Unknown option: %s", argv[0]);
    SetMessage(glbsp_message_buf);

    cur_comms = NULL;
    return GLBSP_E_BadArgs;
  }

  cur_comms = NULL;
  return GLBSP_E_OK;
}

glbsp_ret_e GlbspCheckInfo(nodebuildinfo_t *info,
    volatile nodebuildcomms_t *comms)
{
  cur_comms = comms;
  SetMessage(NULL);

  info->same_filenames = FALSE;
  info->missing_output = FALSE;

  if (!info->input_file || info->input_file[0] == 0)
  {
    SetMessage("Missing input filename !");
    return GLBSP_E_BadArgs;
  }

  if (!info->output_file || info->output_file[0] == 0)
  {
    GlbspFree(info->output_file);
    info->output_file = GlbspStrDup(ReplaceExtension(
          info->input_file, "gwa"));

    info->gwa_mode = 1;
    info->missing_output = TRUE;
  }
  else if (CheckExtension(info->output_file, "gwa"))
  {
    info->gwa_mode = 1;
  }

  if (StrCaseCmp(info->input_file, info->output_file) == 0)
  {
    info->load_all = 1;
    info->same_filenames = TRUE;
  }

  if (info->no_prune && info->pack_sides)
  {
    info->pack_sides = FALSE;
    SetMessage("-noprune and -packsides cannot be used together");
    return GLBSP_E_BadInfoFixed;
  }

  if (info->gwa_mode && info->no_gl)
  {
    info->no_gl = FALSE;
    SetMessage("-nogl with GWA file: nothing to do !");
    return GLBSP_E_BadInfoFixed;
  }
 
  if (info->gwa_mode && info->force_normal)
  {
    info->force_normal = FALSE;
    SetMessage("-forcenormal used, but GWA files don't have normal nodes");
    return GLBSP_E_BadInfoFixed;
  }
 
  if (info->no_normal && info->force_normal)
  {
    info->force_normal = FALSE;
    SetMessage("-forcenormal and -nonormal cannot be used together");
    return GLBSP_E_BadInfoFixed;
  }
  
  if (info->factor <= 0)
  {
    info->factor = DEFAULT_FACTOR;
    SetMessage("Bad factor value !");
    return GLBSP_E_BadInfoFixed;
  }

  if (info->block_limit < 1000 || info->block_limit > 64000)
  {
    info->block_limit = DEFAULT_BLOCK_LIMIT;
    SetMessage("Bad blocklimit value !");
    return GLBSP_E_BadInfoFixed;
  }

  return GLBSP_E_OK;
}


/* ----- memory functions --------------------------- */

const char *GlbspStrDup(const char *str)
{
  if (! str)
    return NULL;

  return UtilStrDup(str);
}

void GlbspFree(const char *str)
{
  if (! str)
    return;

  UtilFree((char *) str);
}


/* ----- build nodes for a single level --------------------------- */

static glbsp_ret_e HandleLevel(void)
{
  superblock_t *seg_list;
  node_t *root_node;
  subsec_t *root_sub;

  glbsp_ret_e ret;

  if (cur_comms->cancelled)
    return GLBSP_E_Cancelled;

  DisplaySetBarLimit(1, 100);
  DisplaySetBar(1, 0);

  cur_build_pos = 0;

  LoadLevel();

  InitBlockmap();

  // create initial segs
  seg_list = CreateSegs();

  // recursively create nodes
  ret = BuildNodes(seg_list, &root_node, &root_sub, 0);
  FreeSuper(seg_list);

  if (ret == GLBSP_E_OK)
  {
    ClockwiseBspTree(root_node);

    PrintMsg("Built %d NODES, %d SSECTORS, %d SEGS, %d VERTEXES\n",
        num_nodes, num_subsecs, num_segs, num_normal_vert + num_gl_vert);

    if (root_node)
      PrintMsg("Heights of left and right subtrees = (%d,%d)\n",
          ComputeHeight(root_node->r.node), ComputeHeight(root_node->l.node));

    SaveLevel(root_node);
  }

  FreeLevel();
  FreeQuickAllocCuts();
  FreeQuickAllocSupers();

  return ret;
}


/* ----- main routine -------------------------------------- */

glbsp_ret_e GlbspBuildNodes(const nodebuildinfo_t *info,
    const nodebuildfuncs_t *funcs, volatile nodebuildcomms_t *comms)
{
  char strbuf[256];

  glbsp_ret_e ret = GLBSP_E_OK;

  cur_info  = info;
  cur_funcs = funcs;
  cur_comms = comms;

  total_big_warn = total_small_warn = 0;

  // clear cancelled flag
  comms->cancelled = FALSE;

  // sanity check
  if (!cur_info->input_file  || cur_info->input_file[0] == 0 ||
      !cur_info->output_file || cur_info->output_file[0] == 0)
  {
    SetMessage("INTERNAL ERROR: Missing in/out filename !");
    return GLBSP_E_BadArgs;
  }

  if (cur_info->no_normal && cur_info->no_gl)
  {
    SetMessage("-nonormal and -nogl specified: nothing to do !");
    return GLBSP_E_BadArgs;
  }

  InitDebug();
  
  if (info->missing_output)
    PrintMsg("* No output file specified. Using: %s\n\n", info->output_file);
  
  if (info->same_filenames)
    PrintMsg("* Output file is same as input file. Using -loadall\n\n");

  // opens and reads directory from the input wad
  ret = ReadWadFile(cur_info->input_file);

  if (ret != GLBSP_E_OK)
  {
    TermDebug();
    return ret;
  }

  if (CountLevels() <= 0)
  {
    CloseWads();
    TermDebug();

    SetMessage("No levels found in wad !");
    return GLBSP_E_Unknown;
  }
   
  PrintMsg("\nCreating nodes using tunable factor of %d\n", info->factor);

  DisplayOpen(DIS_BUILDPROGRESS);
  DisplaySetTitle("glBSP Build Progress");

  sprintf(strbuf, "File: %s", cur_info->input_file);
 
  DisplaySetBarText(2, strbuf);
  DisplaySetBarLimit(2, CountLevels() * 10);
  DisplaySetBar(2, 0);

  cur_file_pos = 0;
  
  // loop over each level in the wad
  while (FindNextLevel())
  {
    ret = HandleLevel();

    if (ret != GLBSP_E_OK)
      break;

    cur_file_pos += 10;
    DisplaySetBar(2, cur_file_pos);
  }

  DisplayClose();

  // writes all the lumps to the output wad
  if (ret == GLBSP_E_OK)
    ret = WriteWadFile(cur_info->output_file);

  // close wads and free memory
  CloseWads();

  PrintMsg("\nTotal serious warnings: %d\n", total_big_warn);
  PrintMsg("Total minor warnings: %d\n", total_small_warn);

  TermDebug();

  cur_info  = NULL;
  cur_comms = NULL;
  cur_funcs = NULL;

  return ret;
}

