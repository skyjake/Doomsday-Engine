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
 *
 * @ingroup widgets
 */
class DENG2_PUBLIC RectangleRule : public Rule, DENG2_OBSERVES(Clock, TimeChange)
{
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
        MAX_INPUT_RULES
    };

public:
    RectangleRule();

    /**
     * Constructs a rectangle rule with individual rules defining the placement
     * of the rectangle. References are kept to all non-NULL rules.
     *
     * @param left    Rule for the left coordinate.
     * @param top     Rule for the top coordinate.
     * @param right   Rule for the right coordinate.
     * @param bottom  Rule for the bottom coordinate.
     */
    RectangleRule(Rule const *left, Rule const *top, Rule const *right, Rule const *bottom);

    RectangleRule(RectangleRule const *rect);

    // Output rules.
    Rule const *left() const;
    Rule const *top() const;
    Rule const *right() const;
    Rule const *bottom() const;
    Rule const *width() const;
    Rule const *height() const;

    /**
     * Sets one of the input rules of the rectangle.
     *
     * @param inputRule  InputRule to set.
     * @param rule       Rule to use as input. A reference is held.
     */
    RectangleRule &setInput(InputRule inputRule, Rule const *rule);

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
    void setAnchorPoint(Vector2f const &normalizedPoint, TimeDelta const &transition = 0);

    /**
     * Returns the current rectangle as defined by the input rules.
     */
    Rectanglef rect() const;

    /**
     * Returns the current rectangle as defined by the input rules.
     * Values are floored to integers.
     */
    Rectanglei recti() const;

protected:
    ~RectangleRule();
    void update();
    void timeChanged(Clock const &);

private:
    struct Instance;
    Instance *d;
};

} // namespace de

#endif // LIBDENG2_RECTANGLERULE_H
