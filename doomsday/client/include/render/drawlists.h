/** @file drawlists.h  Drawable primitive list collection/management.
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

#ifndef DENG_CLIENT_RENDER_DRAWLISTS_H
#define DENG_CLIENT_RENDER_DRAWLISTS_H

#include "de_platform.h"
#include "api_gl.h" // DGLuint

#include "DrawList"
#include <de/Vector>
#include <QVarLengthArray>

class DrawLists
{
public:
    typedef QVarLengthArray<DrawList *, 1024> FoundLists;

public:
    DrawLists();

    /**
     * Locate an appropriate draw list for the specified geometry group and the
     * currently configured, list texture unit write state. Collectively, these
     * arguments comprise the "draw list specification".
     *
     * @param group  Logical geometry group identifier.
     *
     * @return  The chosen list.
     */
    DrawList &find(GeomGroup group);

    /**
     * Finds all draw lists which match the given specification. Note that only
     * non-empty lists are collected.
     *
     * @param group  Logical geometry group identifier.
     * @param found  Set of draw lists which match the result.
     *
     * @return  Number of draw lists found.
     */
    int findAll(GeomGroup group, FoundLists &found);

    /**
     * To be called before rendering of a new frame begins.
     */
    void reset();

    /**
     * All lists will be destroyed.
     */
    void clear();

private:
    DENG2_PRIVATE(d)
};

struct GLTextureUnit;

/**
 * Reset the texture unit write state back to the initial default values.
 * Any mappings between logical units and preconfigured RTU states are
 * cleared at this time.
 */
void RL_LoadDefaultRtus();

DrawListSpec const &RL_CurrentListSpec();

/**
 * Map the texture unit write state for the identified @a idx unit to
 * @a rtu. This creates a reference to @a rtu which MUST continue to
 * remain accessible until the mapping is subsequently cleared or
 * changed (explicitly by call to RL_MapRtu/RL_LoadDefaultRtus, or,
 * implicitly by customizing the unit configuration through one of the
 * RL_RTU* family of functions (at which point a copy will be performed).
 */
void RL_MapRtu(uint idx, GLTextureUnit const *rtu);

/**
 * Copy the configuration for the identified @a idx texture unit of
 * the primitive writer's internal state from @a rtu.
 *
 * @param idx  Logical index of the texture unit being configured.
 * @param rtu  Configured RTU state to copy the configuration from.
 */
void RL_CopyRtu(uint idx, GLTextureUnit const *rtu);

/// @todo Avoid modifying the RTU write state for the purposes of primitive
///       specific translations by implementing these as arguments to the
///       RL_Add* family of functions.

/// Change the scale property of the identified @a idx texture unit.
void RL_Rtu_SetScale(uint idx, de::Vector2f const &st);

/// Scale the offset and scale properties of the identified @a idx texture unit.
void RL_Rtu_Scale(uint idx, float scalar);
void RL_Rtu_ScaleST(uint idx, de::Vector2f const &st);

/// Change the offset property of the identified @a idx texture unit.
void RL_Rtu_SetOffset(uint idx, de::Vector2f const &st);

/// Translate the offset property of the identified @a idx texture unit.
void RL_Rtu_TranslateOffset(uint idx, de::Vector2f const &st);

/// Change the texture assigned to the identified @a idx texture unit.
void RL_Rtu_SetTextureUnmanaged(uint idx, DGLuint glName, int wrapS, int wrapT);

#endif // DENG_CLIENT_RENDER_DRAWLISTS_H
