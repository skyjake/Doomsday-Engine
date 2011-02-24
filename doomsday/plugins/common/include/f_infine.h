/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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

/**
 * f_infine.h:
 */

#ifndef __F_INFINE_H__
#define __F_INFINE_H__

// Condition truth values (that clients can't deduce on their own).
enum {
    FICOND_SECRET,
    FICOND_LEAVEHUB,
    NUM_FICONDS
};

typedef enum infinemode_e {
    FIMODE_LOCAL,
    FIMODE_OVERLAY,
    FIMODE_BEFORE,
    FIMODE_AFTER
} infinemode_t;

extern boolean fiActive;
extern boolean fiCmdExecuted; // Set to true after first command.
extern boolean briefDisabled;

void            FI_Reset(void);
void            FI_Start(char* finalescript, infinemode_t mode);
void            FI_End(void);
void            FI_SetCondition(int index, boolean value);
int             FI_Briefing(uint episode, uint map, ddfinale_t* fin);
int             FI_Debriefing(uint episode, uint map, ddfinale_t* fin);
void            FI_DemoEnds(void);
int             FI_SkipRequest(void);
void            FI_Ticker(void);
int             FI_Responder(event_t* ev);
void            FI_Drawer(void);
boolean         FI_IsMenuTrigger(event_t* ev);

DEFCC(CCmdStartInFine);
DEFCC(CCmdStopInFine);

#endif
