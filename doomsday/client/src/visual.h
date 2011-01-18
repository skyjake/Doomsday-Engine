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

class Rule;

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

    enum PlacementRule {
        Left,
        Top,
        Right,
        Bottom,
        Width,
        Height,
        AnchorX,
        AnchorY
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

    /**
     * Sets a placement rule for the visual. If the particular rule has previously been
     * defined, the old one is destroyed first.
     *
     * @param rule  Visual takes ownership of the rule object.
     */
    void setRule(PlacementRule placementRule, Rule* rule);

    Rule* rule(PlacementRule placementRule);

    template <class RuleType>
    RuleType& ruleAs(PlacementRule placementRule) {
        RuleType* r = dynamic_cast<RuleType*>(rule(placementRule));
        Q_ASSERT(r != 0);
        return *r;
    }

    /**
     * Sets the anchor reference point within the visual rectangle for the anchor X and
     * anchor Y rules.
     *
     * @param normalizedPoint  (0, 0) refers to the top left corner within the visual,
     *                         (1, 1) to the bottom right.
     */
    void setAnchorPoint(const de::Vector2f& normalizedPoint);

    /**
     * Calculates the rectangle of the visual.
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

private:
    Rule** ruleRef(PlacementRule placementRule);

    /// Parent visual (NULL for the root visual).
    Visual* _parent;

    /// Child visuals. Owned by the visual.
    typedef std::list<Visual*> Children;
    Children _children;

    de::Vector2f _normalizedAnchorPoint;
    Rule* _anchorXRule;
    Rule* _anchorYRule;
    Rule* _leftRule;
    Rule* _topRule;
    Rule* _rightRule;
    Rule* _bottomRule;
    Rule* _widthRule;
    Rule* _heightRule;
};

#endif /* LIBDEN2_VISUAL_H */
