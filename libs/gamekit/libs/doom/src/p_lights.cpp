/** @file p_lights.cpp  Handle Sector base lighting effects.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 * @authors Copyright © 1999-2000 Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze (PrBoom 2.2.6)
 * @authors Copyright © 1993-1996 id Software, Inc.
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

#include "jdoom.h"
#include "p_lights.h"

#include "dmu_lib.h"
#include "p_mapsetup.h"
#include "p_mapspec.h"

void T_FireFlicker(void *flickPtr)
{
    fireflicker_t *flick = (fireflicker_t *)flickPtr;
    float amount, lightLevel;

    if(--flick->count)
        return;

    lightLevel = P_GetFloatp(flick->sector, DMU_LIGHT_LEVEL);
    amount = ((P_Random() & 3) * 16) / 255.0f;

    if(lightLevel - amount < flick->minLight)
        P_SetFloatp(flick->sector, DMU_LIGHT_LEVEL, flick->minLight);
    else
        P_SetFloatp(flick->sector, DMU_LIGHT_LEVEL,
                    flick->maxLight - amount);

    flick->count = 4;
}

void fireflicker_s::write(MapStateWriter *msw) const
{
    Writer1 *writer = msw->writer();

    Writer_WriteByte(writer, 1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    Writer_WriteInt32(writer, P_ToIndex(sector));

    Writer_WriteInt32(writer, (int) (255.0f * maxLight));
    Writer_WriteInt32(writer, (int) (255.0f * minLight));
}

/**
 * T_FireFlicker was added to save games in ver5, therefore we don't have
 * an old format to support.
 */
int fireflicker_s::read(MapStateReader *msr)
{
    Reader1 *reader = msr->reader();

    /*int ver =*/ Reader_ReadByte(reader); // version byte.

    // Note: the thinker class byte has already been read.
    sector         = (Sector*)P_ToPtr(DMU_SECTOR, (int) Reader_ReadInt32(reader));
    DE_ASSERT(sector != 0);

    maxLight       = (float) Reader_ReadInt32(reader) / 255.0f;
    minLight       = (float) Reader_ReadInt32(reader) / 255.0f;

    thinker.function = (thinkfunc_t) T_FireFlicker;

    return true; // Add this thinker.
}

void P_SpawnFireFlicker(Sector *sector)
{
    float lightLevel = P_GetFloatp(sector, DMU_LIGHT_LEVEL);
    float otherLevel = DDMAXFLOAT;

    // Note that we are resetting sector attributes.
    // Nothing special about it during gameplay.
    P_ToXSector(sector)->special = 0;

    fireflicker_t *flick = (fireflicker_t *)Z_Calloc(sizeof(*flick), PU_MAP, 0);
    flick->thinker.function = T_FireFlicker;
    Thinker_Add(&flick->thinker);

    flick->sector = sector;
    flick->count = 4;
    flick->maxLight = lightLevel;

    P_FindSectorSurroundingLowestLight(sector, &otherLevel);
    if(otherLevel < lightLevel)
        flick->minLight = otherLevel;
    else
        flick->minLight = lightLevel;
    flick->minLight += (16.0f/255.0f);
}

/**
 * Broken light flashing.
 */
void T_LightFlash(lightflash_t *flash)
{
    float lightLevel;

    if(--flash->count)
        return;

    lightLevel = P_GetFloatp(flash->sector, DMU_LIGHT_LEVEL);
    if(lightLevel == flash->maxLight)
    {
        P_SetFloatp(flash->sector, DMU_LIGHT_LEVEL, flash->minLight);
        flash->count = (P_Random() & flash->minTime) + 1;
    }
    else
    {
        P_SetFloatp(flash->sector, DMU_LIGHT_LEVEL, flash->maxLight);
        flash->count = (P_Random() & flash->maxTime) + 1;
    }
}

void lightflash_s::write(MapStateWriter *msw) const
{
    Writer1 *writer = msw->writer();

    Writer_WriteByte(writer, 1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    Writer_WriteInt32(writer, P_ToIndex(sector));

    Writer_WriteInt32(writer, count);
    Writer_WriteInt32(writer, (int) (255.0f * maxLight));
    Writer_WriteInt32(writer, (int) (255.0f * minLight));
    Writer_WriteInt32(writer, maxTime);
    Writer_WriteInt32(writer, minTime);
}

int lightflash_s::read(MapStateReader *msr)
{
    Reader1 *reader = msr->reader();
    int mapVersion = msr->mapVersion();

    if(mapVersion >= 5)
    {
        // Note: the thinker class byte has already been read.
        /*int ver =*/ Reader_ReadByte(reader); // version byte.

        // Start of used data members.
        sector       = (Sector *)P_ToPtr(DMU_SECTOR, (int) Reader_ReadInt32(reader));
        DE_ASSERT(sector != 0);

        count        = Reader_ReadInt32(reader);
        maxLight     = (float) Reader_ReadInt32(reader) / 255.0f;
        minLight     = (float) Reader_ReadInt32(reader) / 255.0f;
        maxTime      = Reader_ReadInt32(reader);
        minTime      = Reader_ReadInt32(reader);
    }
    else
    {
        // Its in the old pre V5 format which serialized lightflash_t
        // Padding at the start (an old thinker_t struct)
        byte junk[16]; // sizeof thinker_t
        Reader_Read(reader, junk, 16);

        // Start of used data members.
        // A 32bit pointer to sector, serialized.
        sector       = (Sector *)P_ToPtr(DMU_SECTOR, (int) Reader_ReadInt32(reader));
        DE_ASSERT(sector != 0);

        count        = Reader_ReadInt32(reader);
        maxLight     = (float) Reader_ReadInt32(reader) / 255.0f;
        minLight     = (float) Reader_ReadInt32(reader) / 255.0f;
        maxTime      = Reader_ReadInt32(reader);
        minTime      = Reader_ReadInt32(reader);
    }

    thinker.function = (thinkfunc_t) T_LightFlash;

    return true; // Add this thinker.
}

/**
 * After the map has been loaded, scan each sector
 * for specials that spawn thinkers
 */
void P_SpawnLightFlash(Sector *sector)
{
    float lightLevel = P_GetFloatp(sector, DMU_LIGHT_LEVEL);
    float otherLevel = DDMAXFLOAT;

    // Note that we are resetting sector attributes.
    // Nothing special about it during gameplay.
    P_ToXSector(sector)->special = 0;

    lightflash_t *flash = (lightflash_t *)Z_Calloc(sizeof(*flash), PU_MAP, 0);
    flash->thinker.function = (thinkfunc_t) T_LightFlash;
    Thinker_Add(&flash->thinker);

    flash->sector = sector;
    flash->maxLight = lightLevel;

    P_FindSectorSurroundingLowestLight(sector, &otherLevel);
    if(otherLevel < lightLevel)
        flash->minLight = otherLevel;
    else
        flash->minLight = lightLevel;
    flash->maxTime = 64;
    flash->minTime = 7;
    flash->count = (P_Random() & flash->maxTime) + 1;
}

/**
 * Strobe light flashing.
 */
void T_StrobeFlash(strobe_t *flash)
{
    float lightLevel;

    if(--flash->count)
        return;

    lightLevel = P_GetFloatp(flash->sector, DMU_LIGHT_LEVEL);
    if(lightLevel == flash->minLight)
    {
        P_SetFloatp(flash->sector, DMU_LIGHT_LEVEL, flash->maxLight);
        flash->count = flash->brightTime;
    }
    else
    {
        P_SetFloatp(flash->sector, DMU_LIGHT_LEVEL, flash->minLight);
        flash->count = flash->darkTime;
    }
}

void strobe_s::write(MapStateWriter *msw) const
{
    Writer1 *writer = msw->writer();

    Writer_WriteByte(writer, 1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    Writer_WriteInt32(writer, P_ToIndex(sector));

    Writer_WriteInt32(writer, count);
    Writer_WriteInt32(writer, (int) (255.0f * maxLight));
    Writer_WriteInt32(writer, (int) (255.0f * minLight));
    Writer_WriteInt32(writer, darkTime);
    Writer_WriteInt32(writer, brightTime);
}

int strobe_s::read(MapStateReader *msr)
{
    Reader1 *reader = msr->reader();
    int mapVersion = msr->mapVersion();

    if(mapVersion >= 5)
    {
        // Note: the thinker class byte has already been read.
        /*int ver =*/ Reader_ReadByte(reader); // version byte.

        sector      = (Sector *)P_ToPtr(DMU_SECTOR, (int) Reader_ReadInt32(reader));
        DE_ASSERT(sector != 0);

        count       = Reader_ReadInt32(reader);
        maxLight    = (float) Reader_ReadInt32(reader) / 255.0f;
        minLight    = (float) Reader_ReadInt32(reader) / 255.0f;
        darkTime    = Reader_ReadInt32(reader);
        brightTime  = Reader_ReadInt32(reader);
    }
    else
    {
        // Its in the old pre V5 format which serialized strobe_t
        // Padding at the start (an old thinker_t struct)
        byte junk[16]; // sizeof thinker_t
        Reader_Read(reader, junk, 16);

        // Start of used data members.
        // A 32bit pointer to sector, serialized.
        sector      = (Sector *)P_ToPtr(DMU_SECTOR, (int) Reader_ReadInt32(reader));
        DE_ASSERT(sector != 0);

        count       = Reader_ReadInt32(reader);
        minLight    = (float) Reader_ReadInt32(reader) / 255.0f;
        maxLight    = (float) Reader_ReadInt32(reader) / 255.0f;
        darkTime    = Reader_ReadInt32(reader);
        brightTime  = Reader_ReadInt32(reader);
    }

    thinker.function = (thinkfunc_t) T_StrobeFlash;

    return true; // Add this thinker.
}

/**
 * After the map has been loaded, scan each sector for specials that spawn
 * thinkers.
 */
void P_SpawnStrobeFlash(Sector *sector, int fastOrSlow, int inSync)
{
    float lightLevel = P_GetFloatp(sector, DMU_LIGHT_LEVEL);
    float otherLevel = DDMAXFLOAT;

    strobe_t *flash = (strobe_t *)Z_Calloc(sizeof(*flash), PU_MAP, 0);
    flash->thinker.function = (thinkfunc_t) T_StrobeFlash;
    Thinker_Add(&flash->thinker);

    flash->sector = sector;
    flash->darkTime = fastOrSlow;
    flash->brightTime = STROBEBRIGHT;
    flash->maxLight = lightLevel;
    P_FindSectorSurroundingLowestLight(sector, &otherLevel);
    if(otherLevel < lightLevel)
        flash->minLight = otherLevel;
    else
        flash->minLight = lightLevel;

    if(flash->minLight == flash->maxLight)
        flash->minLight = 0;

    // Note that we are resetting sector attributes.
    // Nothing special about it during gameplay.
    P_ToXSector(sector)->special = 0;

    if(!inSync)
    {
        flash->count = (P_Random() & 7) + 1;
    }
    else
    {
        flash->count = 1;
    }
}

/**
 * Start strobing lights (usually from a trigger)
 */
void EV_StartLightStrobing(Line *line)
{
    iterlist_t *list = P_GetSectorIterListForTag(P_ToXLine(line)->tag, false);
    if(!list) return;

    IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
    IterList_RewindIterator(list);

    Sector *sec;
    while((sec = (Sector *)IterList_MoveIterator(list)))
    {
        if(P_ToXSector(sec)->specialData)
            continue;

        P_SpawnStrobeFlash(sec, SLOWDARK, 0);
    }
}

void EV_TurnTagLightsOff(Line *line)
{
    iterlist_t *list = P_GetSectorIterListForTag(P_ToXLine(line)->tag, false);
    if(!list) return;

    IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
    IterList_RewindIterator(list);

    Sector *sec;
    while((sec = (Sector *)IterList_MoveIterator(list)))
    {
        float lightLevel = P_GetFloatp(sec, DMU_LIGHT_LEVEL);
        float otherLevel = DDMAXFLOAT;
        P_FindSectorSurroundingLowestLight(sec, &otherLevel);
        if(otherLevel < lightLevel)
            lightLevel = otherLevel;

        P_SetFloatp(sec, DMU_LIGHT_LEVEL, lightLevel);
    }
}

void EV_LightTurnOn(Line *line, float max)
{
    iterlist_t *list = P_GetSectorIterListForTag(P_ToXLine(line)->tag, false);
    if(!list) return;

    float lightLevel = 0;
    if(NON_ZERO(max))
        lightLevel = max;

    IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
    IterList_RewindIterator(list);

    Sector *sec;
    while((sec = (Sector *)IterList_MoveIterator(list)))
    {
        // If Max = 0 means to search for the highest light level in the
        // surrounding sector.
        if(IS_ZERO(max))
        {
            lightLevel = P_GetFloatp(sec, DMU_LIGHT_LEVEL);
            float otherLevel = DDMINFLOAT;
            P_FindSectorSurroundingHighestLight(sec, &otherLevel);
            if(otherLevel > lightLevel)
                lightLevel = otherLevel;
        }

        P_SetFloatp(sec, DMU_LIGHT_LEVEL, lightLevel);
    }
}

void T_Glow(glow_t *g)
{
    float lightLevel = P_GetFloatp(g->sector, DMU_LIGHT_LEVEL);
    float glowDelta = (1.0f / 255.0f) * (float) GLOWSPEED;

    switch(g->direction)
    {
    case -1: // Down.
        lightLevel -= glowDelta;
        if(lightLevel <= g->minLight)
        {
            lightLevel += glowDelta;
            g->direction = 1;
        }
        break;

    case 1: // Up.
        lightLevel += glowDelta;
        if(lightLevel >= g->maxLight)
        {
            lightLevel -= glowDelta;
            g->direction = -1;
        }
        break;

    default:
        Con_Error("T_Glow: Invalid direction %i.", g->direction);
        break;
    }

    P_SetFloatp(g->sector, DMU_LIGHT_LEVEL, lightLevel);
}

void glow_s::write(MapStateWriter *msw) const
{
    Writer1 *writer = msw->writer();

    Writer_WriteByte(writer, 1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    Writer_WriteInt32(writer, P_ToIndex(sector));

    Writer_WriteInt32(writer, (int) (255.0f * maxLight));
    Writer_WriteInt32(writer, (int) (255.0f * minLight));
    Writer_WriteInt32(writer, direction);
}

int glow_s::read(MapStateReader *msr)
{
    Reader1 *reader = msr->reader();
    int mapVersion = msr->mapVersion();

    if(mapVersion >= 5)
    {
        // Note: the thinker class byte has already been read.
        /*int ver =*/ Reader_ReadByte(reader); // version byte.

        sector        = (Sector *)P_ToPtr(DMU_SECTOR, (int) Reader_ReadInt32(reader));
        DE_ASSERT(sector != 0);

        maxLight      = (float) Reader_ReadInt32(reader) / 255.0f;
        minLight      = (float) Reader_ReadInt32(reader) / 255.0f;
        direction     = Reader_ReadInt32(reader);
    }
    else
    {
        // Its in the old pre V5 format which serialized strobe_t
        // Padding at the start (an old thinker_t struct)
        byte junk[16]; // sizeof thinker_t
        Reader_Read(reader, junk, 16);

        // Start of used data members.
        // A 32bit pointer to sector, serialized.
        sector        = (Sector *)P_ToPtr(DMU_SECTOR, (int) Reader_ReadInt32(reader));
        DE_ASSERT(sector != 0);

        minLight      = (float) Reader_ReadInt32(reader) / 255.0f;
        maxLight      = (float) Reader_ReadInt32(reader) / 255.0f;
        direction     = Reader_ReadInt32(reader);
    }

    thinker.function = (thinkfunc_t) T_Glow;

    return true; // Add this thinker.
}

void P_SpawnGlowingLight(Sector *sector)
{
    float lightLevel = P_GetFloatp(sector, DMU_LIGHT_LEVEL);
    float otherLevel = DDMAXFLOAT;

    glow_t *g = (glow_t *)Z_Calloc(sizeof(*g), PU_MAP, 0);
    g->thinker.function = (thinkfunc_t) T_Glow;
    Thinker_Add(&g->thinker);

    g->sector = sector;
    P_FindSectorSurroundingLowestLight(sector, &otherLevel);
    if(otherLevel < lightLevel)
        g->minLight = otherLevel;
    else
        g->minLight = lightLevel;
    g->maxLight = lightLevel;
    g->direction = -1;

    // Note that we are resetting sector attributes.
    // Nothing special about it during gameplay.
    P_ToXSector(sector)->special = 0;
}
