/**\file
 *\section Copyright and License Summary
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
 *
 * This program is covered by the HERETIC / HEXEN (LIMITED USE) source
 * code license; you can redistribute it and/or modify it under the terms
 * of the HERETIC / HEXEN source code license as published by Activision.
 *
 * THIS MATERIAL IS NOT MADE OR SUPPORTED BY ACTIVISION.
 *
 * WARRANTY INFORMATION.
 * This program is provided as is. Activision and it's affiliates make no
 * warranties of any kind, whether oral or written , express or implied,
 * including any warranty of merchantability, fitness for a particular
 * purpose or non-infringement, and no other representations or claims of
 * any kind shall be binding on or obligate Activision or it's affiliates.
 *
 * LICENSE CONDITIONS.
 * You shall not:
 *
 * 1) Exploit this Program or any of its parts commercially.
 * 2) Use this Program, or permit use of this Program, on more than one
 *    computer, computer terminal, or workstation at the same time.
 * 3) Make copies of this Program or any part thereof, or make copies of
 *    the materials accompanying this Program.
 * 4) Use the program, or permit use of this Program, in a network,
 *    multi-user arrangement or remote access arrangement, including any
 *    online use, except as otherwise explicitly provided by this Program.
 * 5) Sell, rent, lease or license any copies of this Program, without
 *    the express prior written consent of Activision.
 * 6) Remove, disable or circumvent any proprietary notices or labels
 *    contained on or within the Program.
 *
 * You should have received a copy of the HERETIC / HEXEN source code
 * license along with this program (Ravenlic.txt); if not:
 * http://www.ravensoft.com/
 */

#ifndef __X_EVENT__
#define __X_EVENT__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

typedef enum {
    GA_NONE,
    GA_LOADLEVEL,
    GA_INITNEW,
    GA_NEWGAME,
    GA_LOADGAME,
    GA_SAVEGAME,
    GA_COMPLETED,
    GA_LEAVEMAP,
    GA_SINGLEREBORN,
    GA_VICTORY,
    GA_WORLDDONE,
    GA_SCREENSHOT
} gameaction_t;

extern gameaction_t gameaction;

//
// Button/action code definitions.
//
typedef enum
{
    // Press "Fire".
    BT_ATTACK       = 1,
    // Use button, to open doors, activate switches.
    BT_USE          = 2,

    // Flag: game events, not really buttons.
    BT_SPECIAL      = 128,
    BT_SPECIALMASK  = 3,

    // Center player look angle (pitch back to zero).
    //BT_LOOKCENTER = 64,

    // Flag, weapon change pending.
    // If true, the next 3 bits hold weapon num.
    BT_CHANGE       = 4,
    // The 3bit weapon mask and shift, convenience.
    BT_WEAPONMASK   = (8+16+32+64),
    BT_WEAPONSHIFT  = 3,

    BT_JUMP         = 8,
    BT_SPEED        = 16,

    // Pause the game.
    BTS_PAUSE       = 1,
    // Save the game at each console.
    //BTS_SAVEGAME  = 2,

    // Savegame slot numbers
    //  occupy the second byte of buttons.
    //BTS_SAVEMASK  = (4+8+16),
    //BTS_SAVESHIFT     = 2,

    // Special weapon change flags.
    BTS_NEXTWEAPON  = 4,
    BTS_PREVWEAPON  = 8,

} buttoncode_t;

#endif
