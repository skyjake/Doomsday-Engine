/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef RECTANGLERULE_H
#define RECTANGLERULE_H

#include <de/AnimatorVector>
#include "rules.h"

/**
 * A set of rules defining a rectangle.
 *
 * The value of the rectangle rule is the area of the rectangle (width * height).
 * RectangleRule::rect() returns the rectangle itself.
 * The output rules for the sides can be used normally in other rules.
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
    explicit RectangleRule(const Rule* left, const Rule* top, const Rule* right, const Rule* bottom, QObject *parent = 0);
    explicit RectangleRule(const RectangleRule* rect, QObject* parent = 0);

    const Rule* left() const;
    const Rule* top() const;
    const Rule* right() const;
    const Rule* bottom() const;

    /**
     * Sets a placement rule for the visual. If the particular rule has previously been
     * defined, the old one is destroyed first.
     *
     * @param rule  Visual takes ownership of the rule object.
     */
    void setRule(InputRule inputRule, Rule* rule);

    const Rule* inputRule(InputRule inputRule);

    template <class RuleType>
    const RuleType& inputRuleAs(InputRule input) {
        const RuleType* r = dynamic_cast<const RuleType*>(inputRule(input));
        Q_ASSERT(r != 0);
        return *r;
    }

    /**
     * Sets the anchor reference point within the visual rectangle for the anchor X and
     * anchor Y rules.
     *
     * @param normalizedPoint  (0, 0) refers to the top left corner within the visual,
     *                         (1, 1) to the bottom right.
     * @param transition       Transition time for the change.
     */
    void setAnchorPoint(const de::Vector2f& normalizedPoint, const de::TimeDelta& transition = 0);

    /**
     * Returns the current rectangle as defined by the input rules.
     */
    de::Rectanglef rect() const;

public slots:
    void currentTimeChanged();

protected:
    void setup();
    void update();
    void dependencyReplaced(const Rule* oldRule, const Rule* newRule);

private:
    const Rule** ruleRef(InputRule rule);

    DerivedRule* _left;
    DerivedRule* _top;
    DerivedRule* _right;
    DerivedRule* _bottom;

    de::AnimatorVector2 _normalizedAnchorPoint;
    const Rule* _anchorXRule;
    const Rule* _anchorYRule;
    const Rule* _leftRule;
    const Rule* _topRule;
    const Rule* _rightRule;
    const Rule* _bottomRule;
    const Rule* _widthRule;
    const Rule* _heightRule;
};

#endif // RECTANGLERULE_H
