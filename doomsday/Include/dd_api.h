/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * dd_api.h: Data Structures for the Engine/Game Interface
 */

#ifndef __DOOMSDAY_GAME_API_H__
#define __DOOMSDAY_GAME_API_H__

#include "dd_share.h"

/*
 * The routines/data exported out of the Doomsday engine: 
 * 
 * This structure contains pointers to routines that can have alternative
 * handlers in the engine. Also, some select global variables are exported 
 * using this structure (most importantly the map data).
 */
typedef struct
{
	int			apiSize;		// sizeof(game_import_t)
	int			version;		// Doomsday Engine version.

	//
	// DATA
	//
	// Data arrays.
	mobjinfo_t	**mobjinfo;
	state_t		**states;
	sprname_t	**sprnames;
	ddtext_t	**text;
	
	// General information.
	int			*validcount;
	fixed_t		*topslope;
	fixed_t		*bottomslope;

	// Thinker data (DO NOT CHANGE).
	thinker_t	*thinkercap; // The head and tail of the thinker list

	// Map data, pointers to the arrays. 
	int			*numvertexes;
	int			*numsegs;
	int			*numsectors;
	int			*numsubsectors;
	int			*numnodes;
	int			*numlines;
	int			*numsides;
	void		**vertexes;
	void		**segs;
	void		**sectors;
	void		**subsectors;
	void		**nodes;
	void		**lines;
	void		**sides;
	short		**blockmaplump;
	short		**blockmap;
	int			*bmapwidth;
	int			*bmapheight;
	int			*bmaporgx;
	int			*bmaporgy;
	byte		**rejectmatrix;
	void		***polyblockmap;
	void		**polyobjs;
	int			*numpolyobjs;
} 
game_import_t; // game import == engine export

/*
 * The routines/data exported from the game DLL.
 */
typedef struct
{
	int			apiSize;		// sizeof(game_export_t)

	// Base-level.
	void		(*PreInit)(void);
	void		(*PostInit)(void);
	void		(*Shutdown)(void);
	void		(*UpdateState)(int step);
	char*		(*Get)(int id);

	// Ticcmds.
	void		(*BuildTicCmd)(void *cmd);
	void		(*DiscardTicCmd)(void *discarded, void *current);

	// Networking.
	int			(*NetServerStart)(int before);
	int			(*NetServerStop)(int before);
	int			(*NetConnect)(int before);
	int			(*NetDisconnect)(int before);
	int			(*NetPlayerEvent)(int playernum, int type, void *data);
	int			(*NetWorldEvent)(int type, int parm, void *data);
	void		(*HandlePacket)(int fromplayer, int type, void *data, int length);
	
	// Tickers.
	void		(*Ticker)(void);

	// Responders.
	boolean		(*PrivilegedResponder)(event_t *event);
	boolean		(*MN_Responder)(event_t *event);
	boolean		(*G_Responder)(event_t *event);

	// Refresh.
	void		(*BeginFrame)(void);
	void		(*EndFrame)(void);
	void		(*G_Drawer)(void);
	void		(*MN_Drawer)(void);
	void		(*ConsoleBackground)(int *width, int *height);
	void		(*R_Init)(void);

	// Miscellaneous.
	void		(*MobjThinker)();
	fixed_t		(*MobjFriction)(void *mobj);	// Returns a friction factor.

	// Main structure sizes.
	int			ticcmd_size;		// sizeof(ticcmd_t)
	int			vertex_size;		// etc.
	int			seg_size;
	int			sector_size;
	int			subsector_size;
	int			node_size;
	int			line_size;
	int			side_size;
	int			polyobj_size;
}
game_export_t;

typedef game_export_t* (*GETGAMEAPI)(game_import_t*);

#endif
