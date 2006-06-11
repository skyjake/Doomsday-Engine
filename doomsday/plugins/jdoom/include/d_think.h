/* $Id$
 *
 * Copyright (C) 1993-1996 by id Software, Inc.
 *
 * This source is available for distribution and/or modification
 * only under the terms of the DOOM Source Code License as
 * published by id Software. All rights reserved.
 *
 * The source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
 * for more details.
 */

/*
 * MapObj data. Map Objects or mobjs are actors, entities,
 * thinker, take-your-pick... anything that moves, acts, or
 * suffers state changes of more or less violent nature.
 */

#ifndef __D_THINK__
#define __D_THINK__

#ifdef __GNUG__
#pragma interface
#endif

//
// Experimental stuff.
// To compile this as "ANSI C with classes"
//  we will need to handle the various
//  action functions cleanly.
//
typedef void    (*actionf_v) ();
typedef void    (*actionf_p1) (void *);
typedef void    (*actionf_p2) (void *, void *);

typedef union {
    actionf_p1      acp1;
    actionf_v       acv;
    actionf_p2      acp2;

} actionf_t;

#endif
