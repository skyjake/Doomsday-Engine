/** @file margins.cpp
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/ui/Margins"
#include "de/Style"

#include <de/IndirectRule>
#include <de/OperatorRule>

namespace de {
namespace ui {

enum Side
{
    SideLeft,
    SideRight,
    SideTop,
    SideBottom,

    LeftRight,
    TopBottom,

    MAX_SIDES
};

DENG2_PIMPL(Margins)
{
    Rule const *inputs[4];
    IndirectRule *outputs[MAX_SIDES];

    Instance(Public *i, DotPath const &defaultId) : Base(i)
    {
        zap(inputs);
        zap(outputs);

        for(int i = 0; i < 4; ++i)
        {
            setInput(i, defaultId);
        }
    }

    ~Instance()
    {
        for(int i = 0; i < 4; ++i)
        {
            releaseRef(inputs[i]);
        }
        for(int i = 0; i < int(MAX_SIDES); ++i)
        {
            if(outputs[i])
            {
                outputs[i]->unsetSource();
                releaseRef(outputs[i]);
            }
        }
    }

    void setInput(int side, DotPath const &styleId)
    {
        setInput(side, Style::get().rules().rule(styleId));
    }

    void setInput(int side, Rule const &rule)
    {
        DENG2_ASSERT(side >= 0 && side < 4);
        changeRef(inputs[side], rule);
        updateOutput(side);

        DENG2_FOR_AUDIENCE(Change, i)
        {
            i->marginsChanged();
        }
    }

    void updateOutput(int side)
    {
        if(side < 4 && outputs[side] && inputs[side])
        {
            outputs[side]->setSource(*inputs[side]);
        }

        // Update the sums.
        if(side == LeftRight || side == SideLeft || side == SideRight)
        {
            if(outputs[LeftRight] && inputs[SideLeft] && inputs[SideRight])
            {
                outputs[LeftRight]->setSource(*inputs[SideLeft] + *inputs[SideRight]);
            }
        }
        else if(side == TopBottom || side == SideTop || side == SideBottom)
        {
            if(outputs[TopBottom] && inputs[SideTop] && inputs[SideBottom])
            {
                outputs[TopBottom]->setSource(*inputs[SideTop] + *inputs[SideBottom]);
            }
        }
    }

    Rule const &getOutput(int side)
    {
        if(!outputs[side])
        {
            outputs[side] = new IndirectRule;
            updateOutput(side);
        }
        return *outputs[side];
    }

    DENG2_PIMPL_AUDIENCE(Change)
};

DENG2_AUDIENCE_METHOD(Margins, Change)

Margins::Margins(String const &defaultMargin) : d(new Instance(this, defaultMargin))
{}

Margins &Margins::set(Direction dir, DotPath const &marginId)
{
    d->setInput(dir == Left?  SideLeft  :
                dir == Right? SideRight :
                dir == Up?    SideTop   : SideBottom, marginId);
    return *this;
}

Margins &Margins::set(DotPath const &marginId)
{
    set(Left,  marginId);
    set(Right, marginId);
    set(Up,    marginId);
    set(Down,  marginId);
    return *this;
}

Margins &Margins::setLeft(DotPath const &leftMarginId)
{
    return set(ui::Left, leftMarginId);
}

Margins &Margins::setRight(DotPath const &rightMarginId)
{
    return set(ui::Right, rightMarginId);
}

Margins &Margins::setTop(DotPath const &topMarginId)
{
    return set(ui::Up, topMarginId);
}

Margins &Margins::setBottom(DotPath const &bottomMarginId)
{
    return set(ui::Down, bottomMarginId);
}

Margins &Margins::set(Direction dir, Rule const &rule)
{
    d->setInput(dir == Left?  SideLeft  :
                dir == Right? SideRight :
                dir == Up?    SideTop   : SideBottom, rule);
    return *this;
}

Margins &Margins::set(Rule const &rule)
{
    set(Left,  rule);
    set(Right, rule);
    set(Up,    rule);
    set(Down,  rule);
    return *this;
}

Margins &Margins::setAll(Margins const &margins)
{
    if(this == &margins) return *this;

    set(Left,  margins.left());
    set(Right, margins.right());
    set(Up,    margins.top());
    set(Down,  margins.bottom());
    return *this;
}

Margins &Margins::setLeft(Rule const &rule)
{
    return set(ui::Left, rule);
}

Margins &Margins::setRight(Rule const &rule)
{
    return set(ui::Right, rule);
}

Margins &Margins::setTop(Rule const &rule)
{
    return set(ui::Up, rule);
}

Margins &Margins::setBottom(Rule const &rule)
{
    return set(ui::Down, rule);
}

Rule const &Margins::left() const
{
    return d->getOutput(SideLeft);
}

Rule const &Margins::right() const
{
    return d->getOutput(SideRight);
}

Rule const &Margins::top() const
{
    return d->getOutput(SideTop);
}

Rule const &Margins::bottom() const
{
    return d->getOutput(SideBottom);
}

Rule const &Margins::width() const
{
    return d->getOutput(LeftRight);
}

Rule const &Margins::height() const
{
    return d->getOutput(TopBottom);
}

Rule const &Margins::margin(Direction dir) const
{
    return d->getOutput(dir == Left?  SideLeft  :
                        dir == Right? SideRight :
                        dir == Up?    SideTop   : SideBottom);
}

Vector4i Margins::toVector() const
{
    return Vector4i(left().valuei(), top().valuei(), right().valuei(), bottom().valuei());
}

} // namespace ui
} // namespace de
