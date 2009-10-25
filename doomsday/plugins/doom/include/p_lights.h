/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009 Daniel Swanson <danij@dengine.net>
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
 * p_lights.h: Handle Sector base lighting effects.
 */

#ifndef __P_LIGHTS_H__
#define __P_LIGHTS_H__

#include "d_identifiers.h"
#include <de/Thinker>

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#define GLOWSPEED               (8)
#define STROBEBRIGHT            (5)
#define FASTDARK                (15)
#define SLOWDARK                (35)

class LightThinker : public de::Thinker 
{
public:
    sector_t*       sector;
    float           maxLight;
    float           minLight;
    
public:
    LightThinker(SerialId sid) 
        : de::Thinker(sid),
          sector(0), 
          maxLight(0),
          minLight(0) {}
          
    // Implements ISerializable.
    void operator >> (de::Writer& to) const;
    void operator << (de::Reader& from);
};

class LightFlashThinker : public LightThinker 
{
public:
    int             count;
    int             maxTime;
    int             minTime;
    
public:
    LightFlashThinker() 
        : LightThinker(SID_LIGHT_FLASH_THINKER),
          count(0),
          maxTime(0),
          minTime(0) {}
          
    void think(const de::Time::Delta& elapsed);

    // Implements ISerializable.
    void operator >> (de::Writer& to) const;
    void operator << (de::Reader& from);
    
    static Thinker* construct() {
        return new LightFlashThinker;
    }
};

class FireFlickerThinker : public LightThinker
{
public:
    int             count;
    
public:
    FireFlickerThinker()    
        : LightThinker(SID_FIRE_FLICKER_THINKER),
          count(0) {}
          
    void think(const de::Time::Delta& elapsed);

    // Implements ISerializable.
    void operator >> (de::Writer& to) const;
    void operator << (de::Reader& from);

    static Thinker* construct() {
        return new FireFlickerThinker;
    }
};

class StrobeThinker : public LightThinker
{
public:
    int             count;
    int             darkTime;
    int             brightTime;
    
public:
    StrobeThinker()    
        : LightThinker(SID_STROBE_THINKER),
          count(0),
          darkTime(0),
          brightTime(0) {}
          
    void think(const de::Time::Delta& elapsed);

    // Implements ISerializable.
    void operator >> (de::Writer& to) const;
    void operator << (de::Reader& from);

    static Thinker* construct() {
        return new StrobeThinker;
    }
};

class GlowThinker : public LightThinker 
{
public:
    int             direction;
    
public:  
    GlowThinker()    
        : LightThinker(SID_GLOW_THINKER),
          direction(0) {}
          
    void think(const de::Time::Delta& elapsed);

    // Implements ISerializable.
    void operator >> (de::Writer& to) const;
    void operator << (de::Reader& from);

    static Thinker* construct() {
        return new GlowThinker;
    }
};

typedef LightFlashThinker lightflash_t;
typedef FireFlickerThinker fireflicker_t;
typedef StrobeThinker strobe_t;
typedef GlowThinker glow_t;

void            P_SpawnFireFlicker(sector_t* sector);
void            P_SpawnLightFlash(sector_t* sector);
void            P_SpawnStrobeFlash(sector_t* sector, int fastOrSlow, int inSync);
void            P_SpawnGlowingLight(sector_t* sector);

void            EV_StartLightStrobing(linedef_t* line);
void            EV_TurnTagLightsOff(linedef_t* line);
void            EV_LightTurnOn(linedef_t* line, float bright);

#endif
