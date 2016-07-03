/*
 * The Doomsday Engine Project
 *
 * Copyright © 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/RuleRectangle"
#include "de/App"
#include "de/math.h"

namespace de {

DENG2_PIMPL(RuleRectangle)
{
    String debugName;

    // Internal identifiers for the output rules.
    enum OutputId
    {
        OutLeft,
        OutRight,
        OutWidth,

        OutTop,
        OutBottom,
        OutHeight,

        MAX_OUTPUT_RULES
    };

    // The input rules.
    Rule const *inputRules[Rule::MAX_SEMANTICS];
    AnimationRule *_normalizedAnchorX = nullptr;
    AnimationRule *_normalizedAnchorY = nullptr;

    // The output rules.
    IndirectRule *outputRules[MAX_OUTPUT_RULES];
    Rule *_midX = nullptr;
    Rule *_midY = nullptr;

    Impl(Public *i) : Base(i)
    {
        zap(inputRules);

        // Create the output rules.
        for (int i = 0; i < int(MAX_OUTPUT_RULES); ++i)
        {
            outputRules[i] = new IndirectRule;
        }

        debugName = QString("0x%1").arg(dintptr(thisPublic), 0, 16);
    }

    ~Impl()
    {
        releaseRef(_midX);
        releaseRef(_midY);
        releaseRef(_normalizedAnchorX);
        releaseRef(_normalizedAnchorY);

        for (int i = 0; i < int(Rule::MAX_SEMANTICS); ++i)
        {
            releaseRef(inputRules[i]);
        }
        for (int i = 0; i < int(MAX_OUTPUT_RULES); ++i)
        {
            outputRules[i]->unsetSource();
            releaseRef(outputRules[i]);
        }
    }

    Rule const *&ruleRef(Rule::Semantic rule)
    {
        DENG2_ASSERT(rule >= Rule::Left);
        DENG2_ASSERT(rule < Rule::MAX_SEMANTICS);

        return inputRules[rule];
    }

    AnimationRule *normalizedAnchorX()
    {
        if (!_normalizedAnchorX) _normalizedAnchorX = new AnimationRule(0);
        return _normalizedAnchorX;
    }

    AnimationRule *normalizedAnchorY()
    {
        if (!_normalizedAnchorY) _normalizedAnchorY = new AnimationRule(0);
        return _normalizedAnchorY;
    }

    Rule *midX()
    {
        if (!_midX) _midX = holdRef(*outputRules[OutLeft] + *outputRules[OutWidth] / 2);
        return _midX;
    }

    Rule *midY()
    {
        if (!_midY) _midY = holdRef(*outputRules[OutTop] + *outputRules[OutHeight] / 2);
        return _midY;
    }

    inline Rule const &anchorPos(Rule::Semantic anchorInput)
    {
        if (anchorInput == Rule::AnchorX)
        {
            return *normalizedAnchorX();
        }
        else
        {
            DENG2_ASSERT(anchorInput == Rule::AnchorY);
            return *normalizedAnchorY();
        }
    }

    inline static bool isHorizontalInput(Rule::Semantic inputRule)
    {
        return inputRule == Rule::Left  ||
               inputRule == Rule::Right ||
               inputRule == Rule::Width ||
               inputRule == Rule::AnchorX;
    }

    inline static bool isVerticalInput(Rule::Semantic inputRule)
    {
        return !isHorizontalInput(inputRule);
    }

    void setInputRule(Rule::Semantic inputRule, Rule const &rule)
    {
        releaseRef(inputRules[inputRule]);
        inputRules[inputRule] = holdRef(rule);

        updateForChangedInput(inputRule);
    }

    void clearInputRule(Rule::Semantic inputRule)
    {
        releaseRef(inputRules[inputRule]);

        updateForChangedInput(inputRule);
    }

    void updateForChangedInput(Rule::Semantic input)
    {
        if (isHorizontalInput(input))
        {
            updateHorizontalOutputs();
        }
        else
        {
            updateVerticalOutputs();
        }
    }

    void updateHorizontalOutputs()
    {
        updateDimension(Rule::Left, Rule::Right, Rule::Width, Rule::AnchorX,
                        OutLeft, OutRight, OutWidth);
    }

    void updateVerticalOutputs()
    {
        updateDimension(Rule::Top, Rule::Bottom, Rule::Height, Rule::AnchorY,
                        OutTop, OutBottom, OutHeight);
    }

    void updateDimension(Rule::Semantic minInput, Rule::Semantic maxInput,
                         Rule::Semantic deltaInput, Rule::Semantic anchorInput,
                         OutputId minOutput, OutputId maxOutput, OutputId deltaOutput)
    {
        // Both edges must be defined, otherwise the rectangle's position is ambiguous.
        bool minDefined   = false;
        bool maxDefined   = false;
        bool deltaDefined = false;

        // Forget the previous output rules.
        outputRules[minOutput]  ->unsetSource();
        outputRules[maxOutput]  ->unsetSource();
        outputRules[deltaOutput]->unsetSource();

        if (inputRules[deltaInput])
        {
            outputRules[deltaOutput]->setSource(*inputRules[deltaInput]);

            deltaDefined = true;
        }

        if (inputRules[minInput])
        {
            outputRules[minOutput]->setSource(*inputRules[minInput]);

            minDefined = true;
        }

        if (inputRules[maxInput])
        {
            outputRules[maxOutput]->setSource(*inputRules[maxInput]);

            maxDefined = true;
        }

        if (inputRules[anchorInput] && inputRules[deltaInput])
        {
            outputRules[minOutput]->setSource(*inputRules[anchorInput] -
                    anchorPos(anchorInput) * *inputRules[deltaInput]);

            minDefined = true;
        }

        // Calculate missing information from the defined outputs.
        if (deltaDefined && minDefined && !maxDefined)
        {
            outputRules[maxOutput]->setSource(*outputRules[minOutput] + *outputRules[deltaOutput]);

            maxDefined = true;
        }
        if (deltaDefined && !minDefined && maxDefined)
        {
            outputRules[minOutput]->setSource(*outputRules[maxOutput] - *outputRules[deltaOutput]);

            minDefined = true;
        }
        if (!deltaDefined && minDefined && maxDefined)
        {
            outputRules[deltaOutput]->setSource(*outputRules[maxOutput] - *outputRules[minOutput]);

            //deltaDefined = true;
        }
    }
};

RuleRectangle::RuleRectangle() : d(new Impl(this))
{}

Rule const &RuleRectangle::left() const
{
    return *d->outputRules[Impl::OutLeft];
}

Rule const &RuleRectangle::top() const
{
    return *d->outputRules[Impl::OutTop];
}

Rule const &RuleRectangle::right() const
{
    return *d->outputRules[Impl::OutRight];
}

Rule const &RuleRectangle::bottom() const
{
    return *d->outputRules[Impl::OutBottom];
}

Rule const &RuleRectangle::width() const
{
    return *d->outputRules[Impl::OutWidth];
}

Rule const &RuleRectangle::height() const
{
    return *d->outputRules[Impl::OutHeight];
}

Rule const &RuleRectangle::midX() const
{
    return *d->midX();
}

Rule const &RuleRectangle::midY() const
{
    return *d->midY();
}

RuleRectangle &RuleRectangle::setInput(Rule::Semantic inputRule, RefArg<Rule> rule)
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

RuleRectangle &RuleRectangle::setInputsFromRect(RuleRectangle const &rect)
{
    for (int i = 0; i < int(Rule::MAX_SEMANTICS); ++i)
    {
        if (rect.d->inputRules[i])
        {
            setInput(Rule::Semantic(i), *rect.d->inputRules[i]);
        }
        else
        {
            clearInput(Rule::Semantic(i));
        }
    }
    return *this;
}

RuleRectangle &RuleRectangle::setSize(Rule const &width, Rule const &height)
{
    setInput(Rule::Width,  width);
    setInput(Rule::Height, height);
    return *this;
}

RuleRectangle &RuleRectangle::setMidAnchorX(Rule const &middle)
{
    setInput(Rule::AnchorX, middle);
    d->normalizedAnchorX()->set(.5f);
    return *this;
}

RuleRectangle &RuleRectangle::setMidAnchorY(Rule const &middle)
{
    setInput(Rule::AnchorY, middle);
    d->normalizedAnchorY()->set(.5f);
    return *this;
}

RuleRectangle &RuleRectangle::clearInput(Rule::Semantic inputRule)
{
    d->clearInputRule(inputRule);
    return *this;
}

Rule const &RuleRectangle::inputRule(Rule::Semantic inputRule)
{
    DENG2_ASSERT(d->ruleRef(inputRule) != 0);
    return *d->ruleRef(inputRule);
}

void RuleRectangle::setAnchorPoint(Vector2f const &normalizedPoint, TimeDelta const &transition)
{
    d->normalizedAnchorX()->set(normalizedPoint.x, transition);
    d->normalizedAnchorY()->set(normalizedPoint.y, transition);
}

Rectanglef RuleRectangle::rect() const
{
    return Rectanglef(Vector2f(left().value(),  top().value()),
                      Vector2f(right().value(), bottom().value()));
}

Vector2f RuleRectangle::size() const
{
    return Vector2f(width().value(), height().value());
}

Vector2i RuleRectangle::sizei() const
{
    return Vector2i(width().valuei(), height().valuei());
}

Vector2ui RuleRectangle::sizeui() const
{
    return sizei().toVector2ui();
}

Rectanglei RuleRectangle::recti() const
{
    Rectanglef const r = rect();
    return Rectanglei(Vector2i(de::floor(r.topLeft.x),     de::floor(r.topLeft.y)),
                      Vector2i(de::floor(r.bottomRight.x), de::floor(r.bottomRight.y)));
}

void RuleRectangle::setDebugName(String const &name)
{
    d->debugName = name;
}

String RuleRectangle::description() const
{
    String desc = QString("RuleRectangle '%1'").arg(d->debugName);

    for (int i = 0; i < int(Rule::MAX_SEMANTICS); ++i)
    {
        desc += String("\n - ") +
                (i == Rule::Left? "Left" :
                 i == Rule::Top? "Top" :
                 i == Rule::Right? "Right" :
                 i == Rule::Bottom? "Bottom" :
                 i == Rule::Width? "Width" :
                 i == Rule::Height? "Height" :
                 i == Rule::AnchorX? "AnchorX" : "AnchorY") + ": ";

        if (d->inputRules[i])
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
