/** @file hand.cpp Hand (metaphor) for the manipulation of "grabbables".
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

#include "BiasSource" // remove me

#include "world/hand.h"

using namespace de;

DENG2_PIMPL(Hand),
DENG2_OBSERVES(Grabbable, Deletion),
DENG2_OBSERVES(Grabbable, OriginChange)
{
    /// Origin of the hand in the map coordinate space.
    Vector3d origin;
    Vector3d oldOrigin; // For tracking changes.

    /// All currently held grabbables if any (not owned).
    Grab grab;

    /// Averaged origin of eveything currently grabbed.
    Vector3d grabOrigin;
    bool needUpdateGrabOrigin;

    /// Edit properties (applied to the grabbables).
    Vector3f editColor;
    float editIntensity;

    Instance(Public *i, Vector3d const &origin)
        : Base(i),
          origin(origin),
          oldOrigin(origin),
          needUpdateGrabOrigin(false),
          editIntensity(0)
    {}

    void notifyGrabbed(Grabbable &grabbed)
    {
        DENG2_FOR_PUBLIC_AUDIENCE(Grabbed, i)
        {
            i->handGrabbed(self, grabbed);
        }
    }

    void notifyUngrabbed(Grabbable &ungrabbed)
    {
        DENG2_FOR_PUBLIC_AUDIENCE(Ungrabbed, i)
        {
            i->handUngrabbed(self, ungrabbed);
        }
    }

    void grabOne(Grabbable &grabbable)
    {
        // Ignore attempts to re-grab.
        if(grab.contains(&grabbable)) return;

        grabbable.grab();

        // Ensure the grabbable is locked.
        grabbable.lock();

        grab.append(&grabbable);
        grabbable.audienceForDeletion     += this;
        grabbable.audienceForOriginChange += this;

        needUpdateGrabOrigin = true;

        notifyGrabbed(grabbable);
    }

    void ungrabOne(Grabbable &grabbable)
    {
        // Ignore attempts to ungrab what isn't grabbed.
        if(!grab.contains(&grabbable)) return;

        grabbable.ungrab();

        // Ensure the grabbable is unlocked.
        grabbable.unlock();

        grabbable.audienceForDeletion     -= this;
        grabbable.audienceForOriginChange -= this;
        grab.removeOne(&grabbable);

        needUpdateGrabOrigin = true;

        notifyUngrabbed(grabbable);
    }

    // Observes Grabbable Deletion.
    void grabbableBeingDeleted(Grabbable &grabbable)
    {
        DENG2_ASSERT(grab.contains(&grabbable)); //sanity check.

        grab.removeOne(&grabbable);

        needUpdateGrabOrigin = true;

        notifyUngrabbed(grabbable);
    }

    // Observes Grabbable OriginChange.
    void grabbableOriginChanged(Grabbable &grabbable)
    {
        DENG2_ASSERT(grab.contains(&grabbable)); //sanity check.
        needUpdateGrabOrigin = true;
    }

    void updateGrabOrigin()
    {
        needUpdateGrabOrigin = false;

        grabOrigin = Vector3d();
        foreach(Grabbable *grabbable, grab)
            grabOrigin += grabbable->origin();
        if(grab.count() > 1)
            grabOrigin /= grab.count();

        //qDebug() << "Hand new grab origin" << grabOrigin.asText();
    }
};

Hand::Hand(Vector3d const &origin) : d(new Instance(this, origin))
{}

Vector3d const &Hand::origin() const
{
    return d->origin;
}

void Hand::setOrigin(Vector3d const &newOrigin)
{
    if(d->origin != newOrigin)
    {
        //qDebug() << "Hand new origin" << newOrigin.asText();
        d->origin = newOrigin;
    }
}

bool Hand::isEmpty() const
{
    return grabbed().isEmpty();
}

bool Hand::hasGrabbed(Grabbable const &grabbable) const
{
    return grabbed().contains(const_cast<Grabbable *>(&grabbable));
}

void Hand::grab(Grabbable &grabbable)
{
    if(hasGrabbed(grabbable)) return;
    ungrab();
    d->grabOne(grabbable);
}

void Hand::grabMulti(Grabbable &grabbable)
{
    d->grabOne(grabbable);
}

void Hand::ungrab(Grabbable &grabbable)
{
    d->ungrabOne(grabbable);
}

void Hand::ungrab()
{
    //qDebug() << "Hand ungrabbing all";
    while(!isEmpty())
    {
        ungrab(*grabbed().last());
    }
}

Hand::Grab const &Hand::grabbed() const
{
    return d->grab;
}

de::Vector3d const &Hand::grabbedOrigin() const
{
    if(d->needUpdateGrabOrigin)
    {
        d->updateGrabOrigin();
    }
    return d->grabOrigin;
}

float Hand::editIntensity() const
{
    return d->editIntensity;
}

void Hand::setEditIntensity(float newIntensity)
{
    d->editIntensity = newIntensity;
}

void Hand::setEditColor(Vector3f const &newColor)
{
    d->editColor = newColor;
}

Vector3f const &Hand::editColor() const
{
    return d->editColor;
}

void Hand::worldFrameEnds(World &world)
{
    DENG2_UNUSED(world);

    if(grabbedCount())
    {
        // Do we need to move the grab?
        bool moveGrab = (d->origin != d->oldOrigin);

        if(moveGrab && d->needUpdateGrabOrigin)
        {
            // Determine the center of the grab.
            d->updateGrabOrigin();
        }

        foreach(Grabbable *grabbable, grabbed())
        {
            if(moveGrab)
            {
                // The move will be denied if the grabbable is locked.
                if(grabbedCount() == 1)
                {
                    grabbable->move(d->origin);
                }
                else
                {
                    grabbable->move(d->origin + (grabbable->origin() - d->grabOrigin));
                }
            }

            /// @todo There should be a generic mechanism for applying the user's
            /// edits to the grabbables. The editable values are properties of the
            /// hand (an extension of the user's will) however the hand should not
            /// be responsible for their application as this requires knowledge of
            /// their meaning. Instead the hand should provide the values and let
            /// the grabbable(s) update themselves.
            if(!internal::cannotCastGrabbableTo<BiasSource>(grabbable))
            {
                grabbable->as<BiasSource>().setIntensity(d->editIntensity)
                                           .setColor    (d->editColor);
            }
        }
    }

    // Update the hand origin tracking buffer.
    d->oldOrigin = d->origin;
}
