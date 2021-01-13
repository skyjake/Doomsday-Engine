/** @file convexsubspace.h  Map convex subspace.
 * @ingroup world
 *
 * @authors Copyright © 2003-2020 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2016 Daniel Swanson <danij@dengine.net>
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

#pragma once

#include "world/line.h"
#include <doomsday/world/convexsubspace.h>

class Lumobj;
namespace world { class AudioEnvironment; }

/**
 * On client side a convex subspace also provides / links to various geometry data assets
 * and properties used to visualize the subspace.
 */
class ConvexSubspace : public world::ConvexSubspace
{
public:
    ConvexSubspace(mesh::Face &convexPolygon, world::BspLeaf *bspLeaf = nullptr);

    /**
     * Returns a pointer to the face geometry half-edge which has been chosen for use as
     * the base for a triangle fan GL primitive. May return @c nullptr if no suitable base
     * was determined.
     */
    mesh::HEdge *fanBase() const;

    /**
     * Returns the number of vertices needed for a triangle fan GL primitive.
     *
     * @note When first called after a face geometry is assigned a new 'base' half-edge for
     * the triangle fan primitive will be determined.
     *
     * @see fanBase()
     */
    int fanVertexCount() const;

    /**
     * Returns the frame number of the last time mobj sprite projection was performed for
     * the subspace.
     */
    int lastSpriteProjectFrame() const;
    void setLastSpriteProjectFrame(int newFrameNumber);

    //- Audio environment -------------------------------------------------------------------

    /**
     * Recalculate the environmental audio characteristics (reverb) of the subspace.
     */
    bool updateAudioEnvironment();

    /**
     * Provides access to the cached audio environment characteristics of the subspace for
     * efficient accumulation.
     */
    const world::AudioEnvironment &audioEnvironment() const;

    //- Fake radio ---------------------------------------------------------------------------

    /**
     * Returns the total number of shadow line sides linked in the subspace.
     */
    int shadowLineCount() const;

    /**
     * Clear the list of fake radio shadow line sides for the subspace.
     */
    void clearShadowLines();

    /**
     * Iterate through the set of fake radio shadow lines for the subspace.
     *
     * @param func  Callback to make for each LineSide.
     */
    de::LoopResult forAllShadowLines(const std::function<de::LoopResult (LineSide &)>& func) const;

    /**
     * Add the specified line @a side to the set of fake radio shadow lines for the
     * subspace. If the line is already present in this set then nothing will happen.
     *
     * @param side  Map line side to add to the set.
     */
    void addShadowLine(LineSide &side);

    //- Luminous objects --------------------------------------------------------------------

    /**
     * Returns the total number of Lumobjs linked to the subspace.
     */
    int lumobjCount() const;

    /**
     * Iterate all Lumobjs linked in the subspace.
     *
     * @param callback  Call to make for each.
     */
    de::LoopResult forAllLumobjs(const std::function<de::LoopResult (Lumobj &)>& callback) const;

    /**
     * Clear @em all linked Lumobj in the subspace.
     */
    void unlinkAllLumobjs();

    /**
     * Unlink @a lumobj from the subspace.
     *
     * @see link()
     */
    void unlink(Lumobj &lumobj);

    /**
     * Link @a lumobj in the subspace (if already linked nothing happens).
     *
     * @see unlink()
     */
    void link(Lumobj &lumobj);

private:
    DE_PRIVATE(d)
};
