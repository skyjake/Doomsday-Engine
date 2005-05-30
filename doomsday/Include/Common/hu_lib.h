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
// DESCRIPTION:  none
//
//	Compiles for jDoom, jHeretic, jHexen
//-----------------------------------------------------------------------------

#ifndef __HULIB__
#define __HULIB__

#include "Common/hu_stuff.h"

// background and foreground screen numbers
// different from other modules.
#define BG			1
#define FG			0

// font stuff
#define HU_CHARERASE	KEY_BACKSPACE

#define HU_MAXLINES		4
#define HU_MAXLINELENGTH	160

//
// Typedefs of widgets
//

// Text Line widget
//  (parent of Scrolling Text and Input Text widgets)
typedef struct {
	// left-justified position of scrolling text window
	int             x;
	int             y;

	struct dpatch_s *f;			   // font
	int             sc;			   // start character
	char            l[HU_MAXLINELENGTH + 1];	// line of text
	int             len;		   // current line length

	// whether this line needs to be udpated
	int             needsupdate;

} hu_textline_t;

// Scrolling Text window widget
//  (child of Text Line widget)
typedef struct {
	hu_textline_t   l[HU_MAXLINES];	// text lines to draw
	int             h;			   // height in lines
	int             cl;			   // current line number

	// pointer to boolean stating whether to update window
	boolean        *on;
	boolean         laston;		   // last value of *->on.

} hu_stext_t;

// Input Text Line widget
//  (child of Text Line widget)
typedef struct {
	hu_textline_t   l;			   // text line to input on

	// left margin past which I am not to delete characters
	int             lm;

	// pointer to boolean stating whether to update window
	boolean        *on;
	boolean         laston;		   // last value of *->on;

} hu_itext_t;

//
// Widget creation, access, and update routines
//

// initializes heads-up widget library
void            HUlib_init(void);

//
// textline code
//

// clear a line of text
void            HUlib_clearTextLine(hu_textline_t * t);

void            HUlib_initTextLine(hu_textline_t * t, int x, int y,
								   dpatch_t * f, int sc);

// returns success
boolean         HUlib_addCharToTextLine(hu_textline_t * t, char ch);

// returns success
boolean         HUlib_delCharFromTextLine(hu_textline_t * t);

// draws tline
void            HUlib_drawTextLine(hu_textline_t * l, boolean drawcursor);

// erases text line
void            HUlib_eraseTextLine(hu_textline_t * l);

//
// Scrolling Text window widget routines
//

// ?
void            HUlib_initSText(hu_stext_t * s, int x, int y, int h,
								dpatch_t * font, int startchar, boolean *on);

// add a new line
void            HUlib_addLineToSText(hu_stext_t * s);

// ?
void            HUlib_addMessageToSText(hu_stext_t * s, char *prefix,
										char *msg);

// draws stext
void            HUlib_drawSText(hu_stext_t * s);

// erases all stext lines
void            HUlib_eraseSText(hu_stext_t * s);

// Input Text Line widget routines
void            HUlib_initIText(hu_itext_t * it, int x, int y, dpatch_t * font,
								int startchar, boolean *on);

// enforces left margin
void            HUlib_delCharFromIText(hu_itext_t * it);

// enforces left margin
void            HUlib_eraseLineFromIText(hu_itext_t * it);

// resets line and left margin
void            HUlib_resetIText(hu_itext_t * it);

// left of left-margin
void            HUlib_addPrefixToIText(hu_itext_t * it, char *str);

// whether eaten
boolean         HUlib_keyInIText(hu_itext_t * it, unsigned char ch);

void            HUlib_drawIText(hu_itext_t * it);

// erases all itext lines
void            HUlib_eraseIText(hu_itext_t * it);

#endif
//-----------------------------------------------------------------------------
//
// $Log$
// Revision 1.2  2005/05/30 17:27:06  skyjake
// Fixes (now compiles and runs in Linux)
//
// Revision 1.1  2005/05/29 05:21:04  danij
// Commonised HUD widget code.
//
// Revision 1.6  2005/03/02 00:00:02  DaniJ
// Compiles for jDoom, jHeretic, jHexen. Commonised.
//
// Revision 1.5  2004/05/29 18:19:58  skyjake
// Refined indentation style
//
// Revision 1.4  2004/05/29 09:53:11  skyjake
// Consistent style (using GNU Indent)
//
// Revision 1.3  2004/05/28 17:16:35  skyjake
// Resolved conflicts (branch-1-7 overrides)
//
// Revision 1.1.2.1  2004/05/16 10:01:30  skyjake
// Merged good stuff from branch-nix for the final 1.7.15
//
// Revision 1.1.4.1  2003/11/19 17:08:47  skyjake
// Modified to compile with gcc and -DUNIX
//
// Revision 1.1  2003/02/26 19:18:27  skyjake
// Initial checkin
//
// Revision 1.1  2002/09/29 01:04:12  Jaakko
// Added all headers
//
//
//-----------------------------------------------------------------------------
