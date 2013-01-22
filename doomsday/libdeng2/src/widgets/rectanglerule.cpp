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
#include "de/math.h"

namespace de {

struct RectangleRule::Instance
{
    struct InputRuleRef
    {
        union {
            Rule *ptr;
            Rule const *constPtr;
        } rule;
        bool isOwned;

        InputRuleRef(Rule const *cr = 0) : isOwned(false) {
            rule.constPtr = cr;
        }
        InputRuleRef(Rule *r, bool owned) : isOwned(owned) {
            rule.ptr = r;
        }
        float value() const {
            if(!rule.constPtr) return 0;
            return rule.constPtr->value();
        }
        operator bool () const {
            return rule.constPtr != 0;
        }
        operator Rule const * () const {
            return rule.constPtr;
        }
    };

    RectangleRule &self;

    DerivedRule *left;
    DerivedRule *top;
    DerivedRule *right;
    DerivedRule *bottom;

    AnimationVector2 normalizedAnchorPoint;

    InputRuleRef anchorXRule;
    InputRuleRef anchorYRule;
    InputRuleRef leftRule;
    InputRuleRef topRule;
    InputRuleRef rightRule;
    InputRuleRef bottomRule;
    InputRuleRef widthRule;
    InputRuleRef heightRule;

    Instance(RectangleRule &rr) : self(rr) {}

    Instance(RectangleRule &rr, Rule const *left, Rule const *top, Rule const *right, Rule const *bottom)
        : self(rr),
          leftRule(left),
          topRule(top),
          rightRule(right),
          bottomRule(bottom)
    {}

    InputRuleRef &ruleRef(InputRule rule)
    {
        switch(rule)
        {
        case Left:
            return leftRule;

        case Right:
            return rightRule;

        case Top:
            return topRule;

        case Bottom:
            return bottomRule;

        case Width:
            return widthRule;

        case Height:
            return heightRule;

        case AnchorX:
            return anchorXRule;

        default:
            return anchorYRule;
        }
    }

    void setInputRule(InputRule inputRule, Rule *rule, bool owned)
    {
        DENG2_ASSERT(rule != 0);

        InputRuleRef &input = ruleRef(inputRule);
        if(input.rule.ptr)
        {
            // Move the existing dependencies to the new rule.
            //input.rule.ptr->transferDependencies(rule);

            if(input.isOwned)
            {
                // We own this rule, so let's get rid of it now.
                DENG2_ASSERT(input.rule.ptr->parent() == &self);
                delete input.rule.ptr;
            }
        }
        else
        {
            // Define a new dependency.
            self.dependsOn(rule);
        }

        input.rule.ptr = rule;
        input.isOwned  = owned;

        DENG2_ASSERT(input.rule.ptr);
        DENG2_ASSERT(!input.isOwned || input.rule.ptr->parent() == &self);
    }
};

RectangleRule::RectangleRule(QObject *parent)
    : Rule(parent), d(new Instance(*this))
{
    setup();
}

RectangleRule::RectangleRule(Rule const *left, Rule const *top, Rule const *right, Rule const *bottom, QObject *parent)
    : Rule(parent), d(new Instance(*this, left, top, right, bottom))
{
    setup();

    dependsOn(left);
    dependsOn(top);
    dependsOn(right);
    dependsOn(bottom);
}

RectangleRule::RectangleRule(RectangleRule const *rect, QObject *parent)
    : Rule(parent), d(new Instance(*this, rect->left(), rect->top(), rect->right(), rect->bottom()))
{
    setup();

    dependsOn(d->leftRule);
    dependsOn(d->topRule);
    dependsOn(d->rightRule);
    dependsOn(d->bottomRule);
}

void RectangleRule::setup()
{
    // When the application's time changes, check whether this rule
    // needs to be invalidated.
    connect(&Clock::appClock(), SIGNAL(timeChanged()), this, SLOT(currentTimeChanged()));

    // The output rules.
    d->left   = new DerivedRule(this, this);
    d->right  = new DerivedRule(this, this);
    d->top    = new DerivedRule(this, this);
    d->bottom = new DerivedRule(this, this);

    invalidate();
}

Rule const *RectangleRule::left() const
{
    return d->left;
}

Rule const *RectangleRule::top() const
{
    return d->top;
}

Rule const *RectangleRule::right() const
{
    return d->right;
}

Rule const *RectangleRule::bottom() const
{
    return d->bottom;
}

RectangleRule &RectangleRule::setInput(InputRule inputRule, Rule *ruleOwn)
{
    DENG2_ASSERT(ruleOwn != 0);

    // Claim ownership.
    bool owned = false;
    if(claim(ruleOwn))
    {
        owned = true;
    }

    d->setInputRule(inputRule, ruleOwn, owned);
    return *this;
}

RectangleRule &RectangleRule::setInput(InputRule inputRule, Rule const *rule)
{
    d->setInputRule(inputRule, const_cast<Rule *>(rule), false);
    return *this;
}

void RectangleRule::dependencyReplaced(Rule const *oldRule, Rule const *newRule)
{
    for(int i = 0; i < MAX_RULES; ++i)
    {
        Instance::InputRuleRef &dep = d->ruleRef(InputRule(i));
        if(dep.rule.constPtr == oldRule)
        {
            dep.rule.constPtr = newRule;
        }
    }
}

Rule const *RectangleRule::inputRule(InputRule inputRule)
{
    return d->ruleRef(inputRule).rule.constPtr;
}

void RectangleRule::setAnchorPoint(Vector2f const &normalizedPoint, Time::Delta const &transition)
{
    d->normalizedAnchorPoint.setValue(normalizedPoint, transition);
    invalidate();
}

void RectangleRule::update()
{
    // All the edges must be defined, otherwise the rectangle's position is ambiguous.
    bool leftDefined   = false;
    bool topDefined    = false;
    bool rightDefined  = false;
    bool bottomDefined = false;

    Rectanglef r;

    if(d->anchorXRule && d->widthRule)
    {
        r.topLeft.x = d->anchorXRule.value() - d->normalizedAnchorPoint.x * d->widthRule.value();
        r.setWidth(d->widthRule.value());
        leftDefined = rightDefined = true;
    }

    if(d->anchorYRule && d->heightRule)
    {
        r.topLeft.y = d->anchorYRule.value() - d->normalizedAnchorPoint.y * d->heightRule.value();
        r.setHeight(d->heightRule.value());
        topDefined = bottomDefined = true;
    }

    if(d->leftRule)
    {
        r.topLeft.x = d->leftRule.value();
        leftDefined = true;
    }
    if(d->topRule)
    {
        r.topLeft.y = d->topRule.value();
        topDefined = true;
    }
    if(d->rightRule)
    {
        r.bottomRight.x = d->rightRule.value();
        rightDefined = true;
    }
    if(d->bottomRule)
    {
        r.bottomRight.y = d->bottomRule.value();
        bottomDefined = true;
    }

    if(d->widthRule && leftDefined && !rightDefined)
    {
        r.setWidth(d->widthRule.value());
        rightDefined = true;
    }
    if(d->widthRule && !leftDefined && rightDefined)
    {
        r.topLeft.x = r.bottomRight.x - d->widthRule.value();
        leftDefined = true;
    }

    if(d->heightRule && topDefined && !bottomDefined)
    {
        r.setHeight(d->heightRule.value());
        bottomDefined = true;
    }
    if(d->heightRule && !topDefined && bottomDefined)
    {
        r.topLeft.y = r.bottomRight.y - d->heightRule.value();
        topDefined = true;
    }

    DENG2_ASSERT(leftDefined);
    DENG2_ASSERT(rightDefined);
    DENG2_ASSERT(topDefined);
    DENG2_ASSERT(bottomDefined);

    // Update the derived output rules.
    d->left->set(r.topLeft.x);
    d->top->set(r.topLeft.y);
    d->right->set(r.bottomRight.x);
    d->bottom->set(r.bottomRight.y);

    // Mark this rule as valid.
    setValue(r.width() * r.height());
}

void RectangleRule::currentTimeChanged()
{
    if(!d->normalizedAnchorPoint.done())
    {
        invalidate();
    }
}

Rectanglef RectangleRule::rect() const
{
    return Rectanglef(Vector2f(d->left->value(),  d->top->value()),
                      Vector2f(d->right->value(), d->bottom->value()));
}

Rectanglei RectangleRule::recti() const
{
    Rectanglef const r = rect();
    return Rectanglei(Vector2i(de::floor(r.topLeft.x),     de::floor(r.topLeft.y)),
                      Vector2i(de::floor(r.bottomRight.x), de::floor(r.bottomRight.y)));
}

} // namespace de
