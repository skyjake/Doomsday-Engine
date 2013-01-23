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
    /*
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
    */

    RectangleRule &self;

    DerivedRule *left;
    DerivedRule *top;
    DerivedRule *right;
    DerivedRule *bottom;

    AnimationVector2 normalizedAnchorPoint;

    Rule const *inputRules[MAX_RULES];
    /*
    Rule const *anchorXRule;
    Rule const *anchorYRule;
    Rule const *leftRule;
    Rule const *topRule;
    Rule const *rightRule;
    Rule const *bottomRule;
    Rule const *widthRule;
    Rule const *heightRule;
    */

    Instance(RectangleRule &rr) : self(rr)
    {
        memset(inputRules, 0, sizeof(inputRules));
        setup();
    }

    Instance(RectangleRule &rr, Rule const *left, Rule const *top, Rule const *right, Rule const *bottom)
        : self(rr)
    {
        memset(inputRules, 0, sizeof(inputRules));

        inputRules[Left]   = left;
        inputRules[Top]    = top;
        inputRules[Right]  = right;
        inputRules[Bottom] = bottom;

        setup();
    }

    ~Instance()
    {
        de::releaseRef(left);
        de::releaseRef(right);
        de::releaseRef(top);
        de::releaseRef(bottom);
    }

    void setup()
    {
        // Depend on all specified input rules.
        for(int i = 0; i < int(MAX_RULES); ++i)
        {
            if(inputRules[i]) self.dependsOn(inputRules[i]);
        }

        // When the application's time changes, check whether this rule
        // needs to be invalidated.
        //connect(&Clock::appClock(), SIGNAL(timeChanged()), this, SLOT(currentTimeChanged()));

        // The output rules.
        left   = new DerivedRule(&self);
        right  = new DerivedRule(&self);
        top    = new DerivedRule(&self);
        bottom = new DerivedRule(&self);

        self.invalidate();
    }

    Rule const **ruleRef(InputRule rule)
    {
        DENG2_ASSERT(rule < MAX_RULES);
        return &inputRules[rule];
    }

    void setInputRule(InputRule inputRule, Rule const *rule)
    {
        DENG2_ASSERT(rule != 0);

        Rule const **input = ruleRef(inputRule);
        if(*input)
        {
            self.independentOf(*input);
        }

        // Define a new dependency.
        *input = rule;
        self.dependsOn(rule);
    }

    void update()
    {
        // All the edges must be defined, otherwise the rectangle's position is ambiguous.
        bool leftDefined   = false;
        bool topDefined    = false;
        bool rightDefined  = false;
        bool bottomDefined = false;

        Rectanglef r;

        if(inputRules[AnchorX] && inputRules[Width])
        {
            r.topLeft.x = inputRules[AnchorX]->value() -
                    normalizedAnchorPoint.x * inputRules[Width]->value();
            r.setWidth(inputRules[Width]->value());
            leftDefined = rightDefined = true;
        }

        if(inputRules[AnchorY] && inputRules[Height])
        {
            r.topLeft.y = inputRules[AnchorY]->value() -
                    normalizedAnchorPoint.y * inputRules[Height]->value();
            r.setHeight(inputRules[Height]->value());
            topDefined = bottomDefined = true;
        }

        if(inputRules[Left])
        {
            r.topLeft.x = inputRules[Left]->value();
            leftDefined = true;
        }
        if(inputRules[Top])
        {
            r.topLeft.y = inputRules[Top]->value();
            topDefined = true;
        }
        if(inputRules[Right])
        {
            r.bottomRight.x = inputRules[Right]->value();
            rightDefined = true;
        }
        if(inputRules[Bottom])
        {
            r.bottomRight.y = inputRules[Bottom]->value();
            bottomDefined = true;
        }

        if(inputRules[Width] && leftDefined && !rightDefined)
        {
            r.setWidth(inputRules[Width]->value());
            rightDefined = true;
        }
        if(inputRules[Width] && !leftDefined && rightDefined)
        {
            r.topLeft.x = r.bottomRight.x - inputRules[Width]->value();
            leftDefined = true;
        }

        if(inputRules[Height] && topDefined && !bottomDefined)
        {
            r.setHeight(inputRules[Height]->value());
            bottomDefined = true;
        }
        if(inputRules[Height] && !topDefined && bottomDefined)
        {
            r.topLeft.y = r.bottomRight.y - inputRules[Height]->value();
            topDefined = true;
        }

        DENG2_ASSERT(leftDefined);
        DENG2_ASSERT(rightDefined);
        DENG2_ASSERT(topDefined);
        DENG2_ASSERT(bottomDefined);

        // Update the derived output rules.
        left->set(r.topLeft.x);
        top->set(r.topLeft.y);
        right->set(r.bottomRight.x);
        bottom->set(r.bottomRight.y);

        // Mark this rule as valid.
        self.setValue(r.width() * r.height());
    }
};

RectangleRule::RectangleRule()
    : Rule(), d(new Instance(*this))
{}

RectangleRule::RectangleRule(Rule const *left, Rule const *top, Rule const *right, Rule const *bottom)
    : Rule(), d(new Instance(*this, left, top, right, bottom))
{}

RectangleRule::RectangleRule(RectangleRule const *rect)
    : Rule(), d(new Instance(*this, rect->left(), rect->top(), rect->right(), rect->bottom()))
{}

RectangleRule::~RectangleRule()
{
    delete d;
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

RectangleRule &RectangleRule::setInput(InputRule inputRule, Rule const *rule)
{
    d->setInputRule(inputRule, rule);
    return *this;
}

/*
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
*/

Rule const *RectangleRule::inputRule(InputRule inputRule)
{
    return *d->ruleRef(inputRule);
}

void RectangleRule::setAnchorPoint(Vector2f const &normalizedPoint, TimeDelta const &transition)
{
    d->normalizedAnchorPoint.setValue(normalizedPoint, transition);
    invalidate();

    if(transition > 0.0)
    {
        connect(&Clock::appClock(), SIGNAL(timeChanged()), this, SLOT(timeChanged()));
    }
}

void RectangleRule::update()
{
    d->update();
}

void RectangleRule::timeChanged()
{
    invalidate();

    if(d->normalizedAnchorPoint.done())
    {
        disconnect(this, SLOT(timeChanged()));
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
