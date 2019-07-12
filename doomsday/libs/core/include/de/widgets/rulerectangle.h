/*
 * The Doomsday Engine Project
 *
 * Copyright © 2011-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBCORE_RECTANGLERULE_H
#define LIBCORE_RECTANGLERULE_H

#include "../AnimationVector"
#include "../ISizeRule"
#include "../Rectangle"
#include "rules.h"

namespace de {

/**
 * A set of rules defining a rectangle.
 *
 * Instead of being derived from Rule, RuleRectangle acts as a complex mapping
 * between a set of input and output Rule instances. Note that RuleRectangle is
 * not reference-counted like Rule instances.
 *
 * RuleRectangle::rect() returns the rectangle's currently valid bounds. The
 * output rules for the sides can be used normally in other rules. Horizontal
 * and vertical axes are handled independently.
 *
 * Note that RuleRectangle uses a "fluent API" for the input rule set/clear
 * methods.
 *
 * @ingroup widgets
 */
class DE_PUBLIC RuleRectangle : public ISizeRule
{
public:
    RuleRectangle();

    // Output rules.
    Rule const &left() const;
    Rule const &top() const;
    Rule const &right() const;
    Rule const &bottom() const;
    Rule const &midX() const;
    Rule const &midY() const;
    Rule const &width() const override;
    Rule const &height() const override;

    /**
     * Sets one of the input rules of the rectangle.
     *
     * @param inputRule  Semantic of the input rule.
     * @param rule       Rule to use as input. A reference is held.
     */
    RuleRectangle &setInput(Rule::Semantic inputRule, RefArg<Rule> rule);

    RuleRectangle &setLeftTop(Rule const &left, Rule const &top);

    RuleRectangle &setRightBottom(Rule const &right, Rule const &bottom);

    RuleRectangle &setSize(Rule const &width, Rule const &height);

    RuleRectangle &setSize(ISizeRule const &dimensions);

    RuleRectangle &setMidAnchorX(Rule const &middle);

    /**
     * Sets the AnchorY rule to @a middle and Y anchor point to 0.5. This is
     * equivalent to first calling setInput() and then setAnchorPoint().
     *
     * @param middle  Rule for the Y anchor.
     */
    RuleRectangle &setMidAnchorY(Rule const &middle);

    RuleRectangle &setCentered(const RuleRectangle &rect)
    {
        setMidAnchorX(rect.midX());
        setMidAnchorY(rect.midY());
        return *this;
    }

    /**
     * Sets the outputs of another rule rectangle as the inputs of this one. Uses the
     * edge rules of @a rect; to later override width or height, make sure that one of
     * the edges in that dimension are cleared.
     *
     * @param rect  Rectangle whose left, right, top and bottom outputs to use as inputs.
     */
    RuleRectangle &setRect(RuleRectangle const &rect);

    /**
     * Sets the inputs of another rule rectangle as the inputs of this one.
     * (Note the difference to setRect().)
     *
     * @param rect  Rectangle whose inputs to use as inputs.
     */
    RuleRectangle &setInputsFromRect(RuleRectangle const &rect);

    RuleRectangle &clearInput(Rule::Semantic inputRule);

    /**
     * Returns an input rule.
     */
    Rule const &inputRule(Rule::Semantic inputRule);

    template <class RuleType>
    RuleType const &inputRuleAs(Rule::Semantic input) {
        RuleType const *r = dynamic_cast<RuleType const *>(&inputRule(input));
        DE_ASSERT(r != 0);
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
    void setAnchorPoint(Vec2f const &normalizedPoint, TimeSpan const &transition = 0.0);

    /**
     * Returns the current rectangle as defined by the input rules.
     */
    Rectanglef rect() const;

    /**
     * Returns the current size of the rectangle as defined by the input rules.
     */
    Vec2f size() const;

    /**
     * Returns the current size of the rectangle as defined by the input rules.
     */
    Vec2i sizei() const;

    Vec2ui sizeui() const;

    /**
     * Returns the current rectangle as defined by the input rules.
     * Values are floored to integers.
     */
    Rectanglei recti() const;

    void setDebugName(String const &name);

    bool isFullyDefined() const;

    String description() const;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_RECTANGLERULE_H
