/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2009, 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBDEN2_VISUAL_H
#define LIBDEN2_VISUAL_H

#include <de/AnimatorVector>
#include <de/Rectangle>

#include <QObject>
#include <QList>

#include "rules.h"

/**
 * A visual is a graphical object that is drawn onto a drawing surface.
 *
 * @ingroup video
 */
class Visual : public QObject
{
    Q_OBJECT

public:
    enum DrawingStage {
        BeforeChildren,
        AfterChildren
    };

public:
    Visual();

    virtual ~Visual();

    /**
     * Deletes all child visuals.
     */
    void clear();

    /**
     * Adds a child visual. It is appended to the list of children.
     *
     * @param visual  Visual to append. Ownership given to the new parent.
     *
     * @return The added visual.
     */
    Visual* add(Visual* visual);

    /**
     * Removes a child visual.
     *
     * @param visual  Visual to remove from children.
     *
     * @return  Ownership of the visual given to the caller.
     */
    Visual* remove(Visual* visual);

    void setRect(RectangleRule* rule);

    RectangleRule* rule();

    const RectangleRule* rule() const;

    /**
     * Returns the visual's current placement.
     */
    de::Rectanglef rect() const;

    /**
     * Draws the visual tree.
     */
    virtual void draw() const;

    /**
     * Draws this visual only.
     */
    virtual void drawSelf(DrawingStage stage) const;

protected:
    void checkRect();

private:
    /// Parent visual (NULL for the root visual).
    Visual* _parent;

    /// Child visuals. Owned by the visual.
    typedef std::list<Visual*> Children;
    Children _children;

    RectangleRule* _rect;
};

#endif /* LIBDEN2_VISUAL_H */
