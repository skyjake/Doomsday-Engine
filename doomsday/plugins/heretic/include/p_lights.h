/** @file p_lights.h  Handle Sector base lighting effects.
 *
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBHERETIC_PLAY_LIGHTS_H
#define LIBHERETIC_PLAY_LIGHTS_H

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "doomsday.h"

#ifdef __cplusplus
class MapStateReader;
class MapStateWriter;
#endif

#define GLOWSPEED               (8)
#define STROBEBRIGHT            (5)
#define FASTDARK                (15)
#define SLOWDARK                (35)

typedef struct lightflash_s {
    thinker_t thinker;
    Sector *sector;
    int count;
    float maxLight;
    float minLight;
    int maxTime;
    int minTime;

#ifdef __cplusplus
    void write(MapStateWriter *msw) const;
    int read(MapStateReader *msr);
#endif
} lightflash_t;

typedef struct strobe_s {
    thinker_t thinker;
    Sector *sector;
    int count;
    float minLight;
    float maxLight;
    int darkTime;
    int brightTime;

#ifdef __cplusplus
    void write(MapStateWriter *msw) const;
    int read(MapStateReader *msr);
#endif
} strobe_t;

typedef struct glow_s {
    thinker_t thinker;
    Sector *sector;
    float minLight;
    float maxLight;
    int direction;

#ifdef __cplusplus
    void write(MapStateWriter *msw) const;
    int read(MapStateReader *msr);
#endif
} glow_t;

#ifdef __cplusplus
extern "C" {
#endif

void T_LightFlash(lightflash_t *flash);
void P_SpawnLightFlash(Sector *sector);

void T_StrobeFlash(strobe_t *flash);
void P_SpawnStrobeFlash(Sector *sector, int fastOrSlow, int inSync);

void T_Glow(glow_t *g);
void P_SpawnGlowingLight(Sector *sector);

void EV_StartLightStrobing(Line *line);
void EV_TurnTagLightsOff(Line *line);
void EV_LightTurnOn(Line *line, float bright);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBHERETIC_PLAY_LIGHTS_H
