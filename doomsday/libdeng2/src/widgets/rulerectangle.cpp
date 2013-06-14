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

#include "de/RuleRectangle"
#include "de/DelegateRule"
#include "de/App"
#include "de/math.h"

namespace de {

DENG2_PIMPL(RuleRectangle),
DENG2_OBSERVES(Clock, TimeChange),
public DelegateRule::ISource
{
    // Internal identifiers for the output rules.
    enum OutputIds
    {
        OutLeft,
        OutRight,
        OutWidth,

        OutTop,
        OutBottom,
        OutHeight,

        MAX_OUTPUT_RULES,

        // Ranges:
        FIRST_HORIZ_OUTPUT = OutLeft,
        LAST_HORIZ_OUTPUT  = OutWidth,
        FIRST_VERT_OUTPUT  = OutTop,
        LAST_VERT_OUTPUT   = OutHeight
    };

    AnimationVector2 normalizedAnchorPoint;
    Rule const *inputRules[Rule::MAX_SEMANTICS];

    // The output rules.
    DelegateRule *outputRules[MAX_OUTPUT_RULES];

    Instance(Public *i) : Base(i)
    {
        zap(inputRules);

        // Create the output rules.
        for(int i = 0; i < int(MAX_OUTPUT_RULES); ++i)
        {
            outputRules[i] = new DelegateRule(*this, i);
        }
    }

    ~Instance()
    {
        for(int i = 0; i < int(Rule::MAX_SEMANTICS); ++i)
        {
            connectInputToOutputs(Rule::Semantic(i), false);
        }
        for(int i = 0; i < int(MAX_OUTPUT_RULES); ++i)
        {
            outputRules[i]->setSource(0);
            releaseRef(outputRules[i]);
        }
    }

    Rule const *&ruleRef(Rule::Semantic rule)
    {
        DENG2_ASSERT(rule >= Rule::Left);
        DENG2_ASSERT(rule < Rule::MAX_SEMANTICS);

        return inputRules[rule];
    }

    void invalidateOutputs()
    {
        for(int i = 0; i < int(MAX_OUTPUT_RULES); ++i)
        {
            outputRules[i]->invalidate();
        }
    }

    void connectInputToOutputs(Rule::Semantic inputRule, bool doConnect)
    {
        Rule const *&input = ruleRef(inputRule);
        if(!input) return;

        bool isHoriz = (inputRule == Rule::Left  || inputRule == Rule::Right ||
                        inputRule == Rule::Width || inputRule == Rule::AnchorX);

        int const start = (isHoriz? FIRST_HORIZ_OUTPUT : FIRST_VERT_OUTPUT);
        int const end   = (isHoriz? LAST_HORIZ_OUTPUT  : LAST_VERT_OUTPUT );

        for(int i = start; i <= end; ++i)
        {
            if(doConnect)
            {
                outputRules[i]->dependsOn(input);
                outputRules[i]->invalidate();
            }
            else
            {
                outputRules[i]->independentOf(input);
            }
        }
    }

    void setInputRule(Rule::Semantic inputRule, Rule const &rule)
    {
        // Disconnect the old input rule from relevant outputs.
        connectInputToOutputs(inputRule, false);

        ruleRef(inputRule) = &rule;

        // Connect to relevant outputs.
        connectInputToOutputs(inputRule, true);
    }

    void clearInputRule(Rule::Semantic inputRule)
    {
        connectInputToOutputs(inputRule, false);
        ruleRef(inputRule) = 0;
    }

    void updateWidth()
    {
        if(inputRules[Rule::Width])
        {
            outputRules[OutWidth]->set(inputRules[Rule::Width]->value());
        }
        else
        {
            // Need to calculate width using edges.
            updateHorizontal();
        }
    }

    void updateHorizontal()
    {
        // Both edges must be defined, otherwise the rectangle's position is ambiguous.
        bool leftDefined   = false;
        bool rightDefined  = false;

        Rectanglef r;

        if(inputRules[Rule::AnchorX] && inputRules[Rule::Width])
        {
            r.topLeft.x = inputRules[Rule::AnchorX]->value() -
                    normalizedAnchorPoint.x * inputRules[Rule::Width]->value();
            r.setWidth(inputRules[Rule::Width]->value());
            leftDefined = rightDefined = true;
        }

        if(inputRules[Rule::Left])
        {
            r.topLeft.x = inputRules[Rule::Left]->value();
            leftDefined = true;
        }
        if(inputRules[Rule::Right])
        {
            r.bottomRight.x = inputRules[Rule::Right]->value();
            rightDefined = true;
        }

        if(inputRules[Rule::Width] && leftDefined && !rightDefined)
        {
            r.setWidth(inputRules[Rule::Width]->value());
            rightDefined = true;
        }
        if(inputRules[Rule::Width] && !leftDefined && rightDefined)
        {
            r.topLeft.x = r.bottomRight.x - inputRules[Rule::Width]->value();
            leftDefined = true;
        }

#ifdef _DEBUG
        if(!leftDefined || !rightDefined)
        {
            qDebug() << self.description().toLatin1();
        }
#endif

        DENG2_ASSERT(leftDefined);
        DENG2_ASSERT(rightDefined);

        outputRules[OutLeft]->set(r.topLeft.x);
        outputRules[OutRight]->set(r.bottomRight.x);
        outputRules[OutWidth]->set(r.width());
    }

    void updateHeight()
    {
        if(inputRules[Rule::Height])
        {
            outputRules[OutHeight]->set(inputRules[Rule::Height]->value());
        }
        else
        {
            // Need to calculate width using edges.
            updateVertical();
        }
    }

    void updateVertical()
    {
        // Both edges must be defined, otherwise the rectangle's position is ambiguous.
        bool topDefined    = false;
        bool bottomDefined = false;

        Rectanglef r;

        if(inputRules[Rule::AnchorY] && inputRules[Rule::Height])
        {
            r.topLeft.y = inputRules[Rule::AnchorY]->value() -
                    normalizedAnchorPoint.y * inputRules[Rule::Height]->value();
            r.setHeight(inputRules[Rule::Height]->value());
            topDefined = bottomDefined = true;
        }

        if(inputRules[Rule::Top])
        {
            r.topLeft.y = inputRules[Rule::Top]->value();
            topDefined = true;
        }
        if(inputRules[Rule::Bottom])
        {
            r.bottomRight.y = inputRules[Rule::Bottom]->value();
            bottomDefined = true;
        }

        if(inputRules[Rule::Height] && topDefined && !bottomDefined)
        {
            r.setHeight(inputRules[Rule::Height]->value());
            bottomDefined = true;
        }
        if(inputRules[Rule::Height] && !topDefined && bottomDefined)
        {
            r.topLeft.y = r.bottomRight.y - inputRules[Rule::Height]->value();
            topDefined = true;
        }

#ifdef _DEBUG
        if(!topDefined || !bottomDefined)
        {
            qDebug() << self.description().toLatin1();
        }
#endif
        DENG2_ASSERT(topDefined);
        DENG2_ASSERT(bottomDefined);

        // Update the derived output rules.
        outputRules[OutTop]->set(r.topLeft.y);
        outputRules[OutBottom]->set(r.bottomRight.y);
        outputRules[OutHeight]->set(r.height());
    }

    // Implements DelegateRule::ISource.
    void delegateUpdate(int id)
    {
        switch(id)
        {
        case OutLeft:
        case OutRight:
            updateHorizontal();
            break;

        case OutWidth:
            updateWidth();
            break;

        case OutTop:
        case OutBottom:
            updateVertical();
            break;

        case OutHeight:
            updateHeight();
            break;
        }
    }

    void delegateInvalidation(int id)
    {
        // Due to the intrinsic relationships between the outputs (as edges of
        // a rectangle), invalidation of one may cause others to become
        // invalid, too.
        switch(id)
        {
        case OutLeft:
            outputRules[OutRight]->invalidate();
            outputRules[OutWidth]->invalidate();
            break;

        case OutRight:
            outputRules[OutLeft]->invalidate();
            outputRules[OutWidth]->invalidate();
            break;

        case OutWidth:
            outputRules[OutLeft]->invalidate();
            outputRules[OutRight]->invalidate();
            break;

        case OutTop:
            outputRules[OutBottom]->invalidate();
            outputRules[OutHeight]->invalidate();
            break;

        case OutBottom:
            outputRules[OutTop]->invalidate();
            outputRules[OutHeight]->invalidate();
            break;

        case OutHeight:
            outputRules[OutTop]->invalidate();
            outputRules[OutBottom]->invalidate();
            break;
        }
    }

    String delegateDescription(int id) const
    {
        static char const *names[MAX_OUTPUT_RULES] = {
            "Left output",
            "Right output",
            "Width output",
            "Top output",
            "Bottom output",
            "Height output"
        };
        return String(names[id]) + " of RuleRectangle " +
               QString("0x%1").arg(dintptr(thisPublic), 0, 16);
    }

    void timeChanged(Clock const &clock)
    {
        invalidateOutputs();

        if(normalizedAnchorPoint.done())
        {
            clock.audienceForPriorityTimeChange -= this;
        }
    }
};

RuleRectangle::RuleRectangle() : d(new Instance(this))
{}

Rule const &RuleRectangle::left() const
{
    return *d->outputRules[Instance::OutLeft];
}

Rule const &RuleRectangle::top() const
{
    return *d->outputRules[Instance::OutTop];
}

Rule const &RuleRectangle::right() const
{
    return *d->outputRules[Instance::OutRight];
}

Rule const &RuleRectangle::bottom() const
{
    return *d->outputRules[Instance::OutBottom];
}

Rule const &RuleRectangle::width() const
{
    return *d->outputRules[Instance::OutWidth];
}

Rule const &RuleRectangle::height() const
{
    return *d->outputRules[Instance::OutHeight];
}

RuleRectangle &RuleRectangle::setInput(Rule::Semantic inputRule, Rule const &rule)
{
    d->setInputRule(inputRule, rule);
    return *this;
}

RuleRectangle &RuleRectangle::setLeftTop(Rule const &left, Rule const &top)
{
    setInput(Rule::Left, left);
    setInput(Rule::Top,  top);
    return *this;
}

RuleRectangle &RuleRectangle::setRightBottom(Rule const &right, Rule const &bottom)
{
    setInput(Rule::Right,  right);
    setInput(Rule::Bottom, bottom);
    return *this;
}

RuleRectangle &RuleRectangle::setRect(RuleRectangle const &rect)
{
    setInput(Rule::Left,   rect.left());
    setInput(Rule::Top,    rect.top());
    setInput(Rule::Right,  rect.right());
    setInput(Rule::Bottom, rect.bottom());
    return *this;
}

RuleRectangle &RuleRectangle::setSize(Rule const &width, Rule const &height)
{
    setInput(Rule::Width,  width);
    setInput(Rule::Height, height);
    return *this;
}

void RuleRectangle::clearInput(Rule::Semantic inputRule)
{
    d->clearInputRule(inputRule);
}

Rule const &RuleRectangle::inputRule(Rule::Semantic inputRule)
{
    return *d->ruleRef(inputRule);
}

void RuleRectangle::setAnchorPoint(Vector2f const &normalizedPoint, TimeDelta const &transition)
{
    d->normalizedAnchorPoint.setValue(normalizedPoint, transition);
    d->invalidateOutputs();

    if(transition > 0.0)
    {
        // Animation started, keep an eye on the clock until it ends.
        Clock::appClock().audienceForPriorityTimeChange += d;
    }
}

Rectanglef RuleRectangle::rect() const
{
    return Rectanglef(Vector2f(left().value(),  top().value()),
                      Vector2f(right().value(), bottom().value()));
}

Rectanglei RuleRectangle::recti() const
{
    Rectanglef const r = rect();
    return Rectanglei(Vector2i(de::floor(r.topLeft.x),     de::floor(r.topLeft.y)),
                      Vector2i(de::floor(r.bottomRight.x), de::floor(r.bottomRight.y)));
}

String RuleRectangle::description() const
{
    String desc = String("RuleRectangle 0x%1:").arg(dintptr(this), 0, 16);

    for(int i = 0; i < int(Rule::MAX_SEMANTICS); ++i)
    {
        desc += String("\n - ") +
                (i == Rule::Left? "Left" :
                 i == Rule::Top? "Top" :
                 i == Rule::Right? "Right" :
                 i == Rule::Bottom? "Bottom" :
                 i == Rule::Width? "Width" :
                 i == Rule::Height? "Height" :
                 i == Rule::AnchorX? "AnchorX" : "AnchorY") + ": ";

        if(d->inputRules[i])
        {
            desc += d->inputRules[i]->description();
        }
        else
        {
            desc += "(null)";
        }
    }
    return desc;
}

} // namespace de
