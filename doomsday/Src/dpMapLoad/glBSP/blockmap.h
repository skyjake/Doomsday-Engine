//------------------------------------------------------------------------
// BLOCKMAP : Generate the blockmap
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

#ifndef __GLBSP_BLOCKMAP_H__
#define __GLBSP_BLOCKMAP_H__

#include "structs.h"
#include "level.h"

#define DEFAULT_BLOCK_LIMIT  44000

extern int block_x, block_y;
extern int block_w, block_h;

// compute blockmap origin & size (the block_x/y/w/h variables above)
// based on the set of loaded linedefs.
//
void InitBlockmap(void);

// build the blockmap and write the data into the BLOCKMAP lump
void PutBlockmap(void);

#endif /* __GLBSP_BLOCKMAP_H__ */
