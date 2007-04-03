/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

//
// 08/01/2005 DJS - First pass. Combined all game/am_map src files. Lots of hacky compiler directives...


#ifndef __AMMAP_H__
#define __AMMAP_H__

#ifndef __JDOOM__
extern boolean  automapactive;  // Common\f_infine.c is looking for this if not jDoom... ??

// DJS - defined in Include\jDoom\Mn_def.h in all games but jDoom
#define LINEHEIGHT_A 10
#endif

void    AM_Register(void);  // Called during init to register automap cvars and ccmds.
void    AM_Init(void);      // Called during init to initialize the automap.
void    AM_Shutdown(void);  // Called on exit to free any allocated memory.

void    AM_Ticker(void);    // Called by main loop.
void    AM_Drawer(void);    // Called every frame to render the map (if visible).
void    AM_Stop(void);      // Called to force the automap to quit if the level is completed while it is up.

void    M_DrawMAP(void);    // Called to render the map menu.

// Used by ST StatusBar stuff.
#define AM_MSGHEADER (('a'<<24)+('m'<<16))
#define AM_MSGENTERED (AM_MSGHEADER | ('e'<<8))
#define AM_MSGEXITED (AM_MSGHEADER | ('x'<<8))

// Keys
#define AM_STARTKEY DDKEY_TAB
#define AM_ENDKEY   DDKEY_TAB

#define AM_NUMMARKPOINTS 10

// the following is crap
#define LINE_NEVERSEE ML_DONTDRAW

// Counter Cheat flags.
#define CCH_KILLS           0x1
#define CCH_ITEMS           0x2
#define CCH_SECRET          0x4
#define CCH_KILLS_PRCNT         0x8
#define CCH_ITEMS_PRCNT         0x10
#define CCH_SECRET_PRCNT        0x20


#ifdef __JDOOM__
// For use if I do walls with outsides/insides
#define BLUES       (256-4*16+8)
#define YELLOWRANGE 1
#define BLACK       0
#define REDS        (256-5*16)
#define REDRANGE    16
#define BLUERANGE   8
#define GREENS      (7*16)
#define GREENRANGE  16
#define GRAYS       (6*16)
#define GRAYSRANGE  16
#define BROWNS      (4*16)
#define BROWNRANGE  16
#define YELLOWS     (256-32+7)
#define WHITE       (256-47)
#define YOURCOLORS      WHITE
#define YOURRANGE       0

# ifdef __WOLFTC__
#  define WALLCOLORS      GRAYS
#  define WALLRANGE       GRAYSRANGE
#  define TSWALLCOLORS    BROWNS
#  define TSWALLRANGE     BROWNRANGE
#  define CDWALLCOLORS    BROWNS
#  define CDWALLRANGE     BROWNRANGE
#  define THINGCOLORS     REDS
#  define THINGRANGE      REDRANGE
# else
#  define WALLCOLORS      REDS
#  define WALLRANGE       REDRANGE
#  define TSWALLCOLORS    GRAYS
#  define TSWALLRANGE     GRAYSRANGE
#  define CDWALLCOLORS    YELLOWS
#  define CDWALLRANGE     YELLOWRANGE
#  define THINGCOLORS     GREENS
#  define THINGRANGE      GREENRANGE
#endif

#define SECRETWALLCOLORS WALLCOLORS
#define SECRETWALLRANGE WALLRANGE
#define GRIDCOLORS      (GRAYS + GRAYSRANGE/2)
#define GRIDRANGE       0
#define XHAIRCOLORS     GRAYS
#define FDWALLCOLORS        BROWNS
#define FDWALLRANGE     BROWNRANGE

#define BORDEROFFSET 3

// Automap colors
#define BACKGROUND      BLACK

// Keys for Baby Mode
#define KEY1   197      //  Blue Key
#define KEY2   (256-5*16)   //  Red Key
#define KEY3   (256-32+7)   //  Yellow Key
#define KEY4   (256-32+7)   //  Yellow Skull
#define KEY5   (256-5*16)   //  Red Skull
#define KEY6   197      //  Blue Skull

#define BORDERGRAPHIC "brdr_b"

#define MARKERPATCHES (sprintf(namebuf, "AMMNUM%d", i))     // DJS - Patches used for marking the automap, a bit of a hack I suppose

#endif


#ifdef __JHERETIC__

// For use if I do walls with outsides/insides
#define REDS        12*8
#define REDRANGE    1              //16
#define BLUES       (256-4*16+8)
#define BLUERANGE   1              //8
#define GREENS      (33*8)
#define GREENRANGE  1              //16
#define GRAYS       (5*8)
#define GRAYSRANGE  1              //16
#define BROWNS      (14*8)
#define BROWNRANGE  1              //16
#define YELLOWS     10*8
#define YELLOWRANGE 1
#define BLACK       0
#define WHITE       4*8
#define PARCH       13*8-1
#define WALLCOLORS      REDS
#define FDWALLCOLORS        BROWNS
#define CDWALLCOLORS        YELLOWS
#define YOURCOLORS      WHITE
#define YOURRANGE       0
#define WALLRANGE       REDRANGE
#define TSWALLCOLORS        GRAYS
#define TSWALLRANGE     GRAYSRANGE
#define FDWALLRANGE     BROWNRANGE
#define CDWALLRANGE     YELLOWRANGE
#define THINGCOLORS     GREENS
#define THINGRANGE      GREENRANGE
#define SECRETWALLCOLORS WALLCOLORS
#define SECRETWALLRANGE WALLRANGE
#define GRIDCOLORS      (GRAYS + GRAYSRANGE/2)
#define GRIDRANGE       0
#define XHAIRCOLORS     GRAYS

#define BLOODRED  150

// Automap colors
#define BACKGROUND  PARCH
#define YOURCOLORS  WHITE
#define YOURRANGE   0

#define WALLRANGE   REDRANGE
#define TSWALLCOLORS    GRAYS
#define TSWALLRANGE GRAYSRANGE

#define FDWALLRANGE BROWNRANGE

#define CDWALLRANGE YELLOWRANGE
#define THINGCOLORS GREENS
#define THINGRANGE  GREENRANGE
#define SECRETWALLCOLORS WALLCOLORS
#define SECRETWALLRANGE WALLRANGE
#define GRIDCOLORS  (GRAYS + GRAYSRANGE/2)
#define GRIDRANGE   0
#define XHAIRCOLORS GRAYS

#define BORDEROFFSET 4

// Keys for Baby Mode
#define KEY1   144  // HERETIC - Green Key
#define KEY2   197  // HERETIC - Yellow Key
#define KEY3   220  // HERETIC - Blue Key

#define BORDERGRAPHIC "bordb"

#define MARKERPATCHES (sprintf(namebuf, "FONTA%d", (16 +i) ))       // DJS - Patches used for marking the automap, a bit of a hack I suppose

#endif


#ifdef __JHEXEN__
// For use if I do walls with outsides/insides
#define REDS        12*8
#define REDRANGE    1              //16
#define BLUES       (256-4*16+8)
#define BLUERANGE   1              //8
#define GREENS      (33*8)
#define GREENRANGE  1              //16
#define GRAYS       (5*8)
#define GRAYSRANGE  1              //16
#define BROWNS      (14*8)
#define BROWNRANGE  1              //16
#define YELLOWS     10*8
#define YELLOWRANGE 1
#define BLACK       0
#define WHITE       4*8
#define PARCH       13*8-1
#define BLOODRED    177

// Automap colors
#define BACKGROUND  PARCH
#define YOURCOLORS  WHITE
#define YOURRANGE   0
#define WALLCOLORS  83      // REDS
#define WALLRANGE   REDRANGE
#define TSWALLCOLORS    GRAYS
#define TSWALLRANGE GRAYSRANGE
#define FDWALLCOLORS    96      // BROWNS
#define FDWALLRANGE BROWNRANGE
#define CDWALLCOLORS    107     // YELLOWS
#define CDWALLRANGE YELLOWRANGE
#define THINGCOLORS GREENS
#define THINGRANGE  GREENRANGE
#define SECRETWALLCOLORS WALLCOLORS
#define SECRETWALLRANGE WALLRANGE
#define GRIDCOLORS  (GRAYS + GRAYSRANGE/2)
#define GRIDRANGE   0
#define XHAIRCOLORS GRAYS

#define BORDEROFFSET 4

// Automap colors
#define AM_PLR1_COLOR 157       // Blue
#define AM_PLR2_COLOR 177       // Red
#define AM_PLR3_COLOR 137       // Yellow
#define AM_PLR4_COLOR 198       // Green
#define AM_PLR5_COLOR 215       // Jade
#define AM_PLR6_COLOR 32        // White
#define AM_PLR7_COLOR 106       // Hazel
#define AM_PLR8_COLOR 234       // Purple

#define KEY1   197  // HEXEN -
#define KEY2   144  // HEXEN -
#define KEY3   220  // HEXEN -

#define BORDERGRAPHIC "bordb"

#define MARKERPATCHES (sprintf(namebuf, "FONTA%d", (16 +i) ))       // DJS - Patches used for marking the automap, a bit of a hack I suppose

#endif

#ifdef __JSTRIFE__
// For use if I do walls with outsides/insides
#define REDS        12*8
#define REDRANGE    1              //16
#define BLUES       (256-4*16+8)
#define BLUERANGE   1              //8
#define GREENS      (33*8)
#define GREENRANGE  1              //16
#define GRAYS       (5*8)
#define GRAYSRANGE  1              //16
#define BROWNS      (14*8)
#define BROWNRANGE  1              //16
#define YELLOWS     10*8
#define YELLOWRANGE 1
#define BLACK       0
#define WHITE       4*8
#define PARCH       13*8-1
#define BLOODRED    177

// Automap colors
#define BACKGROUND  PARCH
#define YOURCOLORS  WHITE
#define YOURRANGE   0
#define WALLCOLORS  83      // REDS
#define WALLRANGE   REDRANGE
#define TSWALLCOLORS    GRAYS
#define TSWALLRANGE GRAYSRANGE
#define FDWALLCOLORS    96      // BROWNS
#define FDWALLRANGE BROWNRANGE
#define CDWALLCOLORS    107     // YELLOWS
#define CDWALLRANGE YELLOWRANGE
#define THINGCOLORS GREENS
#define THINGRANGE  GREENRANGE
#define SECRETWALLCOLORS WALLCOLORS
#define SECRETWALLRANGE WALLRANGE
#define GRIDCOLORS  (GRAYS + GRAYSRANGE/2)
#define GRIDRANGE   0
#define XHAIRCOLORS GRAYS

#define BORDEROFFSET 4

// Automap colors
#define AM_PLR1_COLOR 157       // Blue
#define AM_PLR2_COLOR 177       // Red
#define AM_PLR3_COLOR 137       // Yellow
#define AM_PLR4_COLOR 198       // Green
#define AM_PLR5_COLOR 215       // Jade
#define AM_PLR6_COLOR 32        // White
#define AM_PLR7_COLOR 106       // Hazel
#define AM_PLR8_COLOR 234       // Purple

#define KEY1   197  // HEXEN -
#define KEY2   144  // HEXEN -
#define KEY3   220  // HEXEN -

#define BORDERGRAPHIC "bordb"

#define MARKERPATCHES (sprintf(namebuf, "FONTA%d", (16 +i) ))       // DJS - Patches used for marking the automap, a bit of a hack I suppose

#endif


#endif
