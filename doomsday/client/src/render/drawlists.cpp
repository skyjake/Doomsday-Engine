/** @file drawlists.cpp  Drawable primitive list collection/management.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "render/drawlists.h"

#include <de/Log>
#include <de/memoryzone.h>
#include <QMultiHash>
#include <QtAlgorithms>

using namespace de;

// Logical texture unit state. Used with RL_LoadDefaultRtus and RL_CopyRtu
static GLTextureUnit const rtuDefault;
static GLTextureUnit rtuState[NUM_TEXMAP_UNITS];
static GLTextureUnit const *rtuMap[NUM_TEXMAP_UNITS];

// GL texture unit state used during write. Global for performance reasons.
static GLTextureUnit const *texunits[NUM_TEXTURE_UNITS];

typedef QMultiHash<DGLuint, DrawList *> DrawListHash;

DENG2_PIMPL(DrawLists)
{
    DrawListHash unlitHash;  ///< Unlit geometry.
    DrawListHash litHash;    ///< Lit geometry.
    DrawListHash dynHash;    ///< Light geometry.
    DrawListHash shinyHash;  ///< Shine geometry.
    DrawListHash shadowHash; ///< Shadow geometry.
    DrawList skyMaskList;    ///< Sky-mask geometry.

    Instance(Public *i)
        : Base(i),
          skyMaskList(SkyMaskGeom)
    {}

    ~Instance()
    {
        self.clear();
    }

    /// Choose the correct draw list hash table.
    DrawListHash &listHash(GeomGroup group)
    {
        switch(group)
        {
        case UnlitGeom:  return unlitHash;
        case LitGeom:    return litHash;
        case LightGeom:  return dynHash;
        case ShadowGeom: return shadowHash;
        case ShineGeom:  return shinyHash;
        }
        DENG2_ASSERT(false);
        return unlitHash;
    }
};

DrawLists::DrawLists() : d(new Instance(this))
{
    RL_LoadDefaultRtus();
}

static void clearLists(DrawListHash &hash)
{
    qDeleteAll(hash);
    hash.clear();
}

void DrawLists::clear()
{
    clearLists(d->unlitHash);
    clearLists(d->litHash);
    clearLists(d->dynHash);
    clearLists(d->shadowHash);
    clearLists(d->shinyHash);
    d->skyMaskList.clear();
}

static void rewindLists(DrawListHash &hash)
{
    foreach(DrawList *list, hash)
    {
        list->rewind();
    }
}

void DrawLists::reset()
{
    rewindLists(d->unlitHash);
    rewindLists(d->litHash);
    rewindLists(d->dynHash);
    rewindLists(d->shadowHash);
    rewindLists(d->shinyHash);
    d->skyMaskList.rewind();
}

// Prepare the final texture unit map for writing "normal" polygons, filling
// any gaps using a default configured texture unit.
static void prepareTexUnitMap()
{
    // Map logical texture units to "real" ones known to the GL renderer.
    texunits[TU_PRIMARY]        = rtuMap[RTU_PRIMARY];
    texunits[TU_PRIMARY_DETAIL] = rtuMap[RTU_PRIMARY_DETAIL];
    texunits[TU_INTER]          = rtuMap[RTU_INTER];
    texunits[TU_INTER_DETAIL]   = rtuMap[RTU_INTER_DETAIL];
}

// Prepare the final texture unit map for writing "shiny" polygons, filling
// any gaps using a default configured texture unit.
static void prepareTexUnitMapForShinyPoly()
{
    // Map logical texture units to "real" ones known to the GL renderer.
    texunits[TU_PRIMARY]        = rtuMap[RTU_REFLECTION];
    texunits[TU_PRIMARY_DETAIL] = &rtuDefault;
    texunits[TU_INTER]          = rtuMap[RTU_REFLECTION_MASK];
    texunits[TU_INTER_DETAIL]   = &rtuDefault;
}

/**
 * Specialized texture unit comparision function that ignores properties which
 * are applied per-primitive and which should not result in list separation.
 *
 * (These properties are written to the primitive header and applied dynamically
 * when reading back the draw list.)
 */
static bool compareTexUnit(GLTextureUnit const &lhs, GLTextureUnit const &rhs)
{
    if(lhs.texture != rhs.texture) return false;
    if(!de::fequal(lhs.opacity, rhs.opacity)) return false;
    // Other properties are applied per-primitive and should not affect the outcome.
    return true;
}

DrawList &DrawLists::find(GeomGroup group)
{
    if(group == ShineGeom)
    {
        prepareTexUnitMapForShinyPoly();
    }
    else
    {
        prepareTexUnitMap();
    }

    // Sky masked geometry is never textured; therefore no draw list hash.
    if(group == SkyMaskGeom)
    {
        d->skyMaskList.setGeomGroup(SkyMaskGeom);
        return d->skyMaskList;
    }

    DrawList *convertable = 0;

    // Find/create a list in the hash.
    DGLuint const key  = texunits[TU_PRIMARY]->textureGLName();
    DrawListHash &hash = d->listHash(group);
    for(DrawListHash::const_iterator it = hash.find(key);
        it != hash.end() && it.key() == key; ++it)
    {
        DrawList *list = it.value();

        if((group == ShineGeom &&
            compareTexUnit(list->unit(TU_PRIMARY), *texunits[TU_PRIMARY])) ||
           (group != ShineGeom &&
            compareTexUnit(list->unit(TU_PRIMARY), *texunits[TU_PRIMARY]) &&
            compareTexUnit(list->unit(TU_PRIMARY_DETAIL), *texunits[TU_PRIMARY_DETAIL])))
        {
            if(!list->unit(TU_INTER).hasTexture() &&
               !texunits[TU_INTER]->hasTexture())
            {
                // This will do great.
                return *list;
            }

            // Is this eligible for conversion to a blended list?
            if(list->isEmpty() && !convertable && texunits[TU_INTER]->hasTexture())
            {
                // If necessary, this empty list will be selected.
                convertable = list;
            }

            // Possibly an exact match?
            if((group == ShineGeom &&
                compareTexUnit(list->unit(TU_INTER), *texunits[TU_INTER])) ||
               (group != ShineGeom &&
                compareTexUnit(list->unit(TU_INTER), *texunits[TU_INTER]) &&
                compareTexUnit(list->unit(TU_INTER_DETAIL), *texunits[TU_INTER_DETAIL])))
            {
                return *list;
            }
        }
    }

    // Did we find a convertable list?
    if(convertable)
    {
        // This list is currently empty.
        if(group == ShineGeom)
        {
            convertable->unit(TU_INTER) = *texunits[TU_INTER];
        }
        else
        {
            convertable->unit(TU_INTER)        = *texunits[TU_INTER];
            convertable->unit(TU_INTER_DETAIL) = *texunits[TU_INTER_DETAIL];
        }

        return *convertable;
    }

    // Create a new list.
    DrawList *list = hash.insert(key, new DrawList(group)).value();

    // Configure the list's GL texture units.
    if(group == ShineGeom)
    {
        list->unit(TU_PRIMARY) = *texunits[TU_PRIMARY];
        if(texunits[TU_INTER]->hasTexture())
        {
            list->unit(TU_INTER) = *texunits[TU_INTER];
        }
    }
    else
    {
        list->unit(TU_PRIMARY)        = *texunits[TU_PRIMARY];
        list->unit(TU_PRIMARY_DETAIL) = *texunits[TU_PRIMARY_DETAIL];

        if(texunits[TU_INTER]->hasTexture())
        {
            list->unit(TU_INTER)        = *texunits[TU_INTER];
            list->unit(TU_INTER_DETAIL) = *texunits[TU_INTER_DETAIL];
        }
    }

    return *list;
}

int DrawLists::findAll(GeomGroup group, FoundLists &found)
{
    LOG_AS("DrawLists::findAll");

    found.clear();
    if(group == SkyMaskGeom)
    {
        if(!d->skyMaskList.isEmpty())
        {
            found.append(&d->skyMaskList);
        }
    }
    else
    {
        DrawListHash const &hash = d->listHash(group);
        foreach(DrawList *list, hash)
        {
            if(!list->isEmpty())
            {
                found.append(list);
            }
        }
    }

    return found.count();
}

static bool isWriteStateRTU(GLTextureUnit const *ptr)
{
    // Note that the default texture unit is not considered to be part of the
    // writeable state.
    for(int i = 0; i < NUM_TEXMAP_UNITS; ++i)
    {
        if(ptr == &rtuState[i])
            return true;
    }
    return false;
}

/**
 * If the identified @a idx texture unit of the primitive writer has been mapped
 * to an external address, insert a copy of it into our internal write state.
 *
 * To be called before customizing a texture unit begins to ensure we are
 * modifying data we have ownership of!
 */
static void copyMappedRtuToState(uint idx)
{
    DENG2_ASSERT(idx < NUM_TEXMAP_UNITS);
    if(isWriteStateRTU(rtuMap[idx])) return;
    RL_CopyRtu(idx, rtuMap[idx]);
}

GLTextureUnit const **RL_RtuState()
{
    return texunits;
}

void RL_LoadDefaultRtus()
{
    for(int i = 0; i < NUM_TEXMAP_UNITS; ++i)
    {
        rtuMap[i] = &rtuDefault;
    }
}

void RL_MapRtu(uint idx, GLTextureUnit const *rtu)
{
    DENG2_ASSERT(idx < NUM_TEXMAP_UNITS);
    rtuMap[idx] = (rtu? rtu : &rtuDefault);
}

void RL_CopyRtu(uint idx, GLTextureUnit const *rtu)
{
    DENG2_ASSERT(idx < NUM_TEXMAP_UNITS);
    if(!rtu)
    {
        // Restore defaults.
        rtuMap[idx] = &rtuDefault;
        return;
    }

    rtuState[idx] = *rtu;
    // Map this unit to that owned by the write state.
    rtuMap[idx] = rtuState + idx;
}

void RL_Rtu_SetScale(uint idx, Vector2f const &st)
{
    DENG2_ASSERT(idx < NUM_TEXMAP_UNITS);
    copyMappedRtuToState(idx);
    rtuState[idx].scale = st;
}

void RL_Rtu_Scale(uint idx, float scalar)
{
    DENG2_ASSERT(idx < NUM_TEXMAP_UNITS);
    copyMappedRtuToState(idx);
    rtuState[idx].scale  *= scalar;
    rtuState[idx].offset *= scalar;
}

void RL_Rtu_ScaleST(uint idx, Vector2f const &st)
{
    DENG2_ASSERT(idx < NUM_TEXMAP_UNITS);
    copyMappedRtuToState(idx);
    rtuState[idx].scale  *= st;
    rtuState[idx].offset *= st;
}

void RL_Rtu_SetOffset(uint idx, Vector2f const &xy)
{
    DENG2_ASSERT(idx < NUM_TEXMAP_UNITS);
    copyMappedRtuToState(idx);
    rtuState[idx].offset = xy;
}

void RL_Rtu_TranslateOffset(uint idx, Vector2f const &xy)
{
    DENG2_ASSERT(idx < NUM_TEXMAP_UNITS);
    copyMappedRtuToState(idx);
    rtuState[idx].offset += xy;
}

void RL_Rtu_SetTextureUnmanaged(uint idx, DGLuint glName, int wrapS, int wrapT)
{
    DENG2_ASSERT(idx < NUM_TEXMAP_UNITS);
    copyMappedRtuToState(idx);
    rtuState[idx].texture.glName  = glName;
    rtuState[idx].texture.glWrapS = wrapS;
    rtuState[idx].texture.glWrapT = wrapT;
    rtuState[idx].texture.variant = 0;
}
