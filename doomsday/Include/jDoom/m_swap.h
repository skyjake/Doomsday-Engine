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
//	Endianess handling, swapping 16bit and 32bit.
//
//-----------------------------------------------------------------------------


#ifndef __M_SWAP__
#define __M_SWAP__


#ifdef __GNUG__
#pragma interface
#endif


// Endianess handling.
// WAD files are stored little endian.
#ifdef __BIG_ENDIAN__
short	SwapSHORT(short);
long	SwapLONG(long);
#define SHORT(x)	((short)SwapSHORT((unsigned short) (x)))
#define LONG(x)         ((long)SwapLONG((unsigned long) (x)))
#else
#define SHORT(x)	(x)
#define LONG(x)         (x)
#endif




#endif
//-----------------------------------------------------------------------------
//
// $Log$
// Revision 1.3  2004/05/28 17:16:35  skyjake
// Resolved conflicts (branch-1-7 overrides)
//
// Revision 1.1.2.1  2004/05/16 10:01:30  skyjake
// Merged good stuff from branch-nix for the final 1.7.15
//
// Revision 1.1.4.1  2003/11/19 17:08:47  skyjake
// Modified to compile with gcc and -DUNIX
//
// Revision 1.1  2003/02/26 19:18:31  skyjake
// Initial checkin
//
// Revision 1.1  2002/09/29 01:04:13  Jaakko
// Added all headers
//
//
//-----------------------------------------------------------------------------

