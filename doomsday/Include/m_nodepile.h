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
 * m_nodepile.h: Specialized Node Allocation
 */

#ifndef __DOOMSDAY_NODEPILE_H__
#define __DOOMSDAY_NODEPILE_H__

#define NP_ROOT_NODE ((void*) -1 )

struct linknode_s;	// Defined in dd_share.h.

typedef struct nodepile_s {
	int	count;
	int pos;
	struct linknode_s *nodes;
} nodepile_t;

void		NP_Init(nodepile_t *pile, int initial);
nodeindex_t	NP_New(nodepile_t *pile, void *ptr);
void		NP_Link(nodepile_t *pile, nodeindex_t node, nodeindex_t root);
void		NP_Unlink(nodepile_t *pile, nodeindex_t node);

#define NP_Dismiss(pile, node) (pile.nodes[node].ptr = 0)

#endif 
