/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_RECTANGLERULE_H
#define LIBDENG2_RECTANGLERULE_H

#include "../AnimationVector"
#include "../Rectangle"
#include "rules.h"

namespace de {

/**
 * A set of rules defining a rectangle.
 *
 * The value of the rectangle rule is the area of the rectangle (width *
 * height). RectangleRule::rect() returns the rectangle itself. The output
 * rules for the sides can be used normally in other rules.
 */
class RectangleRule : public Rule
{
    Q_OBJECT

public:
    enum InputRule {
        Left,
        Top,
        Right,
        Bottom,
        Width,
        Height,
        AnchorX,
        AnchorY,
        MAX_RULES
    };

public:
    explicit RectangleRule(QObject *parent = 0);
    explicit RectangleRule(Rule const *left, Rule const *top, Rule const *right, Rule const *bottom, QObject *parent = 0);
    explicit RectangleRule(RectangleRule const *rect, QObject *parent = 0);

    // Output rules.
    Rule const *left() const;
    Rule const *top() const;
    Rule const *right() const;
    Rule const *bottom() const;

    /**
     * Sets one of the input rules of the rectangle. If the particular rule has
     * previously been defined, the old one is destroyed first.
     *
     * @param inputRule  Input rule to set.
     * @param rule       RectangleRule takes ownership.
     */
    void setRule(InputRule inputRule, Rule* rule);

    /**
     * Returns an input rule.
     */
    Rule const *inputRule(InputRule inputRule);

    template <class RuleType>
    RuleType const &inputRuleAs(InputRule input) {
        RuleType const *r = dynamic_cast<RuleType const *>(inputRule(input));
        DENG2_ASSERT(r != 0);
        return *r;
    }

    /**
     * Sets the anchor reference point within the rectangle for the anchor X
     * and anchor Y rules.
     *
     * @param normalizedPoint  (0, 0) refers to the top left corner,
     *                         (1, 1) to the bottom right.
     * @param transition       Transition time for the change.
     */
    void setAnchorPoint(Vector2f const &normalizedPoint, Time::Delta const &transition = 0);

    /**
     * Returns the current rectangle as defined by the input rules.
     */
    Rectanglef rect() const;

public slots:
    void currentTimeChanged();

protected:
    void setup();
    void update();
    void dependencyReplaced(Rule const *oldRule, Rule const *newRule);

private:
    Rule const **ruleRef(InputRule rule);

    DerivedRule *_left;
    DerivedRule *_top;
    DerivedRule *_right;
    DerivedRule *_bottom;

    AnimationVector2 _normalizedAnchorPoint;
    Rule const *_anchorXRule;
    Rule const *_anchorYRule;
    Rule const *_leftRule;
    Rule const *_topRule;
    Rule const *_rightRule;
    Rule const *_bottomRule;
    Rule const *_widthRule;
    Rule const *_heightRule;
};

} // namespace de

#endif // LIBDENG2_RECTANGLERULE_H
