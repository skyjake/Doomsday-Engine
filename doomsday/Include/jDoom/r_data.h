// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//  Refresh module, data I/O, caching, retrieval of graphics
//  by name.
//
//-----------------------------------------------------------------------------


#ifndef __R_DATA__
#define __R_DATA__

#include "r_defs.h"
#include "r_state.h"

#ifdef __GNUG__
#pragma interface
#endif

// Retrieve column data for span blitting.
/*byte*
R_GetColumn
( int		tex,
  int		col );*/


// I/O, setting up the stuff.
/*void R_InitData (void);
void R_PrecacheLevel (void);*/


// Retrieval.
// Floor/ceiling opaque texture tiles,
// lookup by name. For animation?
//int R_FlatNumForName (char* name);
/*#define R_FlatNumForName			gi.R_FlatNumForName*/


// Called by P_Ticker for switches and animations,
// returns the texture number for the texture name.
//int R_TextureNumForName (char *name);
//int R_CheckTextureNumForName (char *name);
/*#define R_TextureNumForName			gi.R_TextureNumForName
#define R_CheckTextureNumForName	gi.R_CheckTextureNumForName*/

#endif
//-----------------------------------------------------------------------------
//
// $Log$
// Revision 1.1  2003/02/26 19:18:40  skyjake
// Initial checkin
//
// Revision 1.1  2002/09/29 01:04:13  Jaakko
// Added all headers
//
//
//-----------------------------------------------------------------------------
