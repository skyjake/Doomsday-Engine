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
//  Nil.
//    
//-----------------------------------------------------------------------------


#ifndef __M_ARGV__
#define __M_ARGV__

//
// MISC
//
//extern  int	myargc;
//extern  char**	myargv;
#define myargc		Argc()
//#define myargv		Argv

// Returns the position of the given parameter
// in the arg list (0 if not found).
//int M_CheckParm (char* check);

#define	M_CheckParm		gi.CheckParm


#endif
//-----------------------------------------------------------------------------
//
// $Log$
// Revision 1.1  2003/02/26 19:18:30  skyjake
// Initial checkin
//
// Revision 1.1  2002/09/29 01:04:13  Jaakko
// Added all headers
//
//
//-----------------------------------------------------------------------------
