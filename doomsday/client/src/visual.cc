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

#include "visual.h"
#include "rule.h"

using namespace de;

Visual::Visual()
    : _parent(0),
      _anchorXRule(0),
      _anchorYRule(0),
      _leftRule(0),
      _topRule(0),
      _rightRule(0),
      _bottomRule(0),
      _widthRule(0),
      _heightRule(0)
{}

Visual::~Visual()
{
    if(_parent)
    {
        _parent->remove(this);
    }
    clear();
}

void Visual::clear()
{
    for(Children::iterator i = _children.begin(); i != _children.end(); ++i)
    {
        (*i)->_parent = 0;
        delete *i;
    }
    _children.clear();
}

Visual* Visual::add(Visual* visual)
{
    _children.push_back(visual);
    visual->_parent = this;
    return visual;
}

Visual* Visual::remove(Visual* visual)
{
    visual->_parent = 0;
    _children.remove(visual);
    return visual;
}

Rule** Visual::ruleRef(PlacementRule placementRule)
{
    switch(placementRule)
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

void Visual::setRule(PlacementRule placementRule, Rule* rule)
{
    Q_ASSERT(rule->parent() == 0);

    // Take ownership.
    rule->setParent(this);

    Rule** oldRule = ruleRef(placementRule);
    if(*oldRule)
    {
        (*oldRule)->replace(rule);
        delete *oldRule;
    }
    *oldRule = rule;
}

Rule* Visual::rule(PlacementRule placementRule)
{
    return *ruleRef(placementRule);
}

void Visual::setAnchorPoint(const de::Vector2f& normalizedPoint)
{
    _normalizedAnchorPoint = normalizedPoint;
}

de::Rectanglef Visual::rect() const
{
    // All the edges must be defined, otherwise the visual's position is ambiguous.
    bool leftDefined = false;
    bool topDefined = false;
    bool rightDefined = false;
    bool bottomDefined = false;

    de::Rectanglef r;

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
        r.bottomRight.y = _rightRule->value();
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

    if(!leftDefined || !rightDefined || !topDefined || !bottomDefined)
    {
        // As a fallback, use the parent's rectangle.
        Q_ASSERT(_parent != 0);

        r = _parent->rect();
    }

    return r;
}

void Visual::draw() const
{
    drawSelf(BeforeChildren);
    for(Children::const_iterator i = _children.begin(); i != _children.end(); ++i)
    {
        (*i)->draw();
    }
    drawSelf(AfterChildren);
}

void Visual::drawSelf(DrawingStage /*stage*/) const
{}
