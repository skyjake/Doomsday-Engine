//------------------------------------------------------------------------
// MAIN : Main program for glBSP
//------------------------------------------------------------------------
//
//  GL-Friendly Node Builder (C) 2000-2005 Andrew Apted
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

static char glbsp_message_buf[1024];


const nodebuildinfo_t default_buildinfo =
{
  NULL,    // input_file
  NULL,    // output_file
  NULL,    // extra_files

  DEFAULT_FACTOR,  // factor

  FALSE,   // no_reject
  FALSE,   // no_progress
  FALSE,   // quiet
  FALSE,   // mini_warnings
  FALSE,   // force_hexen
  FALSE,   // pack_sides
  FALSE,   // fast

  2,   // spec_version

  FALSE,   // load_all
  FALSE,   // no_normal
  FALSE,   // force_normal
  FALSE,   // gwa_mode
  FALSE,   // prune_sect
  FALSE,   // no_prune
  FALSE,   // merge_vert

  DEFAULT_BLOCK_LIMIT,   // block_limit

  FALSE,   // missing_output
  FALSE    // same_filenames
};

const nodebuildcomms_t default_buildcomms =
{
  NULL,    // message
  FALSE,   // cancelled

  0, 0,    // total warnings
  0, 0     // build and file positions
};


/* ----- option parsing ----------------------------- */

#define EXTRA_BLOCK  10  /* includes terminating NULL */

static void AddExtraFile(nodebuildinfo_t *info, const char *str)
{
  int count = 0;
  int space;

  if (! info->extra_files)
  {
    info->extra_files = (const char **)
        UtilCalloc(EXTRA_BLOCK * sizeof(const char *));

    info->extra_files[0] = str;
    info->extra_files[1] = NULL;

    return;
  }

  while (info->extra_files[count])
    count++;

  space = EXTRA_BLOCK - 1 - (count % EXTRA_BLOCK);

  if (space == 0)
  {
    info->extra_files = (const char **) UtilRealloc((void *)info->extra_files,
        (count + 1 + EXTRA_BLOCK) * sizeof(const char *));
  }

  info->extra_files[count]   = str;
  info->extra_files[count+1] = NULL;
}

#define HANDLE_BOOLEAN(name, field)  \
    if (UtilStrCaseCmp(opt_str, name) == 0)  \
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
  int got_output = FALSE;

  cur_comms = comms;
  SetErrorMsg(NULL);

  while (argc > 0)
  {
    if (argv[0][0] != '-')
    {
      // --- ORDINARY FILENAME ---

      if (got_output)
      {
        SetErrorMsg("Input filenames must precede the -o option");
        cur_comms = NULL;
        return GLBSP_E_BadArgs;
      }

      if (CheckExtension(argv[0], "gwa"))
      {
        SetErrorMsg("Input file cannot be GWA (contains nothing to build)");
        cur_comms = NULL;
        return GLBSP_E_BadArgs;
      }

      if (num_files >= 1)
      {
        AddExtraFile(info, GlbspStrDup(argv[0]));
      }
      else
      {
        GlbspFree(info->input_file);
        info->input_file = GlbspStrDup(argv[0]);
      }

      num_files++;

      argv++; argc--;
      continue;
    }

    // --- AN OPTION ---

    opt_str = &argv[0][1];

    // handle GNU style options beginning with '--'
    if (opt_str[0] == '-')
      opt_str++;

    if (UtilStrCaseCmp(opt_str, "o") == 0)
    {
      if (got_output)
      {
        SetErrorMsg("The -o option cannot be used more than once");
        cur_comms = NULL;
        return GLBSP_E_BadArgs;
      }

      if (num_files >= 2)
      {
        SetErrorMsg("Cannot use -o with multiple input files.");
        cur_comms = NULL;
        return GLBSP_E_BadArgs;
      }

      if (argc < 2 || argv[1][0] == '-')
      {
        SetErrorMsg("Missing filename for the -o option");
        cur_comms = NULL;
        return GLBSP_E_BadArgs;
      }

      GlbspFree(info->output_file);
      info->output_file = GlbspStrDup(argv[1]);

      got_output = TRUE;

      argv += 2; argc -= 2;
      continue;
    }

    if (UtilStrCaseCmp(opt_str, "factor") == 0)
    {
      if (argc < 2)
      {
        SetErrorMsg("Missing factor value");
        cur_comms = NULL;
        return GLBSP_E_BadArgs;
      }

      info->factor = (int) strtol(argv[1], NULL, 10);

      argv += 2; argc -= 2;
      continue;
    }

    if (tolower(opt_str[0]) == 'v' && isdigit(opt_str[1]))
    {
      info->spec_version = (opt_str[1] - '0');

      argv++; argc--;
      continue;
    }

    if (UtilStrCaseCmp(opt_str, "maxblock") == 0)
    {
      if (argc < 2)
      {
        SetErrorMsg("Missing maxblock value");
        cur_comms = NULL;
        return GLBSP_E_BadArgs;
      }

      info->block_limit = (int) strtol(argv[1], NULL, 10);

      argv += 2; argc -= 2;
      continue;
    }

    HANDLE_BOOLEAN("q",           quiet)
    HANDLE_BOOLEAN("fast",        fast)
    HANDLE_BOOLEAN("noreject",    no_reject)
    HANDLE_BOOLEAN("noprog",      no_progress)
    HANDLE_BOOLEAN("warn",        mini_warnings)
    HANDLE_BOOLEAN("pack",        pack_sides)
    HANDLE_BOOLEAN("normal",      force_normal)

    HANDLE_BOOLEAN("loadall",     load_all)
    HANDLE_BOOLEAN("nonormal",    no_normal)
    HANDLE_BOOLEAN("forcegwa",    gwa_mode)
    HANDLE_BOOLEAN("prunesec",    prune_sect)
    HANDLE_BOOLEAN("noprune",     no_prune)
    HANDLE_BOOLEAN("mergevert",   merge_vert)

    // to err is human...
    HANDLE_BOOLEAN("noprogress",  no_progress)
    HANDLE_BOOLEAN("quiet",       quiet)
    HANDLE_BOOLEAN("packsides",   pack_sides)
    HANDLE_BOOLEAN("prunesect",   prune_sect)

    // ignore these options for backwards compatibility
    if (UtilStrCaseCmp(opt_str, "fresh") == 0 ||
        UtilStrCaseCmp(opt_str, "keepdummy") == 0 ||
        UtilStrCaseCmp(opt_str, "keepsec") == 0 ||
        UtilStrCaseCmp(opt_str, "keepsect") == 0)
    {
      argv++; argc--;
      continue;
    }

    // backwards compatibility
    HANDLE_BOOLEAN("forcenormal", force_normal)

    // The -hexen option is only kept for backwards compatibility
    HANDLE_BOOLEAN("hexen", force_hexen)

    sprintf(glbsp_message_buf, "Unknown option: %s", argv[0]);
    SetErrorMsg(glbsp_message_buf);

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
  SetErrorMsg(NULL);

  info->same_filenames = FALSE;
  info->missing_output = FALSE;

  if (!info->input_file || info->input_file[0] == 0)
  {
    SetErrorMsg("Missing input filename !");
    return GLBSP_E_BadArgs;
  }

  if (CheckExtension(info->input_file, "gwa"))
  {
    SetErrorMsg("Input file cannot be GWA (contains nothing to build)");
    return GLBSP_E_BadArgs;
  }

  if (!info->output_file || info->output_file[0] == 0)
  {
    GlbspFree(info->output_file);
    info->output_file = GlbspStrDup(ReplaceExtension(
          info->input_file, "gwa"));

    info->gwa_mode = TRUE;
    info->missing_output = TRUE;
  }
  else  /* has output filename */
  {
    if (CheckExtension(info->output_file, "gwa"))
      info->gwa_mode = TRUE;
  }

  if (UtilStrCaseCmp(info->input_file, info->output_file) == 0)
  {
    info->load_all = TRUE;
    info->same_filenames = TRUE;
  }

  if (info->no_prune && info->pack_sides)
  {
    info->pack_sides = FALSE;
    SetErrorMsg("-noprune and -packsides cannot be used together");
    return GLBSP_E_BadInfoFixed;
  }

  if (info->gwa_mode && info->force_normal)
  {
    info->force_normal = FALSE;
    SetErrorMsg("-forcenormal used, but GWA files don't have normal nodes");
    return GLBSP_E_BadInfoFixed;
  }
 
  if (info->no_normal && info->force_normal)
  {
    info->force_normal = FALSE;
    SetErrorMsg("-forcenormal and -nonormal cannot be used together");
    return GLBSP_E_BadInfoFixed;
  }
 
  if (info->factor <= 0 || info->factor > 32)
  {
    info->factor = DEFAULT_FACTOR;
    SetErrorMsg("Bad factor value !");
    return GLBSP_E_BadInfoFixed;
  }

  if (info->spec_version <= 0 || info->spec_version > 5)
  {
    info->spec_version = 2;
    SetErrorMsg("Bad GL-Nodes version number !");
    return GLBSP_E_BadInfoFixed;
  }
  else if (info->spec_version == 4)
  {
    info->spec_version = 5;
    SetErrorMsg("V4 GL-Nodes is not supported");
    return GLBSP_E_BadInfoFixed;
  }

  if (info->block_limit < 1000 || info->block_limit > 64000)
  {
    info->block_limit = DEFAULT_BLOCK_LIMIT;
    SetErrorMsg("Bad blocklimit value !");
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
  node_t *root_stale_node;
  subsec_t *root_sub;

  glbsp_ret_e ret;

  if (cur_comms->cancelled)
    return GLBSP_E_Cancelled;

  DisplaySetBarLimit(1, 1000);
  DisplaySetBar(1, 0);

  cur_comms->build_pos = 0;

  LoadLevel();

  InitBlockmap();

  // create initial segs
  seg_list = CreateSegs();

  root_stale_node = (num_stale_nodes == 0) ? NULL : 
      LookupStaleNode(num_stale_nodes - 1);

  // recursively create nodes
  ret = BuildNodes(seg_list, &root_node, &root_sub, 0, root_stale_node);
  FreeSuper(seg_list);

  if (ret == GLBSP_E_OK)
  {
    ClockwiseBspTree(root_node);

    PrintVerbose("Built %d NODES, %d SSECTORS, %d SEGS, %d VERTEXES\n",
        num_nodes, num_subsecs, num_segs, num_normal_vert + num_gl_vert);

    if (root_node)
      PrintVerbose("Heights of left and right subtrees = (%d,%d)\n",
          ComputeBspHeight(root_node->r.node),
          ComputeBspHeight(root_node->l.node));

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

  cur_comms->total_big_warn = 0;
  cur_comms->total_small_warn = 0;

  // clear cancelled flag
  comms->cancelled = FALSE;

  // sanity check
  if (!cur_info->input_file  || cur_info->input_file[0] == 0 ||
      !cur_info->output_file || cur_info->output_file[0] == 0)
  {
    SetErrorMsg("INTERNAL ERROR: Missing in/out filename !");
    return GLBSP_E_BadArgs;
  }

  InitDebug();
  InitEndian();
 
  if (info->missing_output)
    PrintMsg("* No output file specified. Using: %s\n\n", info->output_file);
  else if (info->same_filenames)
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

    SetErrorMsg("No levels found in wad !");
    return GLBSP_E_Unknown;
  }
   
  PrintMsg("\n");
  PrintVerbose("Creating nodes using tunable factor of %d\n", info->factor);

  DisplayOpen(DIS_BUILDPROGRESS);
  DisplaySetTitle("glBSP Build Progress");

  sprintf(strbuf, "File: %s", cur_info->input_file);
 
  DisplaySetBarText(2, strbuf);
  DisplaySetBarLimit(2, CountLevels() * 10);
  DisplaySetBar(2, 0);

  cur_comms->file_pos = 0;
  
  // loop over each level in the wad
  while (FindNextLevel())
  {
    ret = HandleLevel();

    if (ret != GLBSP_E_OK)
      break;

    cur_comms->file_pos += 10;
    DisplaySetBar(2, cur_comms->file_pos);
  }

  DisplayClose();

  // writes all the lumps to the output wad
  if (ret == GLBSP_E_OK)
  {
    ret = WriteWadFile(cur_info->output_file);

    // when modifying the original wad, any GWA companion must be deleted
    if (ret == GLBSP_E_OK && cur_info->same_filenames)
      DeleteGwaFile(cur_info->output_file);

    PrintMsg("\n");
    PrintMsg("Total serious warnings: %d\n", cur_comms->total_big_warn);
    PrintMsg("Total minor warnings: %d\n", cur_comms->total_small_warn);

    ReportFailedLevels();
  }

  // close wads and free memory
  CloseWads();

  TermDebug();

  cur_info  = NULL;
  cur_comms = NULL;
  cur_funcs = NULL;

  return ret;
}

