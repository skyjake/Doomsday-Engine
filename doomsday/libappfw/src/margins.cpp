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
        setInput(side, Style::appStyle().rules().rule(styleId));
    }

    void setInput(int side, Rule const &rule)
    {
        DENG2_ASSERT(side >= 0 && side < 4);
        changeRef(inputs[side], rule);
        updateOutput(side);

        DENG2_FOR_PUBLIC_AUDIENCE(Change, i)
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
};

Margins::Margins(String const &defaultMargin) : d(new Instance(this, defaultMargin))
{}

void Margins::set(Direction dir, DotPath const &marginId)
{
    d->setInput(dir == Left?  SideLeft  :
                dir == Right? SideRight :
                dir == Up?    SideTop   : SideBottom, marginId);
}

void Margins::set(DotPath const &marginId)
{
    set(Left,  marginId);
    set(Right, marginId);
    set(Up,    marginId);
    set(Down,  marginId);
}

void Margins::setLeft(DotPath const &leftMarginId)
{
    set(ui::Left, leftMarginId);
}

void Margins::setRight(DotPath const &rightMarginId)
{
    set(ui::Right, rightMarginId);
}

void Margins::setTop(DotPath const &topMarginId)
{
    set(ui::Up, topMarginId);
}

void Margins::setBottom(DotPath const &bottomMarginId)
{
    set(ui::Down, bottomMarginId);
}

void Margins::set(Direction dir, Rule const &rule)
{
    d->setInput(dir == Left?  SideLeft  :
                dir == Right? SideRight :
                dir == Up?    SideTop   : SideBottom, rule);
}

void Margins::set(Rule const &rule)
{
    set(Left,  rule);
    set(Right, rule);
    set(Up,    rule);
    set(Down,  rule);
}

void Margins::setAll(Margins const &margins)
{
    if(this == &margins) return;

    set(Left,  margins.left());
    set(Right, margins.right());
    set(Up,    margins.top());
    set(Down,  margins.bottom());
}

void Margins::setLeft(Rule const &rule)
{
    set(ui::Left, rule);
}

void Margins::setRight(Rule const &rule)
{
    set(ui::Right, rule);
}

void Margins::setTop(Rule const &rule)
{
    set(ui::Up, rule);
}

void Margins::setBottom(Rule const &rule)
{
    set(ui::Down, rule);
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
