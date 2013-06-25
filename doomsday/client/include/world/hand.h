/** @file hand.h Hand (metaphor) for the manipulation of "grabbables".
 *
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_WORLD_HAND_H
#define DENG_WORLD_HAND_H

#include <QList>

#include <de/Observers>
#include <de/Vector>

#include "world/world.h"

#include "Grabbable"

namespace de {
namespace internal {
    template <typename Type>
    inline bool cannotCastGrabbableTo(Grabbable *gabbable) {
        return dynamic_cast<Type *>(gabbable) == NULL;
    }
}
}

/**
 * Represents the "hand" of the user in the client-side world. Facilitates the
 * manipulation of so-called "grabbables" for the purposes of runtime editing.
 *
 * As one might derive from the name, the hand is a metaphor for the will of
 * the user. Although the hand has a presence in the world it should not however
 * be considered a map element (such as a mobj).
 *
 * @ingroup world
 */
class Hand : DENG2_OBSERVES(de::World, FrameEnd)
{
    DENG2_NO_COPY  (Hand)
    DENG2_NO_ASSIGN(Hand)

public:
    /*
     * Notified when a grabbable is grabbed.
     */
    DENG2_DEFINE_AUDIENCE(Grabbed, void handGrabbed(Hand &hand, Grabbable &grabbable))

    /*
     * Notified when a grabbable is ungrabbed.
     */
    DENG2_DEFINE_AUDIENCE(Ungrabbed, void handUngrabbed(Hand &hand, Grabbable &grabbable))

    typedef QList<Grabbable *> Grab;

public:
    /**
     * Construct a new hand.
     */
    Hand(de::Vector3d const &origin = de::Vector3d());

    /**
     * Returns the origin of the hand in the map coordinate space.
     *
     * @see setOrigin()
     */
    de::Vector3d const &origin() const;

    /**
     * Change the origin of the hand in the map coordinate space.
     *
     * @param newOrigin  New origin coordinates to apply.
     *
     * @see origin()
     */
    void setOrigin(de::Vector3d const &newOrigin);

    /**
     * Returns @c true off the hand is empty (i.e., nothing grabbed).
     *
     * @see grabbed()
     */
    bool isEmpty() const;

    /**
     * Returns @c true iff the hand has grabbed the specified @a grabbable.
     * If you only need to know if the grabbable has been grabbed or not (rather
     * than by whom) then @ref Grabbable::isGrabbed() should be used instead as
     * this is faster.
     *
     * @see grabbed(), grab(), ungrab()
     */
    bool hasGrabbed(Grabbable const &grabbable) const;

    /**
     * Grab the specified @a grabbable, releasing the current grab. If already
     * grabbed then nothing will happen.
     *
     * @see grabMulti(), ungrab(), hasGrabbed()
     */
    void grab(Grabbable &grabbable);

    /**
     * Extend the grab by appending the specified @a grabbable to the LIFO stack
     * of grabbables maintained by the hand (the order in which grabbables are
     * grabbed is remembered by the hand).
     *
     * @see grab(), hasGrabbed(), grabbed()
     */
    void grabMulti(Grabbable &grabbable);

    /**
     * Release the specified @a grabbable if grabbed by the hand. If not grabbed
     * then nothing will happen.
     *
     * @see grab(), grabMulti(), grabbed()
     */
    void ungrab(Grabbable &grabbable);

    /**
     * Release anything currently grabbed by the hand. The grabbables are released
     * in reverse order (modelled as a LIFO stack).
     *
     * @see grab(), grabMulti(), grabbed()
     */
    void ungrab();

    /**
     * Provides access to the grab list of everything currently held by the hand.
     *
     * @see grabbedCount()
     */
    Grab const &grabbed() const;

    /**
     * Convenient method of returning the total number of grabbed elements.
     *
     * @see grabbed()
     */
    inline int grabbedCount() const { return grabbed().count(); }

    /**
     * Provides the averaged origin coordinates (in the map coordinate space) of
     * everything currently grabbed by the hand. If nothing is grabbed then a
     * default (0, 0, 0) vector is returned.
     *
     * @see grabbed()
     */
    de::Vector3d const &grabbedOrigin() const;

    float editIntensity() const;
    de::Vector3f const &editColor() const;

    void setEditIntensity(float newIntensity);
    void setEditColor(de::Vector3f const &newColor);

#ifdef __CLIENT__
protected:
    /// Observes World FrameEnd
    void worldFrameEnds(de::World &world);
#endif

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_WORLD_HAND_H
