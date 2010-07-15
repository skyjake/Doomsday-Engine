/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_INFINE_MAIN_H
#define LIBDENG_INFINE_MAIN_H

// We'll use the base template directly as our object.
typedef struct fi_object_s {
    FIOBJECT_BASE_ELEMENTS()
} fi_object_t;

/**
 * @defgroup playsimServerFinaleFlags Play-simulation Server-side Finale Flags.
 *
 * Packet: PSV_FINALE Finale flags. Used with GPT_FINALE and GPT_FINALE2
 */
/*@{*/
#define FINF_BEGIN          0x01
#define FINF_END            0x02
#define FINF_SCRIPT         0x04   // Script included.
#define FINF_AFTER          0x08   // Otherwise before.
#define FINF_SKIP           0x10
#define FINF_OVERLAY        0x20   // Otherwise before (or after).
/*@}*/

#define FINALE_SCRIPT_EXTRADATA_SIZE      gx.finaleConditionsSize

extern float fiDefaultTextRGB[];

void            FI_Register(void);

int             FI_Responder(ddevent_t* ev);
void            FI_Drawer(void);

void            FI_Ticker(timespan_t time);

int             FI_SkipRequest(void);
boolean         FI_CmdExecuted(void);

#endif /* LIBDENG_INFINE_MAIN_H */
