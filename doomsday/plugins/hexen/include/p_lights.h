/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 * p_lights.h:
 */

#ifndef __P_LIGHTS_H__
#define __P_LIGHTS_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#define LIGHT_SEQUENCE_START        (2)
#define LIGHT_SEQUENCE              (3)
#define LIGHT_SEQUENCE_ALT          (4)

typedef enum {
    LITE_RAISEBYVALUE,
    LITE_LOWERBYVALUE,
    LITE_CHANGETOVALUE,
    LITE_FADE,
    LITE_GLOW,
    LITE_FLICKER,
    LITE_STROBE
} lighttype_t;

typedef struct {
    thinker_t       thinker;
    Sector*         sector;
    lighttype_t     type;
    float           value1;
    float           value2;
    int             tics1;  //// \todo Type LITEGLOW uses this as a third light value. As such, it has been left as 0 - 255 for now.
    int             tics2;
    int             count;
} light_t;

typedef struct {
    thinker_t       thinker;
    Sector*         sector;
    int             index;
    float           baseValue;
} phase_t;

void            T_Phase(phase_t* phase);
void            P_SpawnPhasedLight(Sector* sec, float base, int index);

void            T_Light(light_t* light);
void            P_SpawnLightSequence(Sector* sec, int indexStep);

boolean         EV_SpawnLight(LineDef* line, byte* arg, lighttype_t type);

#endif
