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

#include "de/RectangleRule"
#include "de/App"

namespace de {

RectangleRule::RectangleRule(QObject *parent)
    : Rule(parent),
      _anchorXRule(0),
      _anchorYRule(0),
      _leftRule(0),
      _topRule(0),
      _rightRule(0),
      _bottomRule(0),
      _widthRule(0),
      _heightRule(0)
{
    setup();
}

RectangleRule::RectangleRule(Rule const *left, Rule const *top, Rule const *right, Rule const *bottom, QObject *parent)
    : Rule(parent),
      _anchorXRule(0),
      _anchorYRule(0),
      _leftRule(left),
      _topRule(top),
      _rightRule(right),
      _bottomRule(bottom),
      _widthRule(0),
      _heightRule(0)
{
    setup();

    dependsOn(left);
    dependsOn(top);
    dependsOn(right);
    dependsOn(bottom);
}

RectangleRule::RectangleRule(RectangleRule const *rect, QObject *parent)
    : Rule(parent),
      _anchorXRule(0),
      _anchorYRule(0),
      _leftRule(rect->left()),
      _topRule(rect->top()),
      _rightRule(rect->right()),
      _bottomRule(rect->bottom()),
      _widthRule(0),
      _heightRule(0)
{
    setup();

    dependsOn(_leftRule);
    dependsOn(_topRule);
    dependsOn(_rightRule);
    dependsOn(_bottomRule);
}

void RectangleRule::setup()
{
    // When the application's time changes, check whether this rule
    // needs to be invalidated.
    connect(&App::clock(), SIGNAL(timeChanged()), this, SLOT(currentTimeChanged()));

    // The output rules.
    _left   = new DerivedRule(this, this);
    _right  = new DerivedRule(this, this);
    _top    = new DerivedRule(this, this);
    _bottom = new DerivedRule(this, this);

    invalidate();
}

Rule const *RectangleRule::left() const
{
    return _left;
}

Rule const *RectangleRule::top() const
{
    return _top;
}

Rule const *RectangleRule::right() const
{
    return _right;
}

Rule const *RectangleRule::bottom() const
{
    return _bottom;
}

Rule const **RectangleRule::ruleRef(InputRule inputRule)
{
    switch(inputRule)
    {
    case Left:
        return &_leftRule;

    case Right:
        return &_rightRule;

    case Top:
        return &_topRule;

    case Bottom:
        return &_bottomRule;

    case Width:
        return &_widthRule;

    case Height:
        return &_heightRule;

    case AnchorX:
        return &_anchorXRule;

    default:
        return &_anchorYRule;
    }
}

void RectangleRule::setRule(InputRule inputRule, Rule *rule)
{
    DENG2_ASSERT(rule->parent() == 0);

    // Take ownership.
    rule->setParent(this);

    Rule **input = const_cast<Rule **>(ruleRef(inputRule));
    if(*input)
    {
        // Move the existing dependency.
        (*input)->replace(rule);
        delete *input;
    }
    else
    {
        // Define a new dependency.
        dependsOn(rule);
    }
    *input = rule;
}

void RectangleRule::dependencyReplaced(Rule const *oldRule, Rule const *newRule)
{
    for(int i = 0; i < MAX_RULES; ++i)
    {
        Rule const **dep = ruleRef(InputRule(i));
        if(*dep == oldRule)
        {
            *dep = newRule;
        }
    }
}

Rule const *RectangleRule::inputRule(InputRule inputRule)
{
    return *ruleRef(inputRule);
}

void RectangleRule::setAnchorPoint(Vector2f const &normalizedPoint, Time::Delta const &transition)
{
    _normalizedAnchorPoint.setValue(normalizedPoint, transition);
    invalidate();
}

void RectangleRule::update()
{
    // All the edges must be defined, otherwise the visual's position is ambiguous.
    bool leftDefined   = false;
    bool topDefined    = false;
    bool rightDefined  = false;
    bool bottomDefined = false;

    Rectanglef r;

    if(_anchorXRule && _widthRule)
    {
        r.topLeft.x = _anchorXRule->value() - _normalizedAnchorPoint.x * _widthRule->value();
        r.setWidth(_widthRule->value());
        leftDefined = rightDefined = true;
    }

    if(_anchorYRule && _heightRule)
    {
        r.topLeft.y = _anchorYRule->value() - _normalizedAnchorPoint.y * _heightRule->value();
        r.setHeight(_heightRule->value());
        topDefined = bottomDefined = true;
    }

    if(_leftRule)
    {
        r.topLeft.x = _leftRule->value();
        leftDefined = true;
    }
    if(_topRule)
    {
        r.topLeft.y = _topRule->value();
        topDefined = true;
    }
    if(_rightRule)
    {
        r.bottomRight.x = _rightRule->value();
        rightDefined = true;
    }
    if(_bottomRule)
    {
        r.bottomRight.y = _bottomRule->value();
        bottomDefined = true;
    }

    if(_widthRule && leftDefined && !rightDefined)
    {
        r.setWidth(_widthRule->value());
        rightDefined = true;
    }
    if(_widthRule && !leftDefined && rightDefined)
    {
        r.topLeft.x = r.bottomRight.x - _widthRule->value();
        leftDefined = true;
    }

    if(_heightRule && topDefined && !bottomDefined)
    {
        r.setHeight(_heightRule->value());
        bottomDefined = true;
    }
    if(_heightRule && !topDefined && bottomDefined)
    {
        r.topLeft.y = r.bottomRight.y - _heightRule->value();
        topDefined = true;
    }

    DENG2_ASSERT(leftDefined);
    DENG2_ASSERT(rightDefined);
    DENG2_ASSERT(topDefined);
    DENG2_ASSERT(bottomDefined);

    // Update the derived output rules.
    _left->set(r.topLeft.x);
    _top->set(r.topLeft.y);
    _right->set(r.bottomRight.x);
    _bottom->set(r.bottomRight.y);

    // Mark this rule as valid.
    setValue(r.width() * r.height());
}

void RectangleRule::currentTimeChanged()
{
    if(!_normalizedAnchorPoint.done())
    {
        invalidate();
    }
}

Rectanglef RectangleRule::rect() const
{
    return Rectanglef(Vector2f(_left->value(),  _top->value()),
                      Vector2f(_right->value(), _bottom->value()));
}

} // namespace de
